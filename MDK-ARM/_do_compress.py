#!/usr/bin/env python3
import re, os

path = r'E:\Codex\Project\Keil\Reference_Local\STM32F103VET6_Programmer_V1\PROGRAMMER\avrDeviceConst.c'

with open(path, 'r', encoding='utf-8', errors='ignore') as f:
    text = f.read()

# Step 1: Parse names (before first "static const AVR_DeviceParam")
split_point = text.index('static const AVR_DeviceParam')
names_text = text[:split_point]
names = re.findall(r'"([A-Za-z0-9_]+)"', names_text)
names_uniq = list(dict.fromkeys(names))

print(f'Names: {len(names)} parsed')
print(f'Unique names: {len(names_uniq)}')

# Step 2: Parse device entries one by one (line-based to avoid regex timeout)
device_text = text[split_point:]
entries = []

# Find all device entries by looking for pattern ".id = NUMBER"
lines = device_text.split('\n')
current_entry = ''
in_entry = False
brace_depth = 0

for line in lines:
    if '.id = ' in line:
        # Start of a new entry
        if current_entry:
            entries.append(current_entry)
        current_entry = line
        in_entry = True
    elif in_entry:
        current_entry += '\n' + line
        brace_depth += line.count('{') - line.count('}')
        if brace_depth <= 0 and '},' in line:
            entries.append(current_entry)
            current_entry = ''
            in_entry = False
            brace_depth = 0

if current_entry:
    entries.append(current_entry)

print(f'Device entries: {len(entries)} parsed')

# Step 3: Extract fields from each entry
parsed = []
for block in entries:
    e = {}
    for field, pat in [
        ('id', r'\.id\s*=\s*(\d+)'),
        ('sig', r'\.signature\s*=\s*\{([^}]+)\}'),
        ('stk_dc', r'\.stk500_devcode\s*=\s*(\d+)'),
        ('avr_dc', r'\.avr910_devcode\s*=\s*(\d+)'),
        ('to', r'\.timeout\s*=\s*(\d+)'),
        ('stab', r'\.stabdelay\s*=\s*(\d+)'),
        ('ce', r'\.cmdexedelay\s*=\s*(\d+)'),
        ('sl', r'\.synchloops\s*=\s*(\d+)'),
        ('bd', r'\.bytedelay\s*=\s*(\d+)'),
        ('pv', r'\.pollvalue\s*=\s*(\d+)'),
        ('pi', r'\.pollindex\s*=\s*(\d+)'),
        ('ced', r'\.chip_erase_delay\s*=\s*(\d+)'),
        ('fps', r'\.flash_page_size\s*=\s*(\d+)'),
        ('fs', r'\.flash_size\s*=\s*(\d+)'),
        ('es', r'\.eeprom_size\s*=\s*(\d+)'),
        ('fc', r'\.fuse_count\s*=\s*(\d+)'),
    ]:
        m = re.search(pat, block)
        if m: e[field] = m.group(1)

    # Extract mem array
    mem = []
    mm = re.search(r'\.mem\s*=\s*\{(.*?)\}\s*,', block, re.DOTALL)
    if mm:
        for sub in re.finditer(r'\{([0-9,\s]+)\}', mm.group(1)):
            nums = [int(x) for x in re.findall(r'\d+', sub.group(1))]
            mem.append(nums[:5] + [0]*(5-len(nums)))

    # Extract op array
    ops = []
    om = re.search(r'\.op\s*=\s*\{(.*?)\}\s*,', block, re.DOTALL)
    if om:
        for sub in re.finditer(r'\{([0-9,\s]+)\}', om.group(1)):
            nums = [int(x) for x in re.findall(r'\d+', sub.group(1))]
            ops.append(nums[:5] + [0]*(5-len(nums)))

    e['mem'] = mem
    e['op'] = ops
    parsed.append(e)

print(f'Parsed entries: {len(parsed)}')

