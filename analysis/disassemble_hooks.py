import sys,pathlib,bisect
sys.path.insert(0,str(pathlib.Path('.analysis-tools').resolve()))
import pefile,capstone
from capstone.x86 import X86_OP_MEM,X86_REG_RIP
pe=pefile.PE('version.dll'); base=pe.OPTIONAL_HEADER.ImageBase
sec=next(s for s in pe.sections if s.Name.startswith(b'.text'))
md=capstone.Cs(capstone.CS_ARCH_X86,capstone.CS_MODE_64);md.detail=True
ins=list(md.disasm(sec.get_data(),base+sec.VirtualAddress))
targets={}
for label in [b'first ffxDispatch type',b'first ffxFsr3UpscalerContextDispatch',b'first ffxFsr3ContextDispatchUpscale',b'UseFsrInputs',b'InlineWaitMs']:
 off=pe.__data__.find(label)
 if off>=0: targets[base+pe.get_rva_from_offset(off)]=label.decode()
funcs=[(base+e.struct.BeginAddress,base+e.struct.EndAddress) for e in pe.DIRECTORY_ENTRY_EXCEPTION]
found={}
for i in ins:
 for o in i.operands:
  if o.type==X86_OP_MEM and o.mem.base==X86_REG_RIP:
   t=i.address+i.size+o.mem.disp
   if t in targets:
    f=next(((a,b) for a,b in funcs if a<=i.address<b),None)
    print(targets[t],hex(i.address-base),'function', tuple(hex(x-base) for x in f) if f else None)
    if f: found[f]=targets[t]
lines=[]
for (a,b),label in found.items():
 lines.append('\n; '+label+' RVA '+hex(a-base))
 lines.extend(f'{i.address-base:08x}  {i.mnemonic:9} {i.op_str}' for i in ins if a<=i.address<b)
pathlib.Path('analysis/hook-disassembly.txt').write_text('\n'.join(lines),encoding='utf-8')
