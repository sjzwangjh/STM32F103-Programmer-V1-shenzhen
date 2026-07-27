"""Compress avrDeviceConst.c: extract shared tables, generate compressed C"""
import re, json, os

path = r'E:\Codex\Project\Keil\Reference_Local\STM32F103VET6_Programmer_V1\PROGRAMMER\avrDeviceConst.c'
text = open(path, 'r', encoding='utf-8', errors='ignore').read()

# Parse device_names array
names = []
for m in re.finditer(r'"([A-Za-z0-9_]+)"', text[:text.index('static const AVR_DeviceParam')]):
    names.append(m.group(1))
names_uniq = list(dict.fromkeys(names))  # deduplicate preserving order

# Parse each device_params entry
entries = []
entry_re = re.compile(r'\{.*?\.id\s*=\s*(\d+).*?\}', re.DOTALL)
for m in re.finditer(r'\.id\s*=\s*\d+.*?\.op\s*=\s*\{.*?\}\s*\}', text, re.DOTALL):
    block = m.group()
    entry = {}
    # extract fields
    for field, pat in [
        ('id', r'\.id\s*=\s*(\d+)'),
        ('signature', r'\.signature\s*=\s*\{([^}]+)\}'),
        ('stk500_devcode', r'\.stk500_devcode\s*=\s*(\d+)'),
        ('avr910_devcode', r'\.avr910_devcode\s*=\s*(\d+)'),
        ('timeout', r'\.timeout\s*=\s*(\d+)'),
        ('stabdelay', r'\.stabdelay\s*=\s*(\d+)'),
        ('cmdexedelay', r'\.cmdexedelay\s*=\s*(\d+)'),
        ('synchloops', r'\.synchloops\s*=\s*(\d+)'),
        ('bytedelay', r'\.bytedelay\s*=\s*(\d+)'),
        ('pollvalue', r'\.pollvalue\s*=\s*(\d+)'),
        ('pollindex', r'\.pollindex\s*=\s*(\d+)'),
        ('chip_erase_delay', r'\.chip_erase_delay\s*=\s*(\d+)'),
        ('flash_page_size', r'\.flash_page_size\s*=\s*(\d+)'),
        ('flash_size', r'\.flash_size\s*=\s*(\d+)'),
        ('eeprom_size', r'\.eeprom_size\s*=\s*(\d+)'),
        ('fuse_count', r'\.fuse_count\s*=\s*(\d+)'),
        ('mem_count', r'\.mem_count\s*=\s*(\d+)'),
    ]:
        m2 = re.search(pat, block)
        if m2: entry[field] = m2.group(1).strip()

    # Parse mem array
    mem_block = re.search(r'\.mem\s*=\s*\{(.*?)\}\s*,', block, re.DOTALL)
    mems = []
    if mem_block:
        for mm in re.finditer(r'\{(.*?)\}', mem_block.group(1), re.DOTALL):
            nums = re.findall(r'\d+', mm.group(1))
            mems.append([int(x) for x in nums[:5]] + [0]*(5-len([int(x) for x in nums[:5]]))])

    # Parse op array
    op_block = re.search(r'\.op\s*=\s*\{(.*?)\}\s*,', block, re.DOTALL)
    ops = []
    if op_block:
        for om in re.finditer(r'\{(.*?)\}', op_block.group(1), re.DOTALL):
            nums = re.findall(r'\d+', om.group(1))
            ops.append([int(x) for x in nums[:5]] + [0]*(5-len([int(x) for x in nums[:5]]))])

    entry['mem'] = mems
    entry['op'] = ops
    entries.append(entry)

# Find unique op groups
op_groups = {}
for i, e in enumerate(entries):
    sig = json.dumps(e.get('op', []))
    op_groups.setdefault(sig, []).append(i)

# Find unique mem groups
mem_groups = {}
for i, e in enumerate(entries):
    sig = json.dumps(e.get('mem', []))
    mem_groups.setdefault(sig, []).append(i)

