#!/usr/bin/env python3
"""Parse avrdude-avr-init.xml and regenerate g_avrDeviceTable[] with real data."""
import xml.etree.ElementTree as ET
import re, os, json

XML_PATH = r'E:\Codex\Project\Keil\Reference_Local\STM32F103VET6_Programmer_V1\PROGRAMMER\avrdude-avr-init.xml'
C_PATH = r'E:\Codex\Project\Keil\Reference_Local\STM32F103VET6_Programmer_V1\PROGRAMMER\avrDeviceConst.c'

tree = ET.parse(XML_PATH)
root = tree.getroot()
parts = root.findall('part')
print(f'XML has {len(parts)} parts')

# Build parent tree for inheritance
parent_map = {}
for p in parts:
    pid = p.get('id')
    pparent = p.get('parent')
    if pid:
        parent_map[pid] = pparent

def resolve_field(part, field, default=None):
    """Walk parent chain to find a field value."""
    val = part.find(field)
    if val is not None and val.text is not None:
        txt = val.text.strip()
        if txt and txt != 'NULL':
            return txt
    p = part.get('parent')
    if p:
        # Find parent part
        for p2 in parts:
            if p2.get('id') == p:
                return resolve_field(p2, field, default)
    return default

def resolve_signature(part):
    """Resolve signature as three hex numbers."""
    sig = resolve_field(part, 'signature')
    if sig:
        nums = re.findall(r'0x([0-9a-fA-F]+)', sig)
        if len(nums) >= 3:
            return (int(nums[0],16), int(nums[1],16), int(nums[2],16))
    return None

def resolve_int(part, field, default=0):
    val = resolve_field(part, field)
    if val:
        try:
            return int(val)
        except:
            pass
    return default

# Read current name table from C file
c_text = open(C_PATH, 'r', encoding='utf-8', errors='ignore').read()
# Extract the name table
name_match = re.search(r'static const char g_avrNameTable\[\]\[16\] = \{(.*?)\};', c_text, re.DOTALL)
if not name_match:
    print('ERROR: Could not find name table in C file')
    exit(1)
name_block = name_match.group(1)
names = re.findall(r'"([A-Za-z0-9_]+)"', name_block)
print(f'Found {len(names)} names in C file')

# Build a mapping from name to XML part
# XML id is like "1200", "m328p", "t85" etc.
# Names are like "AT90S1200", "ATmega328P", "ATtiny85"
# Match by normalizing: remove "AT", lowercase
def normalize_name(n):
    n = n.upper().replace('AT', '', 1).replace('ATMEGA', 'M').replace('ATTINY', 'T').replace('AT90S', 'S')
    n = n.replace('AVR', '').replace('XMEGA', 'X').replace('USB', 'U')
    return n

# Index parts by id
part_by_id = {}
for p in parts:
    pid = p.get('id')
    if pid:
        part_by_id[pid] = p

# Also index by desc
part_by_desc = {}
for p in parts:
    desc = p.get('desc')
    if desc:
        part_by_desc[desc.upper()] = p

# Match each name to a part
matched = 0
unmatched = []
device_entries = []

