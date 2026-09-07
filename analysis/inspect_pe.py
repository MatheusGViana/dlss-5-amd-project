import sys, pathlib, re, json, hashlib
sys.path.insert(0,str(pathlib.Path('.analysis-tools').resolve()))
import pefile
p=pathlib.Path('version.dll'); b=p.read_bytes(); pe=pefile.PE(data=b)
exports=[{'name':e.name.decode() if e.name else None,'ordinal':e.ordinal,'rva':hex(e.address)} for e in pe.DIRECTORY_ENTRY_EXPORT.symbols]
imports={e.dll.decode():[i.name.decode() if i.name else str(i.ordinal) for i in e.imports] for e in pe.DIRECTORY_ENTRY_IMPORT}
strings=[{'offset':hex(m.start()),'text':m.group().decode('ascii')} for m in re.finditer(rb'[\x20-\x7e]{6,}',b)]
selected=[s for s in strings if re.search(r'dlss|fsr|ffx|\.pdb|\.ini|\.bin|\.safetensors|hip|render.?size|upscal|pre.?sr|post.?sr',s['text'],re.I)]
r={'sha256':hashlib.sha256(b).hexdigest(),'machine':hex(pe.FILE_HEADER.Machine),'image_base':hex(pe.OPTIONAL_HEADER.ImageBase),'exports':exports,'imports':imports,'sections':[{'name':s.Name.rstrip(b'\0').decode(),'rva':hex(s.VirtualAddress),'raw_size':s.SizeOfRawData} for s in pe.sections],'strings':selected}
pathlib.Path('analysis/version-pe.json').write_text(json.dumps(r,indent=2),encoding='utf-8')
pathlib.Path('analysis/version-strings.txt').write_text('\n'.join(s['offset']+' '+s['text'] for s in strings),encoding='utf-8')
print(json.dumps({k:r[k] for k in ['sha256','exports','sections']},indent=2))
print('\n'.join(s['offset']+' '+s['text'] for s in selected if not s['text'].startswith(('.text','_Z')) )[:20000])
