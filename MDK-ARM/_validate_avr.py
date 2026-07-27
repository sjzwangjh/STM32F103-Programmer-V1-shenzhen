"""Validate AVR device constants against avrdude.conf"""
import re, os, json

# Parse avrdude.conf
conf_path = r'E:\wangjunhua\Project\AvrProgrammer\avrdude\out\build\x64-Release\src\avrdude.conf'
text = open(conf_path, 'r', encoding='utf-8', errors='ignore').read()

# Find all "part" sections
parts = re.findall(r'part\s*\{(.*?)\}\s*;', text, re.DOTALL)
print(f"Total parts in avrdude.conf: {len(parts)}")

# Extract part parameters
devices = []
for p in parts:
    dev = {}
    # Extract key fields
    for field, pat in {
        'id': r'(?:"([^"]+)")',  # first quoted string after "part {"
        'signature': r'signature\s*=\s*(0x[0-9a-fA-F]{2})\s+(0x[0-9a-fA-F]{2})\s+(0x[0-9a-fA-F]{2})',
        'stk500_devcode': r'stk500_devcode\s*=\s*(\d+)',
        'avr910_devcode': r'avr910_devcode\s*=\s*(\d+)',
        'chip_erase_delay': r'chip_erase_delay\s*=\s*(\d+)',
        'timeout': r'timeout\s*=\s*(\d+)',
        'stabdelay': r'stabdelay\s*=\s*(\d+)',
        'cmdexedelay': r'cmdexedelay\s*=\s*(\d+)',
        'synchloops': r'synchloops\s*=\s*(\d+)',
        'bytedelay': r'bytedelay\s*=\s*(\d+)',
        'pollvalue': r'pollvalue\s*=\s*(0x[0-9a-fA-F]{2})',
        'pollindex': r'pollindex\s*=\s*(\d+)',
        'flash_size': r'flash\s*=\s*(\d+)',  # rough: find first flash value
        'eeprom_size': r'eeprom\s*=\s*(\d+)',
    }.items():
        m = re.search(pat, p, re.IGNORECASE)
        if m:
            dev[field] = m.group(1).strip('"') 
    
    if 'id' in dev:
        # Try to find flash/eeprom sizes from memory sections
        # Flash: memory "flash" ... size = 0xXXXX;
        mem_flash = re.search(r'memory\s+"flash".*?size\s*=\s*(0x[0-9a-fA-F]+)', p, re.DOTALL | re.IGNORECASE)
        if mem_flash: dev['flash_size'] = str(int(mem_flash.group(1), 16))
        mem_ee = re.search(r'memory\s+"eeprom".*?size\s*=\s*(0x[0-9a-fA-F]+)', p, re.DOTALL | re.IGNORECASE)
        if mem_ee: dev['eeprom_size'] = str(int(mem_ee.group(1), 16))
        
        # Flash page size
        page_size = re.search(r'memory\s+"flash".*?page_size\s*=\s*(0x[0-9a-fA-F]+)', p, re.DOTALL | re.IGNORECASE)
        if page_size: dev['flash_page_size'] = str(int(page_size.group(1), 16))
        
        # Fuse count
        fuse_count = len(re.findall(r'memory\s+"[lhe]fuse"', p))
        if fuse_count > 0: dev['fuse_count'] = str(fuse_count)
        
        # Lock bits
        if re.search(r'memory\s+"lock"', p): dev['has_lock'] = '1'
        
        devices.append(dev)

print(f"Parsed {len(devices)} parts from avrdude.conf")

# Compare with our data
c_path = r'..\PROGRAMMER\avrDeviceConst.c'
ctext = open(c_path,'r',encoding='utf-8',errors='ignore').read()

# Extract our device table entries
our_entries = re.findall(r'\{\s*(\d+),\s*\{(\d+),(\d+),(\d+)\}', ctext)
print(f"Our avrDeviceConst entries: {len(our_entries)}")

# Compare for common devices
our_names = re.findall(r'"([A-Za-z0-9_]+)"', ctext.split('g_avrDeviceTable')[0])
print(f"Our name table entries: {len(our_names)}")

# Quick check: ATmega328P
atmega328 = [d for d in devices if d.get('id','').upper().replace(' ','') == 'M328P']
if atmega328:
    print(f"\nATmega328P from avrdude.conf: {json.dumps(atmega328[0], indent=2)}")

# Check a few devices
for test_name in ['ATmega328P', 'ATtiny85', 'ATmega8', 'AT90S1200']:
    conf_dev = [d for d in devices if d.get('id','').upper() == test_name.upper()]
    if conf_dev:
        d = conf_dev[0]
        sig = re.findall(r'0x([0-9a-fA-F]+)', d.get('signature','0x1e 0x90 0x00'))
        print(f"\n{test_name}: sig={sig}, flash={d.get('flash_size','?')}, ee={d.get('eeprom_size','?')}, "
              f"page={d.get('flash_page_size','?')}, ce_delay={d.get('chip_erase_delay','?')}, "
              f"to={d.get('timeout','?')}, stab={d.get('stabdelay','?')}, ce={d.get('cmdexedelay','?')}")