"""Generate complete 457-entry g_deviceTable[] from XML and shared tables"""
import xml.etree.ElementTree as ET

# Load XML
xml_path = r'E:\wangjunhua\Project\PicProgrammer\PIC10-120-16-Params-Get\pic10-12-16-init.xml'
tree = ET.parse(xml_path)
root = tree.getroot()

# Extract all devices
devices = []
for dev_el in root.findall('Device'):
    dn = dev_el.find('DeviceName')
    if dn is None or not dn.text: continue
    name = dn.text.strip()

    def get(k):
        """Get value from any XML element"""
        for el in dev_el.iter():
            if el.tag == k and el.text:
                return el.text.strip()
        return None

    # Extract parameters
    inst_set = get('instruction_set_id') or ''
    core_family = int(get('core_family') or '0')
    pc_init   = int(get('pc_init_mode') or '0')
    inst_bits = int(get('inst_bits') or '12')
    has_ee    = int(get('has_eeprom') or '0')
    vpp_min   = int(get('vpp_min_mv') or '0')
    vpp_max   = int(get('vpp_max_mv') or '0')
    vdd_min   = int(get('vdd_min_mv') or '0')
    vdd_max   = int(get('vdd_max_mv') or '0')
    vdd_nom   = int(get('vdd_nominal_mv') or '0')
    lvp_th    = int(get('lvp_threshold_mv') or '0')
    lvp_mode  = int(get('lvp_mode') or '0')
    has_vppfirst = int(get('has_vpp_first') or '1')
    wp = int(get('wait_pgm_us') or '0')
    we = int(get('wait_erase_us') or '0')
    wc = int(get('wait_cfg_us') or '0')
    wu = int(get('wait_userid_us') or '0')
    wee = int(get('wait_eedata_us') or '0')
    wre = int(get('wait_rowerase_us') or '0')
    wlp = int(get('wait_lvpgm_us') or '0')
    wle = int(get('wait_lverase_us') or '0')
    ea  = int(get('erase_algo') or '0')
    hre = int(get('has_row_erase_cmd') or '0')
    rpw = int(get('row_pgm_words') or '1')
    rcw = int(get('row_cfg_words') or '1')
    ruw = int(get('row_userid_words') or '1')
    rew = int(get('row_eedata_words') or '0')
    rrew = int(get('row_erase_words') or '0')
    lpw = int(get('latch_pgm_words') or '1')
    lcw = int(get('latch_cfg_words') or '1')
    luw = int(get('latch_userid_words') or '1')
    lew = int(get('latch_eedata_words') or '0')
    lrew = int(get('latch_rowerase_words') or '0')
    cba = int(get('code_base_addr') or '0x0', 16) if get('code_base_addr') else 0x0000
    cea = int(get('code_end_addr') or '0x0', 16) if get('code_end_addr') else 0x0100
    csb = int(get('config_space_base') or '0x0', 16) if get('config_space_base') else 0x0000
    cfg_addr = int(get('config_addr') or '0x2007', 16) if get('config_addr') else 0x2007
    cfg_cnt = int(get('config_word_count') or '1')
    uid_base = int(get('userid_base') or '0x0', 16) if get('userid_base') else 0x0100
    uid_cnt = int(get('userid_word_count') or '4')
    did_addr = int(get('deviceid_addr') or '0x0', 16) if get('deviceid_addr') else 0x0000
    did_mask = int(get('deviceid_mask') or '0x0', 16) if get('deviceid_mask') else 0x0000
    did_expect = int(get('deviceid_expected') or '0x0', 16) if get('deviceid_expected') else 0x0000
    ee_base = int(get('eedata_base') or '0x0', 16) if get('eedata_base') else 0x0000
    ee_end  = int(get('eedata_end_addr') or '0x0', 16) if get('eedata_end_addr') else 0x0000
    occ_base = int(get('osccal_base') or '0x0', 16) if get('osccal_base') else 0x2008
    occ_cnt = int(get('osccal_word_count') or '1')
    cal_base = int(get('cal_data_base') or '0x0', 16) if get('cal_data_base') else 0x0000
    cal_cnt = int(get('cal_data_word_count') or '0')

    devices.append({
        'name': name, 'core': core_family, 'pc_init': pc_init, 'inst': inst_bits,
        'has_ee': has_ee, 'vpp_min': vpp_min, 'vpp_max': vpp_max,
        'vdd_min': vdd_min, 'vdd_max': vdd_max, 'vdd_nom': vdd_nom,
        'lvp_th': lvp_th, 'lvp_mode': lvp_mode, 'has_vppfirst': has_vppfirst,
        'wp': wp, 'we': we, 'wc': wc, 'wu': wu, 'wee': wee, 'wre': wre,
        'wlp': wlp, 'wle': wle, 'ea': ea, 'hre': hre,
        'rpw': rpw, 'rcw': rcw, 'ruw': ruw, 'rew': rew, 'rrew': rrew,
        'lpw': lpw, 'lcw': lcw, 'luw': luw, 'lew': lew, 'lrew': lrew,
        'cba': cba, 'cea': cea, 'csb': csb, 'cfg_addr': cfg_addr,
        'cfg_cnt': cfg_cnt, 'uid_base': uid_base, 'uid_cnt': uid_cnt,
        'did_addr': did_addr, 'did_mask': did_mask, 'did_expect': did_expect,
        'ee_base': ee_base, 'ee_end': ee_end, 'occ_base': occ_base,
        'occ_cnt': occ_cnt, 'cal_base': cal_base, 'cal_cnt': cal_cnt,
    })

