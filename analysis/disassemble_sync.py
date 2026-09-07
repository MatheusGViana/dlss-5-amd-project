exec(open('analysis/disassemble_hooks.py').read().split('targets={}')[0])
funcs=[(base+e.struct.BeginAddress,base+e.struct.EndAddress) for e in pe.DIRECTORY_ENTRY_EXCEPTION]
lines=[]
for rva in [0x4640,0xc9e0,0xc7d0]:
 a,b=next((a,b) for a,b in funcs if a==base+rva)
 lines.append('\n; FUNCTION RVA '+hex(rva))
 for i in ins:
  if a<=i.address<b:lines.append(f'{i.address-base:08x} {i.mnemonic:8} {i.op_str}')
pathlib.Path('analysis/synchronization-disassembly.txt').write_text('\n'.join(lines),encoding='utf-8')