for i, name in enumerate(names):
    # Try direct desc match first
    part = part_by_desc.get(name.upper())
    
    # Try normalizing
    if part is None:
        norm = normalize_name(name)
        for pid, p in part_by_id.items():
            desc = p.get('desc', '').upper()
            if norm in desc.upper() or norm == pid.upper():
                part = p
                break
    
    if part is None:
        # Try matching by removing AT prefix
        simple = name.upper().replace('AT', '', 1)
        for pid, p in part_by_id.items():
            desc = p.get('desc', '').upper()
            if simple in desc:
                part = p
                break
    
    if part is None:
        unmatched.append(name)
        # Use defaults
        sig = (0,0,0)
        flash_size = 0
        eeprom_size = 0
        chip_erase_delay = 0
        timeout = 200
        stabdelay = 100
        cmdexedelay = 25
        synchloops = 32
        pollvalue = 83
        stk500_dc = 0
        avr910_dc = 0
        fuse_count = 0
        flash_page_size = 0
    else:
        matched += 1
        sig = resolve_signature(part) or (0,0,0)
        
        # Get flash size from memory section
        flash_size = 0
        memories = part.find('memories')
        if memories is not None:
            for mem in memories.findall('memory'):
                if mem.get('type') == 'flash':
                    sz = mem.find('size')
                    if sz is not None and sz.text:
                        flash_size = int(sz.text)
                    break
        
        eeprom_size = 0
        if memories is not None:
            for mem in memories.findall('memory'):
                if mem.get('type') == 'eeprom':
                    sz = mem.find('size')
                    if sz is not None and sz.text:
                        eeprom_size = int(sz.text)
                    break
        
        chip_erase_delay = resolve_int(part, 'chip_erase_delay', 0)
        timeout = resolve_int(part, 'timeout', 200)
        stabdelay = resolve_int(part, 'stabdelay', 100)
        cmdexedelay = resolve_int(part, 'cmdexedelay', 25)
        synchloops = resolve_int(part, 'synchloops', 32)
        pollvalue = resolve_int(part, 'pollvalue', 83)
        stk500_dc = resolve_int(part, 'stk500_devcode', 0)
        avr910_dc = resolve_int(part, 'avr910_devcode', 0)
        
        # Count fuse bits
        fuse_count = 0
        if memories is not None:
            for t in ['lfuse', 'hfuse', 'efuse']:
                for mem in memories.findall('memory'):
                    if mem.get('type') == t:
                        fuse_count += 1
        
        # Flash page size
        flash_page_size = 0
        if memories is not None:
            for mem in memories.findall('memory'):
                if mem.get('type') == 'flash':
                    ps = mem.find('page_size')
                    if ps is not None and ps.text:
                        flash_page_size = int(ps.text)
                    break
    
    # Build device entry values (same format as current)
    fp_msb = (flash_page_size >> 8) & 0xFF
    fp_lsb = flash_page_size & 0xFF
    
    device_entries.append({
        'name_idx': i,
        'sig0': sig[0], 'sig1': sig[1], 'sig2': sig[2],
        'mem_group': 0,
        'fp_msb': fp_msb,
        'flash_size': flash_size,
        'eeprom_size': eeprom_size,
        'chip_erase_delay': chip_erase_delay,
        'fuse_count': fuse_count,
        'timeout': timeout,
        'stabdelay': stabdelay,
        'cmdexedelay': cmdexedelay,
        'synchloops': synchloops,
        'pollvalue': pollvalue,
        'pollindex': 3,
        'fp_lsb': fp_lsb,
        'stk500_dc': stk500_dc,
        'avr910_dc': avr910_dc,
        'bytedelay': 0,
        'runtime_group': 0
    })

print(f'\nMatched: {matched}, Unmatched: {len(unmatched)}')
if unmatched:
    print(f'Unmatched devices: {unmatched[:10]}...')

# Generate C code for device table
lines = []
lines.append(f'/* Device entries ({len(device_entries)} total, regenerated from XML) */')
lines.append('static const AVR_DeviceEntry g_avrDeviceTable[] = {')
for e in device_entries:
    lines.append(
        f'  {{ {e["name_idx"]}, {{{e["sig0"]},{e["sig1"]},{e["sig2"]}}}, '
        f'{e["mem_group"]}, {e["fp_msb"]}, {e["flash_size"]}UL, {e["eeprom_size"]}, '
        f'{e["chip_erase_delay"]}, {e["fuse_count"]}, {e["timeout"]}, '
        f'{e["stabdelay"]}, {e["cmdexedelay"]}, {e["synchloops"]}, '
        f'{e["pollvalue"]}, {e["pollindex"]}, {e["fp_lsb"]}, '
        f'{e["stk500_dc"]}, {e["avr910_dc"]}, {e["bytedelay"]} }},'
    )
lines.append('};')
lines.append(f'#define AVR_DEVICE_COUNT {len(device_entries)}')
lines.append('')

# Read current C file and replace the device table
with open(C_PATH, 'r', encoding='utf-8') as f:
    full = f.read()

# Find the g_avrDeviceTable section
start = full.find('static const AVR_DeviceEntry g_avrDeviceTable[]')
if start < 0:
    print('ERROR: Could not find g_avrDeviceTable in C file')
    exit(1)

# Find the matching end (the }; after all entries)
end = full.find('#define AVR_DEVICE_COUNT', start)
if end < 0:
    print('ERROR: Could not find AVR_DEVICE_COUNT marker')
    exit(1)

# Find the start of the line with begin marker
line_start = full.rfind('\n', 0, start) + 1

# Build new content
new_content = full[:line_start] + '\n'.join(lines) + '\n\n' + full[end:]

with open(C_PATH, 'w', encoding='utf-8') as f:
    f.write(new_content)

print(f'\nDone! Wrote {len(device_entries)} entries to {C_PATH}')
print(f'File size: {os.path.getsize(C_PATH)/1024:.1f} KB')