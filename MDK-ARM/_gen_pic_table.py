from pathlib import Path
import re
import xml.etree.ElementTree as ET

ROOT = Path(r"E:\Codex\Project\Keil\Reference_Local\STM32F103VET6_Programmer_V1")
PROGRAMMER = ROOT / "PROGRAMMER"
XML_PATH = PROGRAMMER / "pic10-12-16-init.xml"
C_PATH = PROGRAMMER / "picDeviceConst.c"
H_PATH = PROGRAMMER / "picDeviceConst.h"


def read_text_auto(path: Path):
    for enc in ("utf-8", "utf-8-sig", "gbk", "cp936", "latin1"):
        try:
            return path.read_text(encoding=enc), enc
        except UnicodeDecodeError:
            continue
    raise RuntimeError(f"Unable to decode: {path}")


def write_text(path: Path, text: str, encoding: str):
    normalized = text.replace("\r\n", "\n").replace("\r", "\n")
    path.write_text(normalized, encoding=encoding, newline="\n")


def normalize_source_text(text: str) -> str:
    text = text.replace("`r`n", "\n")
    return text.replace("\r\n", "\n").replace("\r", "\n")


def get_value(dev_el, key):
    for el in dev_el.iter():
        if el.tag == key and el.text:
            return el.text.strip()
    return None


def get_int(dev_el, key, default=0, base=10):
    val = get_value(dev_el, key)
    if not val:
        return default
    return int(val, base)


def fmt_hex(value: int, width: int = 4) -> str:
    return f"0x{value:0{width}X}"


def build_tables():
    root = ET.parse(XML_PATH).getroot()
    devices = []
    for dev_el in root.findall("Device"):
        name = get_value(dev_el, "DeviceName")
        if not name:
            continue
        d = {
            "name": name,
            "core": get_int(dev_el, "core_family", 0),
            "pc_init": get_int(dev_el, "pc_init_mode", 0),
            "inst": get_int(dev_el, "inst_bits", 12),
            "has_ee": get_int(dev_el, "has_eeprom", 0),
            "vpp_min": get_int(dev_el, "vpp_min_mv", 0),
            "vpp_max": get_int(dev_el, "vpp_max_mv", 0),
            "vdd_min": get_int(dev_el, "vdd_min_mv", 0),
            "vdd_max": get_int(dev_el, "vdd_max_mv", 0),
            "vdd_nom": get_int(dev_el, "vdd_nominal_mv", 0),
            "lvp_th": get_int(dev_el, "lvp_threshold_mv", 0),
            "lvp_mode": get_int(dev_el, "lvp_mode", 0),
            "has_vppfirst": get_int(dev_el, "has_vpp_first", 1),
            "wp": get_int(dev_el, "wait_pgm_us", 0),
            "we": get_int(dev_el, "wait_erase_us", 0),
            "wc": get_int(dev_el, "wait_cfg_us", 0),
            "wu": get_int(dev_el, "wait_userid_us", 0),
            "wee": get_int(dev_el, "wait_eedata_us", 0),
            "wre": get_int(dev_el, "wait_rowerase_us", 0),
            "wlp": get_int(dev_el, "wait_lvpgm_us", 0),
            "wle": get_int(dev_el, "wait_lverase_us", 0),
            "ea": get_int(dev_el, "erase_algo", 0),
            "hre": get_int(dev_el, "has_row_erase_cmd", 0),
            "rpw": get_int(dev_el, "row_pgm_words", 1),
            "rcw": get_int(dev_el, "row_cfg_words", 1),
            "ruw": get_int(dev_el, "row_userid_words", 1),
            "rew": get_int(dev_el, "row_eedata_words", 0),
            "rrew": get_int(dev_el, "row_erase_words", 0),
            "lpw": get_int(dev_el, "latch_pgm_words", 1),
            "lcw": get_int(dev_el, "latch_cfg_words", 1),
            "luw": get_int(dev_el, "latch_userid_words", 1),
            "lew": get_int(dev_el, "latch_eedata_words", 0),
            "lrew": get_int(dev_el, "latch_rowerase_words", 0),
            "cba": get_int(dev_el, "code_base_addr", 0x0000, 16),
            "cea": get_int(dev_el, "code_end_addr", 0x0100, 16),
            "csb": get_int(dev_el, "config_space_base", 0x0000, 16),
            "cfg_addr": get_int(dev_el, "config_addr", 0x2007, 16),
            "cfg_cnt": get_int(dev_el, "config_word_count", 1),
            "uid_base": get_int(dev_el, "userid_base", 0x0100, 16),
            "uid_cnt": get_int(dev_el, "userid_word_count", 4),
            "did_addr": get_int(dev_el, "deviceid_addr", 0x0000, 16),
            "did_mask": get_int(dev_el, "deviceid_mask", 0x0000, 16),
            "did_expect": get_int(dev_el, "deviceid_expected", 0x0000, 16),
            "ee_base": get_int(dev_el, "eedata_base", 0x0000, 16),
            "ee_end": get_int(dev_el, "eedata_end_addr", 0x0000, 16),
            "occ_base": get_int(dev_el, "osccal_base", 0x2008, 16),
            "occ_cnt": get_int(dev_el, "osccal_word_count", 1),
            "cal_base": get_int(dev_el, "cal_data_base", 0x0000, 16),
            "cal_cnt": get_int(dev_el, "cal_data_word_count", 0),
        }
        devices.append(d)

    power_table = []
    power_map = {}
    seq_table = []
    seq_map = {}
    space_table = []
    space_map = {}

    for d in devices:
        power_sig = (
            d["vpp_min"], d["vpp_max"], d["vdd_min"], d["vdd_max"],
            d["vdd_nom"], d["lvp_th"], d["has_vppfirst"], d["lvp_mode"],
        )
        seq_sig = (
            d["wp"], d["we"], d["wc"], d["wu"], d["wee"], d["wre"],
            d["wlp"], d["wle"], d["ea"], d["hre"], d["rpw"], d["rcw"],
            d["ruw"], d["rew"], d["rrew"], d["lpw"], d["lcw"], d["luw"],
            d["lew"], d["lrew"],
        )
        space_sig = (
            d["cba"], d["cea"], d["csb"], d["cfg_addr"], d["cfg_cnt"],
            d["uid_base"], d["uid_cnt"], d["did_addr"], d["ee_base"],
            d["ee_end"], d["occ_base"], d["occ_cnt"], d["cal_base"],
            d["cal_cnt"],
        )

        d["power_idx"] = power_map.setdefault(power_sig, len(power_table))
        if d["power_idx"] == len(power_table):
            power_table.append(power_sig)

        d["seq_idx"] = seq_map.setdefault(seq_sig, len(seq_table))
        if d["seq_idx"] == len(seq_table):
            seq_table.append(seq_sig)

        d["space_idx"] = space_map.setdefault(space_sig, len(space_table))
        if d["space_idx"] == len(space_table):
            space_table.append(space_sig)

    return devices, power_table, seq_table, space_table


