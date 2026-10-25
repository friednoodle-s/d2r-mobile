#!/usr/bin/env python3
# 生成 dm/host/dm_loadout_tab.h：一键配装（圣火圣骑士整套 + 佣兵三件）的件与词缀。
# 依据：视频画面 OCR（research\bili_字幕\BV1Q2hP6LEkw_装备词缀_OCR.md）+ 原版表（uniqueitems / runes / properties 的可变范围）。
# 暗金与符文之语只写「表里可变」的词缀并取最大值（固定词缀生成时已对）；底材自带的（超强、法杖技能、盾抗）写进底材词缀表；稀有 / 手工 / 魔法件清空后照 OCR 全写。
# 属性值按 itemstatcost 的 ValShift 换成原始值；layer 按 C:\Users\miiro\agy_tasks\20261025_d2r_stat_re\report.md 第 1 节（技能 / 光环 = 技能号，职业技能 = 职业，技能树 = 职业 × 8 + 树，触发施放 / 充能 = 技能 << 6 | 等级）。
# 用法：py -3 -X utf8 dm/ios/tools/gen_loadout.py <资源根目录> > dm/host/dm_loadout_tab.h
# Generates dm/host/dm_loadout_tab.h: pieces and affixes of the one-click loadout (whole Holy Fire paladin set + three mercenary pieces).
# Basis: on-screen OCR of the video + the original tables (variable ranges in uniqueitems / runes / properties).
# Uniques and runewords only get their variable affixes, at the maximum (fixed ones are already right on creation); base-item extras (superior, staff skills, shield resists) go into the base's affix list; rare / crafted / magic pieces are wiped and written in full from the OCR.
# Values are converted to raw values by itemstatcost ValShift; layers follow section 1 of the stat_re report (skill / aura = skill id, class skills = class, skill tab = class × 8 + tab, chance to cast / charges = skill << 6 | level).
# Usage: py -3 -X utf8 dm/ios/tools/gen_loadout.py <resource root> > dm/host/dm_loadout_tab.h
import os
import sys

sys.dont_write_bytecode = True
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import romfs_art  # noqa: E402


def table(root, name):
    raw = romfs_art.read(root, "data/global/excel/" + name)
    txt = raw.decode("utf-8", "replace") if isinstance(raw, bytes) else raw
    lines = txt.split("\n")
    hdr = lines[0].rstrip("\r").split("\t")
    return [dict(zip(hdr, l.rstrip("\r").split("\t"))) for l in lines[1:] if l.strip()]


root = sys.argv[1] if len(sys.argv) > 1 else r"C:\Users\miiro\Desktop\d2\res\D2R Mobile Resources"
ISC = {r["Stat"]: r for r in table(root, "itemstatcost.txt")}
SK = {r["skill"]: int(r["*Id"]) for r in table(root, "skills.txt") if r.get("*Id", "").strip().lstrip("-").isdigit()}


def sid(name):
    return int(ISC[name]["*ID"])


def raw(name, v):
    sh = ISC[name].get("ValShift", "").strip()
    return v << int(sh) if sh else v


def skill(name):
    if name not in SK:
        sys.exit("技能名查不到: " + name)
    return SK[name]


PAL = 3  # 圣骑士职业号 | paladin class id


def S(name, v, layer=0):
    return (sid(name), layer, raw(name, v))


def all_res(v):
    return [S("fireresist", v), S("lightresist", v), S("coldresist", v), S("poisonresist", v)]


def all_stats(v):
    return [S("strength", v), S("energy", v), S("dexterity", v), S("vitality", v)]


def ed(v):
    return [S("item_maxdamage_percent", v), S("item_mindamage_percent", v)]


def ctc(name, sk, lvl, chance):
    return S(name, chance, skill(sk) << 6 | lvl)


def charges(sk, lvl, n):
    return (sid("item_charged_skill"), skill(sk) << 6 | lvl, n << 8 | n)