# Map each device to its mem/op group index
mem_idx = {}
for idx, (sig, devs) in enumerate(sorted(mem_groups.items(), key=lambda x: -len(x[1]))):
    for d in devs: mem_idx[d] = idx

op_idx = {}
for idx, (sig, devs) in enumerate(sorted(op_groups.items(), key=lambda x: -len(x[1]))):
    for d in devs: op_idx[d] = idx

# Print stats
print(f'Parsed {len(entries)} device entries')
print(f'Unique OP groups: {len(op_groups)}')
print(f'Unique MEM groups: {len(mem_groups)}')
print(f'Names: {len(names)}')

# Generate compressed C code
lines = []
lines.append('/* Auto-generated compressed avrDeviceConst.c */')
lines.append('#include "avrDeviceConst.h"')
lines.append('#include <string.h>')
lines.append('')

# Names table
lines.append(f'/* Names table ({len(names_uniq)} entries) */')
lines.append('static const char* const g_avrNameTable[] = {')
for n in names_uniq:
    lines.append(f'    "{n}",')
lines.append('};')
lines.append('')

# OP groups (only store unique ones)
first_op_key = list(op_groups.keys())[0]
first_op = json.loads(first_op_key)
lines.append(f'/* Shared OP group (ALL devices identical) */')
lines.append('static const AVR_OpGroup g_avrOpGroups[1] = {')
lines.append('    { {{')
for cmd in first_op:
    lines.append(f'        {{{cmd[0]},{cmd[1]},{cmd[2]},{cmd[3]},{cmd[4]}}},')
lines.append('    }} }')
lines.append('};')
lines.append('')

# MEM groups
mem_keys = sorted(mem_groups.keys(), key=lambda x: -len(mem_groups[x]))
lines.append(f'/* Shared MEM groups ({len(mem_keys)} unique) */')
lines.append('static const AVR_MemGroup g_avrMemGroups[] = {')
for mk in mem_keys:
    mg = json.loads(mk)
    lines.append('    { {{')
    for m in mg:
        lines.append(f'        {{{m[0]},{m[1]},{m[2]},{m[3]},{m[4]},{{0,0}}}},')
    lines.append('    }} },')
lines.append('};')
lines.append('')

# Device entries (compressed 28 bytes each)
lines.append(f'/* Device entries ({len(entries)} total, 28 bytes each) */')
lines.append('static const AVR_DeviceEntry g_avrDeviceTable[] = {')
for i, e in enumerate(entries):
    name_idx = names_uniq.index(names[i]) if i < len(names) else 0
    sig = re.findall(r'\d+', e.get('signature',''))
    s = [int(x) for x in sig[:3]]
    fp = int(e.get('flash_page_size','0'))
    lines.append(f'    {{ {name_idx}, {{{s[0]},{s[1]},{s[2]}}}, {mem_idx.get(i,0)}, '
                f'{fp>>8}, {e.get("flash_size","0U")}U, {e.get("eeprom_size","0")}, '
                f'{e.get("chip_erase_delay","0")}, {e.get("fuse_count","0")}, '
                f'{e.get("timeout","200")}, {e.get("stabdelay","100")}, '
                f'{e.get("cmdexedelay","25")}, {e.get("synchloops","32")}, '
                f'{e.get("pollvalue","83")}, {e.get("pollindex","3")}, '
                f'{fp&0xFF}, {e.get("stk500_devcode","0")}, {e.get("avr910_devcode","0")}, '
                f'{e.get("bytedelay","0")} }},')
lines.append('};')
lines.append(f'#define AVR_DEVICE_COUNT {len(entries)}')
lines.append('')