print(f"Parsed {len(devices)} devices from XML")

# Build shared tables from device data
# Power table: unique (vpp/vdd/lvp) combos
power_sigs = {}
power_table = []
for d in devices:
    sig = (d['vpp_min'],d['vpp_max'],d['vdd_min'],d['vdd_max'],d['vdd_nom'],
           d['lvp_th'],d['has_vppfirst'],d['lvp_mode'])
    if sig not in power_sigs:
        power_sigs[sig] = len(power_table)
        power_table.append(sig)

# Seq/wait latch table: unique timing+latch combos
seq_sigs = {}
seq_table = []
for d in devices:
    sig = (d['wp'],d['we'],d['wc'],d['wu'],d['wee'],d['wre'],d['wlp'],d['wle'],
           d['ea'],d['hre'],d['rpw'],d['rcw'],d['ruw'],d['rew'],d['rrew'],
           d['lpw'],d['lcw'],d['luw'],d['lew'],d['lrew'])
    if sig not in seq_sigs:
        seq_sigs[sig] = len(seq_table)
        seq_table.append(sig)

# Space table: unique address space combos
space_sigs = {}
space_table = []
for d in devices:
    sig = (d['cba'],d['cea'],d['csb'],d['cfg_addr'],d['cfg_cnt'],
           d['uid_base'],d['uid_cnt'],d['did_addr'],
           d['ee_base'],d['ee_end'],d['occ_base'],d['occ_cnt'],
           d['cal_base'],d['cal_cnt'])
    if sig not in space_sigs:
        space_sigs[sig] = len(space_table)
        space_table.append(sig)

# Assign indices
for d in devices:
    psig = (d['vpp_min'],d['vpp_max'],d['vdd_min'],d['vdd_max'],d['vdd_nom'],
            d['lvp_th'],d['has_vppfirst'],d['lvp_mode'])
    ssig = (d['wp'],d['we'],d['wc'],d['wu'],d['wee'],d['wre'],d['wlp'],d['wle'],
            d['ea'],d['hre'],d['rpw'],d['rcw'],d['ruw'],d['rew'],d['rrew'],
            d['lpw'],d['lcw'],d['luw'],d['lew'],d['lrew'])
    spsig = (d['cba'],d['cea'],d['csb'],d['cfg_addr'],d['cfg_cnt'],
             d['uid_base'],d['uid_cnt'],d['did_addr'],
             d['ee_base'],d['ee_end'],d['occ_base'],d['occ_cnt'],
             d['cal_base'],d['cal_cnt'])
    d['power_idx'] = power_sigs[psig]
    d['seq_idx']   = seq_sigs[ssig]
    d['space_idx'] = space_sigs[spsig]

print(f"Shared tables: power={len(power_table)}, seq={len(seq_table)}, space={len(space_table)}")

# Generate C code
lines = []
lines.append("/* Auto-generated complete PIC device table (457 devices) */")
lines.append("/* Generated from pic10-12-16-init.xml */")
lines.append("")