def render_power_table(power_table):
    lines = [
        f"/* Power shared table ({len(power_table)} entries) */",
        "static const pic8_power_entry_t g_powerTable[] = {",
    ]
    for p in power_table:
        lines.append(f"  {{{p[0]},{p[1]},{p[2]},{p[3]},{p[4]},{p[5]},{p[6]},{p[7]}}},")
    lines.append("};")
    return "\n".join(lines)


def render_seq_table(seq_table):
    lines = [
        f"/* Seq+latch shared table ({len(seq_table)} entries) */",
        "static const pic8_seq_entry_t g_seqTable[] = {",
    ]
    for s in seq_table:
        lines.append("  {" + ",".join(str(x) for x in s) + "},")
    lines.append("};")
    return "\n".join(lines)


def render_space_table(space_table):
    lines = [
        f"/* Space shared table ({len(space_table)} entries) */",
        "static const pic8_space_entry_t g_spaceTable[] = {",
    ]
    for sp in space_table:
        lines.append(
            f"  {{{fmt_hex(sp[0])},{fmt_hex(sp[1])},{fmt_hex(sp[2])},{fmt_hex(sp[3])},{sp[4]},{fmt_hex(sp[5])},{sp[6]},{fmt_hex(sp[7])},{fmt_hex(sp[8])},{fmt_hex(sp[9])},{fmt_hex(sp[10])},{sp[11]},{fmt_hex(sp[12])},{sp[13]},0xFF}},"
        )
    lines.append("};")
    return "\n".join(lines)


def render_device_table(devices):
    lines = [
        f"/* Device table ({len(devices)} entries) */",
        "static const pic8_device_index_t g_deviceTable[] = {",
    ]
    for d in devices:
        lines.append(
            f'  {{ "{d["name"]}", {d["power_idx"]}, {d["seq_idx"]}, {d["space_idx"]}, 0, {d["core"]}, {d["pc_init"]}, {d["has_ee"]}, {d["inst"]}, {fmt_hex(d["did_mask"])}, {fmt_hex(d["did_expect"])} }},'
        )
    lines.append("};")
    return "\n".join(lines)


def replace_table_block(text: str, marker: str, replacement: str) -> str:
    pattern = re.compile(re.escape(marker) + r".*?^};\s*$", re.S | re.M)
    new_text, count = pattern.subn(replacement, text, count=1)
    if count != 1:
        raise RuntimeError(f"Failed to replace block: {marker}")
    return new_text


def main():
    devices, power_table, seq_table, space_table = build_tables()
    c_text, c_enc = read_text_auto(C_PATH)
    h_text, h_enc = read_text_auto(H_PATH)

    c_text = normalize_source_text(c_text)
    h_text = normalize_source_text(h_text)

    c_text = re.sub(
        r"/\* Auto-generated complete PIC device table \(\d+ devices\) \*/",
        f"/* Auto-generated complete PIC device table ({len(devices)} devices) */",
        c_text,
        count=1,
    )
    c_text = replace_table_block(
        c_text,
        "static const pic8_power_entry_t g_powerTable[] = {",
        render_power_table(power_table),
    )
    c_text = replace_table_block(
        c_text,
        "static const pic8_seq_entry_t g_seqTable[] = {",
        render_seq_table(seq_table),
    )
    c_text = replace_table_block(
        c_text,
        "static const pic8_space_entry_t g_spaceTable[] = {",
        render_space_table(space_table),
    )
    c_text = replace_table_block(
        c_text,
        "static const pic8_device_index_t g_deviceTable[] = {",
        render_device_table(devices),
    )

    h_text, count = re.subn(
        r"#define\s+PIC8_DEVICE_TABLE_SIZE\s+\d+",
        f"#define PIC8_DEVICE_TABLE_SIZE               {len(devices)}",
        h_text,
        count=1,
    )
    if count != 1:
        raise RuntimeError("Failed to update PIC8_DEVICE_TABLE_SIZE")

    write_text(C_PATH, c_text, c_enc)
    write_text(H_PATH, h_text, h_enc)

    print(f"XML devices: {len(devices)}")
    print(f"Power table: {len(power_table)}")
    print(f"Seq table: {len(seq_table)}")
    print(f"Space table: {len(space_table)}")
    print(f"Updated: {C_PATH}")
    print(f"Updated: {H_PATH}")


if __name__ == "__main__":
    main()