# 件：(名字, 装备位, 副手组, 佣兵, 种类, 品质, 无形, 暗金加孔, 暗金行 / 符文之语键, 底材码, 镶嵌物, 词缀, 底材词缀, 清空, 镶嵌珠宝词缀)
# piece: (name, slot, secondary set, merc, kind, quality, ethereal, extra sockets, unique row / runeword key, base code, fillers, affixes, base affixes, wipe, filler jewel affixes)
P = []


def piece(what, slot, kind, key=None, base=None, set2=0, merc=0, q=0, eth=0, nsock=0, fill=(), st=(), bst=(), clear=0, fst=()):
    P.append(dict(what=what, slot=slot, set2=set2, merc=merc, kind=kind, q=q, eth=eth, nsock=nsock, key=key, base=base,
                  fill=list(fill), st=list(st), bst=list(bst), clear=clear, fst=list(fst)))


# 主手组 | primary set
piece("正义之手", 4, "LK_RUNEWORD", "Runeword48", "7cr ", st=ed(330), bst=ed(10) + [S("tohit", 3)])
piece("巨龙（盾）", 5, "LK_RUNEWORD", "Runeword27", "pab ", st=all_stats(5), bst=[S("item_armor_percent", 15)] + all_res(45))
piece("岁月之冠", 1, "LK_UNIQUE", 344, nsock=2, fill=["r32 ", "#398"],
      st=all_res(30) + [S("armorclass", 150), S("damageresist", 15)],
      fst=[S("firemindam", 20), S("firemaxdam", 60), S("passive_fire_mastery", 10), S("passive_fire_pierce", 10),
           S("item_addexperience", 5), S("item_magicbonus", 35), ctc("item_skillongethit", "Blaze", 25, 1)])
piece("项链（手工）", 2, "LK_CUSTOM", base="amu ", q=8, clear=1,
      st=[S("item_addclassskills", 2, PAL), S("item_fastercastrate", 10), S("dexterity", 19), S("maxhp", 58), S("maxmana", 15),
          S("manarecoverybonus", 7), charges("Teleport", 2, 25)])
piece("巨龙（甲）", 3, "LK_RUNEWORD", "Runeword27", "utp ", st=all_stats(5), bst=[S("item_armor_percent", 15)])
piece("戒指 混沌之触", 6, "LK_CUSTOM", base="rin ", q=6, clear=1,
      st=[S("item_fastercastrate", 10), S("manadrainmindam", 6), S("strength", 20), S("maxmana", 88),
          S("coldresist", 6), S("lightresist", 6), S("poisonresist", 6), S("fireresist", 25)])
piece("戒指 风暴之眼", 7, "LK_CUSTOM", base="rin ", q=6, clear=1,
      st=[S("item_fastercastrate", 10), S("dexterity", 9), S("maxhp", 36), S("maxmana", 74),
          S("coldresist", 7), S("lightresist", 7), S("poisonresist", 7), S("fireresist", 35)])
piece("腰带（手工）", 8, "LK_CUSTOM", base="uvc ", q=8, clear=1,
      st=[S("item_fastercastrate", 10), S("item_fastergethitrate", 24), S("strength", 26), S("maxhp", 53), S("maxmana", 19),
          S("manarecoverybonus", 6), S("fireresist", 26)])
piece("绯红骑士", 9, "LK_UNIQUE", 241, st=[S("item_armor_percent", 200)])
piece("德古拉之握", 10, "LK_UNIQUE", 364,
      st=[S("item_armor_percent", 120), S("lifedrainmindam", 10), S("item_healafterkill", 10), S("strength", 15)])
# 副手组 | secondary set
piece("战争召唤", 4, "LK_RUNEWORD", "Runeword13", "wsp ", set2=1,
      st=ed(240) + [S("item_nonclassskill", 6, skill("Battle Command")), S("item_nonclassskill", 6, skill("Battle Orders")),
                    S("item_nonclassskill", 4, skill("Battle Cry"))],
      bst=[S("item_singleskill", 3, skill("Redemption")), S("item_singleskill", 3, skill("Fist of the Heavens")),
           S("item_singleskill", 3, skill("Meditation"))])