# Power table
lines.append(f"/* Power shared table ({len(power_table)} entries) */")
lines.append("static const pic8_power_entry_t g_powerTable[] = {")
for p in power_table:
    lines.append(f"  {{{p[0]},{p[1]},{p[2]},{p[3]},{p[4]},{p[5]},{p[6]},{p[7]}}},")
lines.append("};")
lines.append("")

# Seq table
lines.append(f"/* Seq+latch shared table ({len(seq_table)} entries) */")
lines.append("static const pic8_seq_entry_t g_seqTable[] = {")
for s in seq_table:
    vals = ','.join(str(x) for x in s)
    lines.append(f"  {{{vals}}},")
lines.append("};")
lines.append("")

# Space table
lines.append(f"/* Space shared table ({len(space_table)} entries) */")
lines.append("static const pic8_space_entry_t g_spaceTable[] = {")
for sp in space_table:
    lines.append(f"  {{0x{sp[0]:04X},0x{sp[1]:04X},0x{sp[2]:04X},0x{sp[3]:04X},{sp[4]},0x{sp[5]:04X},{sp[6]},0x{sp[7]:04X},0x{sp[8]:04X},0x{sp[9]:04X},0x{sp[10]:04X},{sp[11]},0x{sp[12]:04X},{sp[13]},0xFF}},")
lines.append("};")
lines.append("")

# Device table
lines.append(f"/* Device table ({len(devices)} entries) */")
lines.append("static const pic8_device_index_t g_deviceTable[] = {")
for d in devices:
    # Device entry: name, power_idx, seq_idx, space_idx, dcr_idx(0), core, pc_init, has_ee, inst_bits, devid_mask, devid_expected
    lines.append(
        f'  {{ "{d["name"]}", {d["power_idx"]}, {d["seq_idx"]}, {d["space_idx"]}, 0, '
        f'{d["core"]}, {d["pc_init"]}, {d["has_ee"]}, {d["inst"]}, '
        f'{d["did_mask"]}, {d["did_expect"]} }},'
    )
lines.append("};")
lines.append(f"#define PIC8_DEVICE_TABLE_SIZE {len(devices)}")
lines.append("")

# API functions
lines.append("""
/* ── API ── */
uint16_t pic8GetDeviceList(const char **names, uint16_t maxCount) {
    uint16_t i, count = PIC8_DEVICE_TABLE_SIZE;
    if (names == NULL || maxCount == 0) return count;
    if (maxCount < count) count = maxCount;
    for (i = 0; i < count; i++) names[i] = g_deviceTable[i].name;
    return PIC8_DEVICE_TABLE_SIZE;
}

int8_t pic8FindDevice(const char *deviceName, pic_prog_params_t *out) {
    uint16_t i;
    if (deviceName == NULL || out == NULL) return -1;
    for (i = 0; i < PIC8_DEVICE_TABLE_SIZE; i++) {
        const char *t = g_deviceTable[i].name;
        const char *d = deviceName;
        uint8_t ok = 1, j;
        for (j = 0; j < 16; j++) {
            char c1 = t[j]; if (c1>='A'&&c1<='Z') c1 += 32;
            char c2 = d[j]; if (c2>='A'&&c2<='Z') c2 += 32;
            if (c1 != c2) { ok = 0; break; }
            if (c1 == 0 && c2 == 0) break;
        }
        if (ok) { pic8AggregateParams(&g_deviceTable[i], out); return 0; }
    }
    return -1;
}

int8_t pic8FindDeviceByIndex(uint16_t index, pic_prog_params_t *out) {
    if (out == NULL || index >= PIC8_DEVICE_TABLE_SIZE) return -1;
    pic8AggregateParams(&g_deviceTable[index], out);
    return 0;
}

uint16_t pic8GetDeviceCount(void) { return PIC8_DEVICE_TABLE_SIZE; }

int8_t pic8GetDeviceEntry(uint16_t index, const pic8_device_index_t **entry) {
    if (entry == NULL || index >= PIC8_DEVICE_TABLE_SIZE) return -1;
    *entry = &g_deviceTable[index];
    return 0;
}
""")

out_path = r'..\PROGRAMMER\picDeviceConst_gen.c'
with open(out_path, 'w', encoding='utf-8') as f:
    f.write('\n'.join(lines))
import os
print(f"Written: {out_path} ({len(lines)} lines, {os.path.getsize(out_path)/1024:.1f} KB)")