#pragma once
#include <wx/wx.h>
#include <wx/vector.h>
#include <set>
#include <string>
#include "core/Component.h"
#include "file/NetlistExport.h"   // 复用 PartName / PinTypeOf / GatePinCount

// ================================================================
// 把电路导出成 KiCad 原理图(.kicad_sch)。
//
// 为什么要有这个:KiCad 6+ 的"正统"流程是
//   原理图 --(工具→从原理图更新PCB / F8)--> PCB
// 从文件导入网表虽然还在(文件→导入→网表),但原理图这条路更顺、
// 元件符号和连线都看得见,演示效果也更直观。
//
// 坐标:画布像素 → 毫米,比例 0.127(即 20px = 2.54mm,KiCad 标准栅格)。
// 精度:内部一律用"纳米整数"计算,导出时按 6 位小数打印,
//       保证"引脚位置"和"导线端点"在 KiCad 里是同一个点(否则连不上!)
// ================================================================

const double KICAD_NM_PER_PX = 127000.0;   // 1px = 0.127mm = 127000nm

// 原理图坐标(纳米整数),y 轴向下(和画布一致)
inline long long KicadX(long long px) { return px * 127000LL; }
inline long long KicadY(long long py) { return py * 127000LL; }

// 引脚相对元件中心的偏移(纳米,y 向下)
inline void KicadPinOffsetNm(const Component& c, int pin, long long* dx, long long* dy) {
    wxPoint p = GetPinPos(c, pin);
    *dx = (long long)(p.x - c.x) * 127000LL;
    *dy = (long long)(p.y - c.y) * 127000LL;
}

// 纳米整数 → 毫米字符串(6 位小数,恰好无损)
inline wxString NmToMm(long long nm) {
    return wxString::Format("%lld.%06lld", nm / 1000000, llabs(nm % 1000000));
}

// 生成一个稳定且合法的 UUID(演示用;真实软件一般用随机 UUID)
inline wxString FakeUuid(int a, int b) {
    return wxString::Format("%08x-1111-2222-3333-%012x", a, b);
}

