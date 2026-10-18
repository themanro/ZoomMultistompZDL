import ctypes,json,subprocess,sys,tempfile,unittest
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2]
sys.path.insert(0,str(ROOT/'build'))
from selector_metadata import pedal_label,SELECTORS,pedal_label_c

class Selectors(unittest.TestCase):
 def test_complete_ranges_and_safe_targets(self):
  db=json.loads((ROOT/'tools/effects_db.json').read_text())
  for e in db['custom']+db['stock']:
   for p in e['params']:
    if 'choices' not in p:continue
    cs=p['choices'];self.assertEqual([v for c in cs for v in range(c['from'],c['to']+1)],list(range(p['max']+1)))
    for c in cs:self.assertTrue(c['from']<=c['value']<=c['to'])
 def test_actual_c_label_callbacks_all_raw_values(self):
  # Exercise the generated C ABI on host, including its NUL termination.
  for name,ps in SELECTORS.items():
   m={'effect_name':name,'params':[{'name':p,'max':cs[-1]['to']} for p,cs in ps.items()]}
   with tempfile.TemporaryDirectory() as td:
    c=Path(td)/'labels.c';lib=Path(td)/'labels.dylib';c.write_text(pedal_label_c(m))
    subprocess.run(['cc','-shared','-fPIC',str(c),'-o',str(lib)],check=True,capture_output=True)
    dll=ctypes.CDLL(str(lib))
    for i,cs in enumerate(ps.values()):
     fn=getattr(dll,f'ZDL_GetLabel_{i}');fn.argtypes=[ctypes.c_uint,ctypes.c_void_p];fn.restype=ctypes.c_int
     for choice in cs:
      for raw in range(choice['from'],choice['to']+1):
       buf=ctypes.create_string_buffer(b'XXXXXXXXZ',10);n=fn(raw,buf)
       self.assertEqual(buf.value.decode(),pedal_label(choice['label']));self.assertEqual(n,len(pedal_label(choice['label'])));self.assertEqual(buf.raw[8],ord('Z'))
 def test_shallow_water_control_names_and_noise_toggle(self):
  db=json.loads((ROOT/'tools/effects_db.json').read_text())
  effect=next(e for e in db['custom'] if e['name']=='Shallow')
  self.assertEqual([p['name'] for p in effect['params']],['Rate','Depth','Sensitivity','Noise','Mix'])
  self.assertEqual([c['label'] for c in effect['params'][3]['choices']],['Off','On'])
 def test_rainbow_machine_controls_and_short_pedal_label(self):
  db=json.loads((ROOT/'tools/effects_db.json').read_text())
  effect=next(e for e in db['custom'] if e['name']=='Rainbow')
  self.assertEqual([p['name'] for p in effect['params']],
                   ['Pitch','Primary','Tracking','Tone','Secondary','Magic'])
  self.assertEqual(effect['params'][4]['name'],'Secondary')
  self.assertEqual(__import__('parameter_display').pedal_name('Rainbow','Secondary'),'Second')
 def test_spacerev_expression_lfo_controls(self):
  db=json.loads((ROOT/'tools/effects_db.json').read_text())
  effect=next(e for e in db['custom'] if e['name']=='SpaceRev')
  self.assertEqual([p['name'] for p in effect['params']],
                   ['Expression','Rate','Depth','Mix'])
  self.assertEqual(__import__('parameter_display').pedal_name('SpaceRev','Expression'),'Expr')
 def test_space2_fifth_shimmer_controls(self):
  db=json.loads((ROOT/'tools/effects_db.json').read_text())
  effect=next(e for e in db['custom'] if e['name']=='Space2')
  self.assertEqual([p['name'] for p in effect['params']],['Fifth','Decay','Tone','Mix'])
 def test_corrupt_pll_controls_and_pitch_selectors(self):
  db=json.loads((ROOT/'tools/effects_db.json').read_text())
  effect=next(e for e in db['custom'] if e['name']=='Corrupt')
  self.assertEqual([p['name'] for p in effect['params']],
                   ['Master','Sub','Harm','Rate','Depth','Mix'])
  self.assertEqual([c['label'] for c in effect['params'][0]['choices']],
                   ['Unis','-1Oct','-2Oct'])
  self.assertEqual([c['label'] for c in effect['params'][2]['choices']],
                   ['Unis','5th','Oct','4th','-Oct','12th'])
 def test_particle_granular_modes_and_controls(self):
  db=json.loads((ROOT/'tools/effects_db.json').read_text())
  effect=next(e for e in db['custom'] if e['name']=='Particle')
  self.assertEqual([p['name'] for p in effect['params']],
                   ['Mode','Size','Density','Delay','Feedback','Mix'])
  self.assertEqual([c['label'] for c in effect['params'][0]['choices']],
                   ['Rand','Dens','LFO','RPit','Rev','Det','DPit','LPit'])
 def test_rooms6_matches_actual_dsp_selector(self):
  src=(ROOT/'src/custom/rooms/rooms.c').read_text()
  start=src.index('#ifdef ROOMS_DISCRETE_MODE')
  snippet=src[start:src.index('#endif',start)+len('#endif')]
  with tempfile.TemporaryDirectory() as td:
   for discrete in (False,True):
    c=Path(td)/'mode.c';lib=Path(td)/('mode'+str(discrete)+'.dylib')
    c.write_text(('#define ROOMS_DISCRETE_MODE 1\n' if discrete else '')+
       'int mode_for(int raw) { float modeN=(float)raw*0.01f;\n'+snippet+'\nreturn mode;}')
    subprocess.run(['cc','-shared','-fPIC',str(c),'-o',str(lib)],check=True,capture_output=True)
    dll=ctypes.CDLL(str(lib));fn=dll.mode_for;fn.argtypes=[ctypes.c_int];fn.restype=ctypes.c_int
    cs=SELECTORS['Rooms6' if discrete else 'Rooms']['Mode']
    for i,choice in enumerate(cs):
     for raw in range(choice['from'],choice['to']+1):self.assertEqual(fn(raw),i)
if __name__=='__main__':unittest.main()