# Step 4: Find unique mem and op groups
import json
mem_groups = {}
op_groups = {}
for i, e in enumerate(parsed):
    msig = json.dumps(e.get('mem', []))
    osig = json.dumps(e.get('op', []))
    mem_groups.setdefault(msig, []).append(i)
    op_groups.setdefault(osig, []).append(i)

mem_keys_sorted = sorted(mem_groups.keys(), key=lambda x: -len(mem_groups[x]))
op_keys_sorted = sorted(op_groups.keys(), key=lambda x: -len(op_groups[x]))

mem_idx = {}
for idx, mk in enumerate(mem_keys_sorted):
    for d in mem_groups[mk]: mem_idx[d] = idx

print(f'unique MEM groups: {len(mem_groups)}')
print(f'unique OP groups: {len(op_groups)}')

# Step 5: Generate compressed C code
out_lines = []
out_lines.append('/* Auto-generated compressed avrDeviceConst.c */')
out_lines.append('#include "avrDeviceConst.h"')
out_lines.append('#include <string.h>')
out_lines.append('')

out_lines.append(f'/* Name table ({len(names_uniq)} unique names) */')
out_lines.append('static const char g_avrNameTable[][16] = {')
for n in names_uniq:
    out_lines.append(f'    "{n}",')
out_lines.append('};')
out_lines.append(f'#define AVR_NAME_COUNT {len(names_uniq)}')
out_lines.append('')

# OP group (all identical — just 1)
op_sample = json.loads(op_keys_sorted[0])
out_lines.append('/* Shared OP group (all AVR ISP devices share identical commands) */')
out_lines.append('static const AVR_OpGroup g_avrOpGroups[1] = {')
out_lines.append('    {{')
for cmd in op_sample:
    out_lines.append(f'        {{{cmd[0]},{cmd[1]},{cmd[2]},{cmd[3]},{cmd[4]}}},')
out_lines.append('    }}')
out_lines.append('};')
out_lines.append('')

# MEM groups
out_lines.append(f'/* Shared MEM groups ({len(mem_keys_sorted)} unique) */')
out_lines.append('static const AVR_MemGroup g_avrMemGroups[] = {')
for mk in mem_keys_sorted:
    mg = json.loads(mk)
    out_lines.append('    {{')
    for m in mg:
        out_lines.append(f'        {{{m[0]},{m[1]},{m[2]},{m[3]},{m[4]},{{0,0}}}},')
    out_lines.append('    }},')
out_lines.append('};')
out_lines.append('')

# Device entries
out_lines.append(f'/* Device entries ({len(parsed)} total) */')
out_lines.append('static const AVR_DeviceEntry g_avrDeviceTable[] = {')
for i, e in enumerate(parsed):
    name_idx = names_uniq.index(names[i]) if i < len(names) else 0
    sig = [int(x) for x in re.findall(r'\d+', e.get('sig', '0,0,0'))[:3]]
    fp = int(e.get('fps', '0'))
    out_lines.append(
        f'  {{ {name_idx}, {{{sig[0]},{sig[1]},{sig[2]}}}, {mem_idx.get(i,0)}, '
        f'{fp>>8}, {e.get("fs","0U")}UL, {e.get("es","0")}, '
        f'{e.get("ced","0")}, {e.get("fc","0")}, {e.get("to","200")}, '
        f'{e.get("stab","100")}, {e.get("ce","25")}, {e.get("sl","32")}, '
        f'{e.get("pv","83")}, {e.get("pi","3")}, {fp&0xFF}, '
        f'{e.get("stk_dc","0")}, {e.get("avr_dc","0")}, {e.get("bd","0")} }},'
    )
out_lines.append('};')
out_lines.append(f'#define AVR_DEVICE_COUNT {len(parsed)}')
out_lines.append('')

