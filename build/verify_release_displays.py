"""Validate native art, short headings and runtime mode label relocations."""
import hashlib,json,struct
from pathlib import Path
from decode_picture import decode_picture,read_image_info
from extract_effect_db import parse_zdl
from gen_init_materialize import _read_elf_sections
from zdl import Zdl
from selector_metadata import SELECTORS,pedal_label
from parameter_display import pedal_name
ROOT=Path(__file__).resolve().parents[1]
def verify_string_relocations(blob, sections, choices):
    # A descriptor callback pointer alone is insufficient: every string's
    # address materialization must also be relocated by the pedal loader.
    t=sections['.text']; c=sections['.const']; r=sections['.rela.dyn']; sy=sections['.dynsym']
    targets={}
    for j in range(r['offset'],r['offset']+r['size'],12):
        va,info,addend=struct.unpack_from('<IIi',blob,j)
        if info&255 not in (9,10) or not t['addr']<=va<t['addr']+t['size']:continue
        symidx=info>>8
        assert symidx>0, 'unresolved dynamic relocation'
        target=struct.unpack_from('<I',blob,sy['offset']+symidx*16+4)[0]+addend
        if c['addr']<=target<c['addr']+c['size']:
            off=c['offset']+target-c['addr']
            label=blob[off:off+8].split(b'\0')[0].decode('ascii',errors='replace')
            targets.setdefault(label,set()).add(info&255)
    for label in {pedal_label(g['label']) for cs in choices.values() for g in cs}:
        assert targets.get(label)=={9,10}, f'{label}: missing runtime string address relocation'

def verify():
 rows=[];pictures=set()
 # The published effects (dist/index.json), not every .ZDL there: dist/ also holds
 # a user's own NAM captures and cabs, which are not releases.
 published=[ROOT/'dist'/e['file'] for e in json.loads((ROOT/'dist'/'index.json').read_text())['effects']]
 for path in published:
  e=parse_zdl(path);b=Zdl.load(path).elf;s=_read_elf_sections(b);cs=s['.const'];c=b[cs['offset']:cs['offset']+cs['size']];desc=c.index(b'OnOff\0')
  for i,p in enumerate(e['params']):
   entry=c[desc+(i+2)*48:desc+(i+3)*48];heading=entry[:12].split(b'\0')[0].decode()
   assert heading==pedal_name(e['name'],p['name']), (path,heading)
   assert len(heading)<=5,(path,heading)
  if e['name'] in SELECTORS:verify_string_relocations(b,s,SELECTORS[e['name']])
  px,_=decode_picture(str(path));assert len(px)==64 and all(len(r)==128 for r in px)
  imageinfo=read_image_info(str(path))
  assert imageinfo['width']==128 and imageinfo['height']==64
  for _,x,y in imageinfo['knobs']:assert 0<=x<=108 and 0<=y<=49
  digest=hashlib.sha256(bytes(v for r in px for v in r)).hexdigest();assert digest not in pictures
  pictures.add(digest);rows.append({'effect':e['name'],'headings':'<=5 chars','cover':'128x64','labels':list(SELECTORS.get(e['name'],{}))})
 assert len(rows)==len(published)
 return rows
if __name__=='__main__':print(json.dumps(verify(),indent=2))
