"""Create private backends without modifying the supplied version.dll."""
import hashlib, json, pathlib, sys, re
sys.path.insert(0, str(pathlib.Path(__file__).resolve().parents[1] / '.analysis-tools'))
import pefile
root = pathlib.Path(__file__).resolve().parents[1]
original = (root / 'version.dll').read_bytes()
expected = '106223723fd9266c44d38dc2fb77933948ab37803f46bfcea2bae3a0a474ac84'
assert hashlib.sha256(original).hexdigest() == expected, 'Unsupported original runtime'
pe = pefile.PE(data=original)
patched = bytearray(original)
changes = []
# Keep CRT/HIP initializers and DllMain setup. Disable only the standalone
# thread which installs game hooks. OptiScaler supplies device + queue + weights.
# Queue notification retains its native atomics/fences but must not resubmit work.
for rva, before, after, reason in [
    (0x2bfc, 'ff15d6910600', '31c090909090', 'disable standalone hook installer thread'),
    (0x4653, 'ff15e7280700', '909090909090', 'notification after actual ExecuteCommandLists; do not execute twice'),
]:
    off = pe.get_offset_from_rva(rva)
    old, new = bytes.fromhex(before), bytes.fromhex(after)
    assert original[off:off+len(old)] == old, f'Unexpected instructions at {rva:x}'
    patched[off:off+len(old)] = new
    changes.append(dict(rva=hex(rva), offset=hex(off), before=before, after=after, reason=reason))
# The embedded shader is compiled by the runtime. On timeout it must leave
# the current colour untouched rather than applying an unwarped old residual.
for old, replacement, reason in [
    (b'if (flags.Load(12) != 0) d = prev[id.xy].rgb;',
     b'if (flags.Load(12) != 0) return;', 'timeout keeps current input instead of stale residual'),
    (b'previous residual shown', b'current input kept', 'report the new timeout fallback accurately'),
]:
    assert original.count(old) == 1, 'Unexpected embedded shader/log text'
    assert len(replacement) <= len(old)
    off = original.index(old)
    new = replacement.ljust(len(old), b' ')
    patched[off:off+len(old)] = new
    changes.append(dict(offset=hex(off), before=old.hex(), after=new.hex(), reason=reason))
# Bound the GPU spin even if submission notification is delayed or lost.
# Preserve string size because the binary supplies a fixed compilation length.
prefix = b'\ngloballycoherent RWByteAddressBuffer flags : register(u0);'
assert original.count(prefix) == 1
off = original.index(prefix)
end = original.index(b'\0', off)
old = original[off:end]
assert old.count(b'i < maxIter') == 1
new = re.sub(rb'//[^\n]*', b'', old).replace(b'i < maxIter', b'i < min(maxIter, 2097152u)')
assert len(new) <= len(old)
new = new.ljust(len(old), b' ')
patched[off:end] = new
changes.append(dict(offset=hex(off), before=old.hex(), after=new.hex(), reason='bound GPU wait shader to 2097152 iterations'))
out = root / 'package-amd-presr'
out.mkdir(exist_ok=True)
for i in range(1, 4):
    (out / f'dlssnr_amd_pass{i}.dll').write_bytes(patched)
sha = hashlib.sha256(patched).hexdigest()
(root/'analysis/runtime-patches.json').write_text(json.dumps(dict(original_sha256=expected,patched_sha256=sha,changes=changes),indent=2))
header = root/'OptiScaler-DLSSNR-PreSR-Multipass-main/OptiScaler/dlssnr/amd/RuntimeHash.h'
header.parent.mkdir(parents=True,exist_ok=True)
header.write_text('#pragma once\ninline constexpr unsigned char AmdRuntimeSha256[32] = {' + ','.join('0x'+sha[i:i+2] for i in range(0,64,2)) + '};\n')
print(sha)
