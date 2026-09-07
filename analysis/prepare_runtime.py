"""Create private backends without modifying the supplied version.dll."""
import hashlib, json, pathlib, sys
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