inline wxString ExportKicadSchematic(const wxVector<Component>& comps, const wxVector<Wire>& wires)
{
    for(const auto& c:comps)if(c.type==GATE_MODULE) {
        wxVector<Component> flat;wxVector<Wire> flatW;
        if(!FlattenCircuit(comps,wires,flat,flatW))return {};
        return ExportKicadSchematic(flat,flatW);
    }
    const wxString rootUuid = "aaaaaaaa-bbbb-cccc-dddd-eeeeeeeeeeee";
    wxString s = "(kicad_sch\n";
    s += "\t(version 20231120)\n";
    s += "\t(generator \"eeschema\")\n";
    s += "\t(generator_version \"8.0\")\n";
    s += wxString::Format("\t(uuid \"%s\")\n", rootUuid);
    s += "\t(paper \"A4\")\n";

    // ---------- 内嵌元件库:每种类型一份符号定义(含引脚位置和电气类型) ----------
    s += "\t(lib_symbols\n";
    std::set<wxString> emitted;
    for (size_t i = 0; i < comps.size(); i++) {
        const Component& c = comps[i];
        wxString part = PartName(c);
        if (emitted.count(part)) continue;
        emitted.insert(part);

        // 外形框:把引脚范围往外扩 1.27mm
        long long minX = 0, maxX = 0, minY = 0, maxY = 0;
        for (int p = 0; p < GatePinCount(c); p++) {
            long long dx, dy;
            KicadPinOffsetNm(c, p, &dx, &dy);
            if (p == 0) { minX = maxX = dx; minY = maxY = dy; }
            minX = std::min(minX, dx); maxX = std::max(maxX, dx);
            minY = std::min(minY, dy); maxY = std::max(maxY, dy);
        }
        long long pad = 1270000LL;   // 1.27mm
        s += wxString::Format("\t\t(symbol \"MyEDA:%s\"\n", part);
        s += "\t\t\t(pin_names (offset 0))\n";
        s += "\t\t\t(in_bom yes)\n\t\t\t(on_board yes)\n";
        s += "\t\t\t(property \"Reference\" \"U\" (at 0 0 0) (effects (font (size 1.27 1.27))))\n";
        s += wxString::Format("\t\t\t(property \"Value\" \"%s\" (at 0 0 0) (effects (font (size 1.27 1.27))))\n", part);
        s += "\t\t\t(property \"Footprint\" \"\" (at 0 0 0) (effects (font (size 1.27 1.27)) hide))\n";
        s += "\t\t\t(property \"Datasheet\" \"~\" (at 0 0 0) (effects (font (size 1.27 1.27)) hide))\n";
        s += wxString::Format("\t\t\t(symbol \"%s_0_1\"\n", part);
        s += wxString::Format("\t\t\t\t(rectangle (start %s %s) (end %s %s)\n",
            NmToMm(minX - pad), NmToMm(-(maxY + pad)), NmToMm(maxX + pad), NmToMm(-(minY - pad)));
        s += "\t\t\t\t\t(stroke (width 0.254) (type default)) (fill (type background)))\n";
        s += "\t\t\t)\n";
        s += wxString::Format("\t\t\t(symbol \"%s_1_1\"\n", part);
        for (int p = 0; p < GatePinCount(c); p++) {
            long long dx, dy;
            KicadPinOffsetNm(c, p, &dx, &dy);
            // 符号库坐标系 y 向上 → 取负;length 0:连接点就在引脚位置上,不会和导线错开
            s += wxString::Format("\t\t\t\t(pin %s line (at %s %s 0) (length 0)\n",
                                  PinTypeOf(c, p), NmToMm(dx), NmToMm(-dy));
            s += "\t\t\t\t\t(name \"~\" (effects (font (size 1.27 1.27))))\n";
            s += wxString::Format("\t\t\t\t\t(number \"%d\" (effects (font (size 1.27 1.27)))))\n", p + 1);
        }
        s += "\t\t\t)\n\t\t)\n";
    }
    s += "\t)\n";

    // ---------- 元件实例 ----------
    for (size_t i = 0; i < comps.size(); i++) {
        const Component& c = comps[i];
        wxString part = PartName(c);
        long long px = KicadX(c.x), py = KicadY(c.y);
        s += "\t(symbol\n";
        s += wxString::Format("\t\t(lib_id \"MyEDA:%s\")\n", part);
        s += wxString::Format("\t\t(at %s %s 0)\n", NmToMm(px), NmToMm(py));
        s += "\t\t(unit 1)\n\t\t(exclude_from_sim no)\n\t\t(in_bom yes)\n\t\t(on_board yes)\n\t\t(dnp no)\n";
        s += wxString::Format("\t\t(uuid \"%s\")\n", FakeUuid(1000 + (int)i, (int)i));
        s += wxString::Format("\t\t(property \"Reference\" \"U%d\" (at %s %s 0) (effects (font (size 1.27 1.27))))\n",
                              c.id, NmToMm(px + 2540000LL), NmToMm(py));
        s += wxString::Format("\t\t(property \"Value\" \"%s\" (at %s %s 0) (effects (font (size 1.27 1.27))))\n",
                              part, NmToMm(px), NmToMm(py + 5080000LL));
        s += wxString::Format("\t\t(property \"Footprint\" \"\" (at %s %s 0) (effects (font (size 1.27 1.27)) hide))\n",
                              NmToMm(px), NmToMm(py));
        s += wxString::Format("\t\t(property \"Datasheet\" \"~\" (at %s %s 0) (effects (font (size 1.27 1.27)) hide))\n",
                              NmToMm(px), NmToMm(py));
        for (int p = 0; p < GatePinCount(c); p++)
            s += wxString::Format("\t\t(pin \"%d\" (uuid \"%s\"))\n", p + 1, FakeUuid(2000 + (int)i, p));
        s += "\t\t(instances\n\t\t\t(project \"\"\n";
        s += wxString::Format("\t\t\t\t(path \"/%s\"\n", rootUuid);
        s += wxString::Format("\t\t\t\t\t(reference \"U%d\") (unit 1))))\n", c.id);
        s += "\t)\n";
    }

    // ---------- 导线:端点用同样的整数纳米算,保证和引脚严丝合缝 ----------
    int wireNo = 0;
    for (size_t i = 0; i < wires.size(); i++) {
        const Component* a = nullptr;
        const Component* b = nullptr;
        for (size_t k = 0; k < comps.size(); k++) {
            if (comps[k].id == wires[i].comp1) a = &comps[k];
            if (comps[k].id == wires[i].comp2) b = &comps[k];
        }
        if (!a || !b) continue;
        long long ax, ay, bx, by;
        KicadPinOffsetNm(*a, wires[i].pin1, &ax, &ay);
        KicadPinOffsetNm(*b, wires[i].pin2, &bx, &by);
        s += "\t(wire\n";
        s += wxString::Format("\t\t(pts (xy %s %s) (xy %s %s))\n",
                              NmToMm(KicadX(a->x) + ax), NmToMm(KicadY(a->y) + ay),
                              NmToMm(KicadX(b->x) + bx), NmToMm(KicadY(b->y) + by));
        s += "\t\t(stroke (width 0) (type default))\n";
        s += wxString::Format("\t\t(uuid \"%s\"))\n", FakeUuid(3000 + wireNo, wireNo));
        wireNo++;
    }

    s += "\t(sheet_instances\n\t\t(path \"/\" (page \"1\")))\n";
    s += ")\n";
    return s;
}