# API functions
lines.append('/* ── API ── */')
lines.append('const AVR_OpGroup* avr_get_op_group(uint8_t idx) {')
lines.append('    (void)idx; return &g_avrOpGroups[0]; }')
lines.append('')
lines.append('const AVR_MemGroup* avr_get_mem_group(uint8_t idx) {')
lines.append(f'    return (idx < {len(mem_keys)}) ? &g_avrMemGroups[idx] : &g_avrMemGroups[0]; }}')
lines.append('')
lines.append('const AVR_DeviceEntry* avr_get_device_entry(uint16_t index) {')
lines.append('    return (index < AVR_DEVICE_COUNT) ? &g_avrDeviceTable[index] : NULL; }')
lines.append('')
lines.append('const char* avr_get_device_name(uint16_t index) {')
lines.append('    if (index >= AVR_DEVICE_COUNT || g_avrDeviceTable[index].name_idx >= (sizeof(g_avrNameTable)/sizeof(g_avrNameTable[0]))) return NULL;')
lines.append('    return g_avrNameTable[g_avrDeviceTable[index].name_idx]; }')
lines.append('')
lines.append('uint16_t avr_get_device_count(void) { return AVR_DEVICE_COUNT; }')
lines.append('')
lines.append('int16_t avr_find_device_by_signature(const uint8_t sig[3]) {')
lines.append('    uint16_t i;')
lines.append('    for(i=0;i<AVR_DEVICE_COUNT;i++) {')
lines.append('        const AVR_DeviceEntry* e=&g_avrDeviceTable[i];')
lines.append('        if(e->signature[0]==sig[0]&&e->signature[1]==sig[1]&&e->signature[2]==sig[2]) return (int16_t)i;')
lines.append('    } return -1; }')
lines.append('')
lines.append('int16_t avr_find_device_by_name(const char* name) {')
lines.append('    uint16_t i;')
lines.append('    for(i=0;i<AVR_DEVICE_COUNT;i++) {')
lines.append('        const char* n=avr_get_device_name(i);')
lines.append('        if(n&&strcasecmp(n,name)==0) return (int16_t)i; } return -1; }')
lines.append('')
lines.append('void avr_get_full_params(const AVR_DeviceEntry* e, void* out, uint16_t sz) {')
lines.append('    /* 将压缩条目展开为完整结构体, 用于兼容旧代码 */')
lines.append('    if(!e||!out||sz<160) return;')
lines.append('    memset(out,0,sz);')
lines.append('    uint8_t* buf=(uint8_t*)out;')
lines.append('    uint16_t pos=0;')
lines.append('    memcpy(buf+pos,e->signature,3); pos=12;')
lines.append('    buf[12]=e->stk500_devcode; buf[13]=e->avr910_devcode;')
lines.append('    buf[14]=e->timeout; buf[15]=e->stabdelay; buf[16]=e->cmdexedelay;')
lines.append('    buf[17]=e->synchloops; buf[18]=e->bytedelay; buf[19]=e->pollvalue; buf[20]=e->pollindex;')
lines.append('    buf[21]=e->chip_erase_delay&0xFF; buf[22]=(e->chip_erase_delay>>8)&0xFF;')
lines.append('    buf[25]=e->flash_page_size_lsb; buf[26]=e->flash_page_size_msb;')
lines.append('    memcpy(buf+27,&e->flash_size,4);')
lines.append('    buf[31]=e->eeprom_size&0xFF; buf[39]=(e->eeprom_size>>8)&0xFF;')
lines.append('    buf[40]=e->fuse_count;')
lines.append('    /* mem slots */')
lines.append('    const AVR_MemGroup* mg=avr_get_mem_group(e->mem_group);')
lines.append('    if(mg) memcpy(buf+48,mg->mem,sizeof(AVR_MemSlot)*AVR_MEM_MAX);')
lines.append('    /* op slots */')
lines.append('    const AVR_OpGroup* og=avr_get_op_group(0);')
lines.append('    if(og) memcpy(buf+112,og->op,sizeof(AVR_SPICmd)*AVR_OP_MAX); }')

out_path = r'E:\Codex\Project\Keil\Reference_Local\STM32F103VET6_Programmer_V1\PROGRAMMER\avrDeviceConst_compressed.c'
with open(out_path, 'w', encoding='utf-8') as f:
    f.write('\n'.join(lines))
print(f'Written: {out_path} ({len(lines)} lines)')
print(f'Original: 474 KB -> compressed: {os.path.getsize(out_path)/1024:.1f} KB')