# API functions
out_lines.append('const AVR_OpGroup* avr_get_op_group(uint8_t idx) { (void)idx; return &g_avrOpGroups[0]; }')
out_lines.append('')
out_lines.append(f'const AVR_MemGroup* avr_get_mem_group(uint8_t idx) {{ return (idx<{len(mem_keys_sorted)})?&g_avrMemGroups[idx]:&g_avrMemGroups[0]; }}')
out_lines.append('')
out_lines.append('const AVR_DeviceEntry* avr_get_device_entry(uint16_t i) { return (i<AVR_DEVICE_COUNT)?&g_avrDeviceTable[i]:0; }')
out_lines.append('')
out_lines.append('const char* avr_get_device_name(uint16_t i) {')
out_lines.append('  if(i>=AVR_DEVICE_COUNT||g_avrDeviceTable[i].name_idx>=AVR_NAME_COUNT) return 0;')
out_lines.append('  return g_avrNameTable[g_avrDeviceTable[i].name_idx]; }')
out_lines.append('')
out_lines.append('uint16_t avr_get_device_count(void) { return AVR_DEVICE_COUNT; }')
out_lines.append('')
out_lines.append('int16_t avr_find_device_by_signature(const uint8_t s[3]) {')
out_lines.append('  uint16_t i; for(i=0;i<AVR_DEVICE_COUNT;i++) {')
out_lines.append('    const AVR_DeviceEntry*e=&g_avrDeviceTable[i];')
out_lines.append('    if(e->signature[0]==s[0]&&e->signature[1]==s[1]&&e->signature[2]==s[2]) return i; } return -1; }')
out_lines.append('')
out_lines.append('int16_t avr_find_device_by_name(const char* n) {')
out_lines.append('  uint16_t i; for(i=0;i<AVR_DEVICE_COUNT;i++) {')
out_lines.append('    const char* dn=avr_get_device_name(i); if(dn&&strcasecmp(dn,n)==0) return i; } return -1; }')
out_lines.append('')
out_lines.append('void avr_get_full_params(const AVR_DeviceEntry* e, void* out, uint16_t sz) {')
out_lines.append('  if(!e||!out||sz<160) return; memset(out,0,sz); uint8_t*b=(uint8_t*)out;')
out_lines.append('  memcpy(b,e->signature,3); b[12]=e->stk500_devcode; b[13]=e->avr910_devcode;')
out_lines.append('  b[14]=e->timeout; b[15]=e->stabdelay; b[16]=e->cmdexedelay; b[17]=e->synchloops;')
out_lines.append('  b[18]=e->bytedelay; b[19]=e->pollvalue; b[20]=e->pollindex;')
out_lines.append('  b[21]=e->chip_erase_delay&0xFF; b[22]=e->chip_erase_delay>>8;')
out_lines.append('  b[25]=e->flash_page_size_lsb; b[26]=e->flash_page_size_msb;')
out_lines.append('  memcpy(b+27,&e->flash_size,4);')
out_lines.append('  b[31]=e->eeprom_size&0xFF; b[39]=e->eeprom_size>>8; b[40]=e->fuse_count;')
out_lines.append('  const AVR_MemGroup*mg=avr_get_mem_group(e->mem_group);')
out_lines.append('  if(mg) memcpy(b+48,mg->mem,sizeof(AVR_MemSlot)*AVR_MEM_MAX);')
out_lines.append('  const AVR_OpGroup*og=avr_get_op_group(0);')
out_lines.append('  if(og) memcpy(b+112,og->op,sizeof(AVR_SPICmd)*AVR_OP_MAX); }')

out_path = r'E:\Codex\Project\Keil\Reference_Local\STM32F103VET6_Programmer_V1\PROGRAMMER\avrDeviceConst.c'
with open(out_path, 'w', encoding='utf-8') as f:
    f.write('\n'.join(out_lines))

sz = os.path.getsize(out_path)
print(f'Written: {out_path} ({len(out_lines)} lines, {sz/1024:.1f} KB)')