exec(open('analysis/disassemble_hooks.py').read().split('targets={}')[0])
for i in ins:
 if not base+0x3940<=i.address<base+0x3e6a: continue
 notes=[]
 for o in i.operands:
  if o.type==X86_OP_MEM and o.mem.base==X86_REG_RIP:
   t=i.address+i.size+o.mem.disp
   try:
    d=pe.get_data(t-base,80).split(b'\0')[0]
    if len(d)>3 and all(32<=v<127 for v in d):notes.append(d.decode())
   except Exception: pass
 print(f'{i.address-base:08x} {i.mnemonic:8} {i.op_str}'+(' ; '+' | '.join(notes) if notes else ''))
