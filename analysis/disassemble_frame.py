exec(open('analysis/disassemble_hooks.py').read().split('targets={}')[0])
funcs=[(base+e.struct.BeginAddress,base+e.struct.EndAddress) for e in pe.DIRECTORY_ENTRY_EXCEPTION]
a,b=next((a,b) for a,b in funcs if a==base+0xa0b0)
lines=[]
for i in ins:
 if not a<=i.address<b: continue
 notes=[]
 for o in i.operands:
  if o.type==X86_OP_MEM and o.mem.base==X86_REG_RIP:
   t=i.address+i.size+o.mem.disp
   try:
    data=pe.get_data(t-base,180).split(b'\0')[0]
    if len(data)>5 and all(32<=v<127 for v in data):notes.append(data.decode())
   except Exception:pass
 lines.append(f'{i.address-base:08x} {i.mnemonic:8} {i.op_str}'+(' ; '+' | '.join(notes) if notes else ''))
pathlib.Path('analysis/process-frame-disassembly.txt').write_text('\n'.join(lines),encoding='utf-8')
print(hex(a-base),hex(b-base),len(lines))
