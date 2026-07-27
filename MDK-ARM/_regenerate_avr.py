"""One-shot regenerate avrDeviceConst.c from XML"""
import xml.etree.ElementTree as ET, re, os

XML = r'E:\Codex\Project\Keil\Reference_Local\STM32F103VET6_Programmer_V1\PROGRAMMER\avrdude-avr-init.xml'
OUT = r'E:\Codex\Project\Keil\Reference_Local\STM32F103VET6_Programmer_V1\PROGRAMMER\avrDeviceConst.c'

tree = ET.parse(XML); root = tree.getroot(); parts = root.findall('part')

def rf(p, f):
    while p is not None:
        v = p.find(f)
        if v is not None and v.text:
            t = v.text.strip()
            if t and t != 'NULL': return t
        pid = p.get('parent'); p = None
        if pid:
            for p2 in parts:
                if p2.get('id') == pid: p = p2; break
    return None

def rs(p):
    s = rf(p, 'signature')
    if s:
        n = re.findall(r'0x([0-9a-fA-F]+)', s)
        if len(n) >= 3: return (int(n[0], 16), int(n[1], 16), int(n[2], 16))
    return (0, 0, 0)

def ri(p, f, d=0):
    s = rf(p, f); return int(s) if s else d

# Build entries from XML
entries = []
for p in parts:
    desc = p.get('desc', '')
    s = rs(p); fs = 0; es = 0; fps = 0; fc = 0
    mems = p.find('memories')
    if mems is not None:
        for m in mems.findall('memory'):
            t = m.get('type'); sz = m.find('size')
            if t == 'flash':
                if sz is not None: fs = int(sz.text)
                ps = m.find('page_size')
                if ps is not None: fps = int(ps.text)
            elif t == 'eeprom':
                if sz is not None: es = int(sz.text)
            elif t in ('lfuse', 'hfuse', 'efuse'): fc += 1
    entries.append({
        'name': desc, 'sig': s, 'fs': fs, 'es': es,
        'fps': fps, 'fc': fc,
        'ced': ri(p, 'chip_erase_delay', 0),
        'to': ri(p, 'timeout', 200), 'sd': ri(p, 'stabdelay', 100),
        'ce': ri(p, 'cmdexedelay', 25), 'sl': ri(p, 'synchloops', 32),
        'pv': ri(p, 'pollvalue', 83),
        's5': ri(p, 'stk500_devcode', 0),
        'a9': ri(p, 'avr910_devcode', 0),
    })

print(f'{len(entries)} entries')

# Build C text line by line
L = []
L.append('/* Auto-generated avrDeviceConst.c from avrdude-avr-init.xml */')
L.append('#include "avrDeviceConst.h"')
L.append('#include <string.h>')
L.append('')

L.append(f'/* Name table ({len(entries)} names) */')
L.append('static const char g_avrNameTable[][16] = {')
for e in entries:
    L.append(f'    "{e["name"]}",')
L.append('};')
L.append(f'#define AVR_NAME_COUNT {len(entries)}')
L.append('')

# OP group
L.append('/* Shared OP group */')
L.append('static const AVR_OpGroup g_avrOpGroups[1] = {{')
for cmd in [(0xAC,0x53,0x00,0x00,0),(0xAC,0x80,0x00,0x00,0),(0x20,0x00,0x00,0x00,3),
             (0x20,0x00,0x00,0x00,3),(0x28,0x00,0x00,0x00,3),(0x40,0x00,0x00,0x00,0),
             (0x40,0x00,0x00,0x00,0),(0x48,0x00,0x00,0x00,0),(0x40,0x00,0x00,0x00,0),
             (0x48,0x00,0x00,0x00,0),(0x4D,0x00,0x00,0x00,0)]:
    L.append(f'    {{{cmd[0]},{cmd[1]},{cmd[2]},{cmd[3]},{cmd[4]}}},')
L.append('    }}')
L.append('};')
L.append('')

# MEM group
L.append('static const AVR_MemGroup g_avrMemGroups[] = {{{')
for m in [(0,0,0,10,0,0x41,0xFF,0xFF),(0,0,0,20,0,0x41,0xFF,0xFF),
           (1,0,1,0,0,0x00,0x00,0x00),(1,0,1,0,0,0x00,0x00,0x00),
           (1,0,1,0,0,0x00,0x00,0x00),(1,0,1,0,0,0x00,0x00,0x00),
           (3,0,1,0,0,0x00,0x00,0x00),(1,0,1,0,0,0x00,0x00,0x00)]:
    L.append(f'    {{{m[0]},{m[1]},{m[2]},{m[3]},{m[4]},0x{m[5]:02X},{{0x{m[6]:02X},0x{m[7]:02X}}}}},')
L.append('    }},')
L.append('};')
L.append('#define AVR_MEM_SHARED_COUNT 1')
L.append('')

# Device table
L.append(f'/* Device entries ({len(entries)} total) */')
L.append('static const AVR_DeviceEntry g_avrDeviceTable[] = {')
for i, e in enumerate(entries):
    s = e['sig']
    fp_msb = (e['fps'] >> 8) & 0xFF
    fp_lsb = e['fps'] & 0xFF
    L.append(f'  {{{i},{{{s[0]},{s[1]},{s[2]}}},0,{fp_msb},{e["fs"]}UL,{e["es"]},{e["ced"]},{e["fc"]},{e["to"]},{e["sd"]},{e["ce"]},{e["sl"]},{e["pv"]},3,{fp_lsb},{e["s5"]},{e["a9"]},0}},')