piece("精神（盾）", 5, "LK_RUNEWORD", "Runeword130", "pab ", set2=1,
      st=[S("maxmana", 112), S("item_fastercastrate", 35), S("item_absorbmagic", 8)],
      bst=[S("item_armor_percent", 15)] + all_res(45))
# 护身符（留背包） | charms (stay in the inventory)
piece("地狱火炬", 0, "LK_UNIQUE", 400, st=[S("item_addclassskills", 3, PAL)] + all_stats(20) + all_res(20))
piece("毁灭", 0, "LK_UNIQUE", 381, st=all_stats(20) + all_res(20) + [S("item_addexperience", 10)])
piece("新生火焰裂隙", 0, "LK_UNIQUE", 433, clear=1,
      st=[S("item_pierce_fire_immunity", 300), S("fireresist", -70), S("passive_fire_pierce", 10)] + all_stats(8) +
         [S("maxhp", 64), S("magic_damage_reduction", 9), S("item_magicbonus", 14)])
piece("活力之雄狮烙印", 0, "LK_CUSTOM", base="cm3 ", q=4, clear=1, st=[S("item_addskill_tab", 1, PAL * 8 + 0), S("maxhp", 45)])
piece("小型咒符 生命抗性", 0, "LK_CUSTOM", base="cm1 ", q=4, clear=1, st=[S("maxhp", 20)] + all_res(5))
piece("小型咒符 生命火抗", 0, "LK_CUSTOM", base="cm1 ", q=4, clear=1, st=[S("maxhp", 20), S("fireresist", 11)])
piece("小型咒符 伤害准确", 0, "LK_CUSTOM", base="cm1 ", q=4, clear=1, st=[S("maxdamage", 3), S("tohit", 20), S("maxhp", 20)])
# 佣兵 | mercenary
piece("佣兵 信心", 4, "LK_RUNEWORD", "Runeword38", "amc ", merc=1,
      st=[S("item_aura", 15, skill("Fanaticism")), S("item_allskills", 2)], bst=ed(15) + [S("item_addskill_tab", 3, 0)])
piece("佣兵 治愈", 1, "LK_RUNEWORD", "Runeword178", "urn ", merc=1, eth=1,
      st=[S("item_armor_percent", 100), S("poisonresist", 30)])
piece("佣兵 荣耀之链", 3, "LK_RUNEWORD", "Runeword14", "utp ", merc=1, eth=1, bst=[S("item_armor_percent", 15)])


def cstats(name, lst):
    if not lst:
        return "NULL"
    body = ", ".join("{ %d, %d, %d }" % t for t in lst)
    print("static const lo_stat_t %s[] = { %s };" % (name, body))
    return name


def cs(s):
    return "NULL" if s is None else '"%s"' % s


print("// 生成文件，勿手改：py -3 -X utf8 dm/ios/tools/gen_loadout.py <资源根目录> > dm/host/dm_loadout_tab.h")
print("// Generated, do not edit: py -3 -X utf8 dm/ios/tools/gen_loadout.py <resource root> > dm/host/dm_loadout_tab.h")
print("#pragma once")
rows = []
for i, p in enumerate(P):
    a = cstats("k_lo_st%d" % i, p["st"])
    b = cstats("k_lo_bst%d" % i, p["bst"])
    f = cstats("k_lo_fst%d" % i, p["fst"])
    uid = p["key"] if p["kind"] == "LK_UNIQUE" else -1
    rw = cs(p["key"]) if p["kind"] == "LK_RUNEWORD" else "NULL"
    fill = "{ " + ", ".join(cs(x) for x in p["fill"]) + " }" if p["fill"] else "{ 0 }"
    rows.append('    { "%s", %d, %d, %d, %s, %d, %d, %d, %d, %s, %s, %s, %s, %d, %s, %d, %s, %d, %d },' % (
        p["what"], p["slot"], p["set2"], p["merc"], p["kind"], p["q"], p["eth"], p["nsock"], uid, rw, cs(p["base"]), fill,
        a, len(p["st"]), b, len(p["bst"]), f, len(p["fst"]), p["clear"]))
print("static const lo_piece_t k_lo[] = {")
print("\n".join(rows))
print("};")