L.append('};')
L.append(f'#define AVR_DEVICE_COUNT {len(entries)}')
L.append('')

# API functions (compact to minimize file size)
L.append('''/* API functions */
const AVR_OpGroup* avr_get_op_group(uint8_t idx) { (void)idx; return &g_avrOpGroups[0]; }
const AVR_MemGroup* avr_get_mem_group(uint8_t idx) { return (idx<1)?&g_avrMemGroups[idx]:&g_avrMemGroups[0]; }
const AVR_DeviceEntry* avr_get_device_entry(uint16_t i) { return (i<AVR_DEVICE_COUNT)?&g_avrDeviceTable[i]:0; }
const char* avr_get_device_name(uint16_t i) {
  if(i>=AVR_DEVICE_COUNT) return 0;
  uint16_t ni=g_avrDeviceTable[i].name_idx;
  if(ni>=AVR_NAME_COUNT) return 0;
  return g_avrNameTable[ni]; }
uint16_t avr_get_device_count(void) { return AVR_DEVICE_COUNT; }
int16_t avr_find_device_by_signature(const uint8_t s[3]) {
  uint16_t i; for(i=0;i<AVR_DEVICE_COUNT;i++) {
    const AVR_DeviceEntry*e=&g_avrDeviceTable[i];
    if(e->signature[0]==s[0]&&e->signature[1]==s[1]&&e->signature[2]==s[2]) return i; } return -1; }
int16_t avr_find_device_by_name(const char* n) {
  if(!n) return -1;
  uint16_t i; for(i=0;i<AVR_DEVICE_COUNT;i++) {
    const char* dn=avr_get_device_name(i); if(dn&&strcasecmp(dn,n)==0) return i; } return -1; }
int avrAggregateParams(const AVR_DeviceEntry* e, AVRPART* out) {
  if(!e||!out) return -1; memset(out,0,sizeof(*out));
  out->desc=g_avrNameTable[e->name_idx]; out->id=g_avrNameTable[e->name_idx];
  out->signature[0]=e->signature[0]; out->signature[1]=e->signature[1]; out->signature[2]=e->signature[2];
  out->stk500_devcode=e->stk500_devcode; out->avr910_devcode=e->avr910_devcode;
  out->timeout=e->timeout; out->stabdelay=e->stabdelay; out->cmdexedelay=e->cmdexedelay;
  out->synchloops=e->synchloops; out->bytedelay=e->bytedelay;
  out->pollvalue=e->pollvalue; out->pollindex=e->pollindex;
  out->chip_erase_delay=e->chip_erase_delay;
  out->flash_page_size=((uint16_t)e->flash_page_size_msb<<8)|e->flash_page_size_lsb;
  out->flash_size=e->flash_size; out->eeprom_size=e->eeprom_size; out->fuse_count=e->fuse_count;
  const AVR_MemGroup* mg=avr_get_mem_group(0); const AVR_OpGroup* og=avr_get_op_group(0);
  if(mg) { uint8_t i; const char* mn[8]={"flash","eeprom","lfuse","hfuse","efuse","lock","signature","calibration"};
    for(i=0;i<AVR_MEM_MAX;i++) { out->mem[i].desc=mn[i]; out->mem[i].mem_index=i;
      out->mem[i].size=mg->mem[i].size; out->mem[i].page_size=mg->mem[i].page_size;
      out->mem[i].readsize=mg->mem[i].readsize; out->mem[i].delay=mg->mem[i].delay;
      out->mem[i].flags=mg->mem[i].flags; out->mem[i].paged=(mg->mem[i].page_size>1)?1:0; }
    out->mem[0].size=out->flash_size; out->mem[0].page_size=out->flash_page_size;
    out->mem[0].paged=(out->flash_page_size>1)?1:0; out->mem[1].size=out->eeprom_size; }
  if(og) memcpy(out->op,og->op,sizeof(out->op)); return 0; }
int avrFindDeviceByName(const char* n, avr_prog_params_t* out) {
  int16_t idx=avr_find_device_by_name(n); if(idx<0) return -1;
  return avrAggregateParams(&g_avrDeviceTable[idx],out); }
int avrFindDeviceByIndex(uint16_t idx, avr_prog_params_t* out) {
  if(out==NULL||idx>=AVR_DEVICE_COUNT) return -1;
  return avrAggregateParams(&g_avrDeviceTable[idx],out); }
void avr_get_full_params(const AVR_DeviceEntry* e, void* out, uint16_t sz) {
  if(!e||!out||sz<160) return; avrAggregateParams(e,(AVRPART*)out); }
''')

with open(OUT, 'w', encoding='utf-8') as f:
    f.write('\n'.join(L))

print(f'Written {os.path.getsize(OUT)/1024:.1f} KB')
# Verify
for i, e in enumerate(entries[:5]):
    print(f'  [{i}] {e["name"][:20]:20s} sig=0x{e["sig"][0]:02X}{e["sig"][1]:02X}{e["sig"][2]:02X} fl={e["fs"]}')
for i, e in enumerate(entries):
    if 'm328' in e['name'].lower() or 'ATmega328' in e['name']:
        print(f'  {e["name"]} sig=0x{e["sig"][0]:02X}{e["sig"][1]:02X}{e["sig"][2]:02X} fl={e["fs"]} ee={e["es"]}')