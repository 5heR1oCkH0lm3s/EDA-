#pragma once
#include <wx/wx.h>
#include <wx/vector.h>
#include <wx/file.h>
#include <algorithm>
#include <map>
#include <set>
#include <cctype>
#include <string>
#include <regex>
#include <utility>
#include <vector>
#include "core/Component.h"
#include "core/CircuitValidation.h"
#include "core/Subcircuit.h"

// ================================================================
// 网表 = 电路的"电气描述":有哪些元件、哪些引脚彼此相连。
// PCB 软件(KiCad)读进网表,就知道该把哪些焊盘连在一起。
//
// 格式对齐:本文件按 KiCad 10 实际导出的网表形状生成
// (用 `kicad-cli sch export netlist --format kicadsexpr` 导出一份参考网表逐字段比对过):
//   · 元件带 fields/property/tstamps/units 段
//   · 网络带 (class "Default"),节点带 (pintype "input"/"output")
//   · 引脚编号从 1 开始(和 PCB 焊盘编号一致)
//   · 网络名用 KiCad 风格 "Net-(U1-Pad2)"
// ================================================================

// 引脚电气类型:告诉 PCB 软件这个脚是输入还是输出
inline wxString PinTypeOf(const Component& c, int pin) {
    if (c.type == GATE_MODULE) return IsOutputPin(c,pin)?"output":"input";
    if (c.type == SW_INPUT || c.type==SW_CLOCK) return "output";
    if (c.type == SW_LED)   return "input";
    if (c.type == GATE_NOT) return (pin == 0) ? "input" : "output";
    return (pin == 2) ? "output" : "input";   // 多输入门:0/1 输入,2 输出
}

// 元件的默认封装(PCB 上的实际焊盘图形)。
// 为什么要写:KiCad 导入网表时,没有封装的元件会报"无法添加 XXX(没有分配封装)"
// 而进不了 PCB。这里给出可用的默认封装,演示时能一路走通;
// 真实项目应当由用户按实际元器件选封装(在 KiCad 里改即可,库名都是 KiCad 标准库)
inline wxString DefaultFootprint(const Component& c) {
    if (c.type == SW_INPUT) return "Button_Switch_THT:SW_PUSH_6mm";   // 轻触按键(2 脚)
    if (c.type == SW_LED)   return "LED_THT:LED_D5.0mm";              // 5mm 直插 LED(2 脚)
    return "Package_DIP:DIP-14_W7.62mm";                             // 逻辑门按 14 脚 DIP(类 74 系列)
}

// 元件在网表里的"型号名":普通门用类型名,自定义元件用它自己的名字
// (真实 EDA 也是这样:网表只引用元件名,具体行为由元件库提供)
inline wxString PartName(const Component& c) {
    if (c.type == GATE_CUSTOM && !c.customName.empty())
        return c.customName;
    return GateTypeName(c.type);
}

// 把画布上的"一根根连线"归并成"电气网络"。
// 为什么要归并:一个输出常常要接多个输入(叫"扇出"),比如半加器里
// 开关 SW_A 同时接到 XOR 和 AND —— 画了 2 根线,但电气上是同一个网络。
// 如果不合并,同一个引脚会出现在两个网络里,PCB 软件会当成冲突/短路报错。
// 算法:并查集(union-find) —— 每根连线把两端"合并到同一组",
//       最后同一组的引脚就是一个网络。
inline std::vector<std::vector<std::pair<int,int>>> BuildNets(const wxVector<Wire>& wires)
{
    typedef std::pair<int,int> Pin;      // (元件编号, 引脚号)
    std::map<Pin, Pin> parent;           // 并查集:引脚 → 它的"上级"

    // 第 1 步:每个引脚先自成一派
    for (size_t i = 0; i < wires.size(); i++) {
        Pin a(wires[i].comp1, wires[i].pin1);
        Pin b(wires[i].comp2, wires[i].pin2);
        parent[a] = a;
        parent[b] = b;
    }
    // 找"族长"(顺手做路径压缩,让下次查找更快)
    auto findRoot = [&parent](Pin p) {
        Pin root = p;
        while (parent[root] != root) root = parent[root];
        while (parent[p] != root) { Pin next = parent[p]; parent[p] = root; p = next; }
        return root;
    };
    // 第 2 步:每根连线把两端并成一家
    for (size_t i = 0; i < wires.size(); i++) {
        Pin ra = findRoot(Pin(wires[i].comp1, wires[i].pin1));
        Pin rb = findRoot(Pin(wires[i].comp2, wires[i].pin2));
        if (ra != rb) parent[ra] = rb;
    }
    // 第 3 步:同一个族长的引脚归为一个网络
    std::map<Pin, std::vector<Pin>> groups;
    for (std::map<Pin, Pin>::iterator it = parent.begin(); it != parent.end(); ++it)
        groups[findRoot(it->first)].push_back(it->first);

    std::vector<std::vector<Pin>> nets;
    for (std::map<Pin, std::vector<Pin>>::iterator it = groups.begin(); it != groups.end(); ++it) {
        std::vector<Pin> nodes = it->second;
        std::sort(nodes.begin(), nodes.end());   // 排序 → 输出稳定,便于往返比对
        nets.push_back(nodes);
    }
    std::sort(nets.begin(), nets.end());
    return nets;
}

// 把整份电路导出为 KiCad 网表文本
inline wxString ExportKiCadNetlist(const wxVector<Component>& comps, const wxVector<Wire>& wires) {
    std::map<int, const Component*> byId;
    for (size_t i = 0; i < comps.size(); i++)
        byId[comps[i].id] = &comps[i];

    for(const auto& c:comps)if(c.type==GATE_MODULE) {
        wxVector<Component> flat;wxVector<Wire> flatW;
        if(!FlattenCircuit(comps,wires,flat,flatW))return {};
        return ExportKiCadNetlist(flat,flatW);
    }
    wxString s = "(export (version \"E\")\n";
    s += "  (design\n";
    s += "    (source \"MyEDA schematic\")\n";
    s += "    (date \"2026-09-17\")\n";
    s += "    (tool \"MyEDA 0.1\")\n";
    s += "    (sheet (number \"1\") (name \"/\") (tstamps \"/\"))\n";
    s += "  )\n";

    // ---------- 元件清单 ----------
    s += "  (components\n";
    for (size_t i = 0; i < comps.size(); i++) {
        const Component& c = comps[i];
        s += wxString::Format("    (comp (ref \"U%d\")\n", c.id);
        s += wxString::Format("      (value \"%s\")\n", PartName(c));
        // 这个紧跟 value 的 (footprint ...) 才是 KiCad 导入时读取封装的字段
        // (KiCad 自己导出的网表里也有它,和下面 fields 里的那份是重复写两遍)
        s += wxString::Format("      (footprint \"%s\")\n", DefaultFootprint(c));
        s += "      (fields\n";
        s += wxString::Format("        (field (name \"Footprint\") \"%s\")\n", DefaultFootprint(c));
        s += "        (field (name \"Datasheet\"))\n";
        s += "      )\n";
        s += wxString::Format("      (libsource (lib \"MyEDA\") (part \"%s\") (description \"\"))\n",
                              PartName(c));
        s += "      (property (name \"Sheetname\") (value \"Root\"))\n";
        s += "      (property (name \"Sheetfile\") (value \"MyEDA\"))\n";
        s += "      (sheetpath (names \"/\") (tstamps \"/\"))\n";
        s += wxString::Format("      (tstamps \"U%d\")\n", c.id);
        s += "      (units (unit (name \"A\") (pins\n";
        for (int p = 0; p < GatePinCount(c); p++)
            s += wxString::Format("        (pin (num \"%d\"))\n", p + 1);   // 引脚从 1 编号
        s += "      )))\n";
        s += "    )\n";
    }
    s += "  )\n";

    // ---------- 元件库定义:每种型号一段(引脚类型告诉 PCB 软件脚的电气方向) ----------
    s += "  (libparts\n";
    std::set<wxString> emitted;
    for (size_t i = 0; i < comps.size(); i++) {
        const Component& c = comps[i];
        wxString part = PartName(c);
        if (emitted.count(part)) continue;
        emitted.insert(part);
        s += wxString::Format("    (libpart (lib \"MyEDA\") (part \"%s\")\n", part);
        s += "      (fields (field (name \"Reference\") \"U\") ";
        s += wxString::Format("(field (name \"Value\") \"%s\") ", part);
        s += wxString::Format("(field (name \"Footprint\") \"%s\"))\n", DefaultFootprint(c));
        s += "      (pins\n";
        for (int p = 0; p < GatePinCount(c); p++)
            s += wxString::Format("        (pin (num \"%d\") (name \"\") (type \"%s\"))\n",
                                  p + 1, PinTypeOf(c, p));
        s += "      )\n    )\n";
    }
    s += "  )\n";
    s += "  (libraries)\n";

    // ---------- 网络:电气上连通的一组引脚 = 一个网络 ----------
    s += "  (nets\n";
    std::vector<std::vector<std::pair<int,int>>> nets = BuildNets(wires);
    for (size_t i = 0; i < nets.size(); i++) {
        const std::vector<std::pair<int,int>>& nodes = nets[i];
        if (nodes.empty()) continue;
        // 网络名沿用 KiCad 习惯:Net-(U1-Pad2) —— 一个引脚只属于一个网络,名字天然唯一
        s += wxString::Format("    (net (code \"%d\") (name \"Net-(U%d-Pad%d)\") (class \"Default\")\n",
                              (int)i + 1, nodes[0].first, nodes[0].second + 1);
        for (size_t k = 0; k < nodes.size(); k++) {
            const Component* c = byId.count(nodes[k].first) ? byId[nodes[k].first] : nullptr;
            wxString pt = c ? PinTypeOf(*c, nodes[k].second) : "passive";
            s += wxString::Format("      (node (ref \"U%d\") (pin \"%d\") (pintype \"%s\"))\n",
                                  nodes[k].first, nodes[k].second + 1, pt);
        }
        s += "    )\n";
    }
    s += "  )\n";
    s += ")\n";
    return s;
}

// 生成并写文件
inline bool SaveNetlist(const wxString& path,
                        const wxVector<Component>& comps, const wxVector<Wire>& wires) {
    const auto text=ExportKiCadNetlist(comps,wires);
    if(text.empty())return false;
    wxFile f;
    if(!f.Open(path,wxFile::write))return false;
    const bool written=f.Write(text,wxConvUTF8);
    return f.Close() && written;
}

// ================================================================
// 导入:解析 KiCad 网表,反向重建电路。
//
// 兼容性(踩过的坑,别改回去):
//   · 用 \s+ 而不是空格:KiCad 导出的网表每个字段单独一行,写死空格会解析失败
//   · 元件编号前缀不写死 U:真实网表可能是 R1、C3、U2 混排
//   · 引脚号从 1 开始,读回来要减 1 变成我们内部的 0 起编号
//
// 说明:网表只含"电气信息"(元件型号/编号、引脚连接),不含坐标和开关状态,
// 所以导入后的元件按网格自动排布、开关全为"关"——这是网表格式的天然限制
// ================================================================
inline bool ParseKiCadNetlistUnchecked(const wxString& text,
                              wxVector<Component>& comps, wxVector<Wire>& wires) {
    comps.clear();
    wires.clear();

    std::string t = std::string((const char*)text.utf8_str());
    std::regex compRe("\\(comp\\s+\\(ref\\s+\"([A-Za-z_]*)(\\d+)\"\\)\\s+\\(value\\s+\"([^\"]+)\"\\)");
    std::regex nodeRe("\\(node\\s+\\(ref\\s+\"([A-Za-z_]*)(\\d+)\"\\)\\s+\\(pin\\s+\"(\\d+)\"\\)");
    auto end = std::sregex_iterator();

    // 元件:ref 还原编号、value 还原型号;坐标按导入顺序排成网格。
    // value 不是标准门名(AND/OR/...)= 自定义元件(真值表在库里,网表不含行为,所以默认全 0)
    std::map<std::pair<std::string,int>, int> refToId;   // (前缀,编号) → 内部编号
    std::set<int> usedIds;
    int idx = 0;
    for (auto it = std::sregex_iterator(t.begin(), t.end(), compRe); it != end; ++it) {
        std::string prefix = (*it)[1];
        int num = std::stoi((*it)[2]);
        std::string val = (*it)[3];

        // 编号优先沿用网表里的数字(我们自己导出的就是 U1、U2...);
        // 撞号时(不同前缀可能同号,如 R1 和 C1)另找一个空号
        if (refToId.count(std::make_pair(prefix,num))) return false;
        int id = num;
        if (usedIds.count(id)) {
            id = 1;
            while (usedIds.count(id)) id++;
        }
        usedIds.insert(id);
        refToId[std::make_pair(prefix, num)] = id;

        Component c;
        c.id = id;
        c.type = GateTypeFromName(val);          // 不认识的名字会落到 AND,下面纠正
        if (val != "AND" && val != "OR" && val != "NOT" && val != "XOR" &&
            val != "NAND" && val != "NOR" && val != "SW" && val != "LED" && val != "DFF" && val != "CLK") {
            c.type = GATE_CUSTOM;
            c.customName = wxString::FromUTF8(val.c_str());
            c.truth = 0;   // 网表里没有真值表,导入后需在元件库里补行为
        }
        c.x = 180 + (idx % 3) * 230;
        c.y = 160 + (idx / 3) * 170;
        comps.push_back(c);
        idx++;
    }

    // 网络:先按括号配对切出每个 (net ...) 块,块内节点依次"链式重连"成线。
    // 一个网络有 N 个节点 → 重连成 N-1 根线,电气上完全等价。
    // 注意两件事:
    //   ① 找块头要容忍 (net 后面跟空格或换行 —— KiCad 导出的是换行),同时别把 (nets 段名当块头
    //   ② 必须"块内配对":把所有节点拉平再两两配对,会把上一个网络的节点和
    //      下一个网络的节点连到一起(曾经的真实 bug)
    size_t pos = 0;
    while (true) {
        pos = t.find("(net", pos);
        if (pos == std::string::npos) break;
        size_t after = pos + 4;
        if (after < t.size() && !std::isspace((unsigned char)t[after])) { pos = after; continue; }

        int depth = 0;
        size_t i = pos;
        for (; i < t.size(); i++) {
            if (t[i] == '(') depth++;
            else if (t[i] == ')') { depth--; if (depth == 0) break; }
        }
        std::string block = t.substr(pos, (i < t.size()) ? (i - pos + 1) : std::string::npos);

        std::vector<std::pair<int,int>> pins;
        for (auto it = std::sregex_iterator(block.begin(), block.end(), nodeRe); it != end; ++it) {
            std::string prefix = (*it)[1];
            int num = std::stoi((*it)[2]);
            int pin1based = std::stoi((*it)[3]);
            std::map<std::pair<std::string,int>, int>::iterator f =
                refToId.find(std::make_pair(prefix, num));
            if (f == refToId.end()) return false; // Missing referenced component.
            pins.push_back(std::make_pair(f->second, pin1based - 1));   // 1 起编号 → 0 起
        }
        for (size_t k = 0; k + 1 < pins.size(); k++)
            wires.push_back({ pins[k].first, pins[k].second,
                              pins[k+1].first, pins[k+1].second });

        pos = (i < t.size()) ? i : t.size();
    }
    return true;
}


// Validate the document before extraction, then commit the whole parsed circuit.
inline bool ParseKiCadNetlist(const wxString& text,wxVector<Component>& comps,wxVector<Wire>& wires) {
    try {
        const std::string data(text.utf8_str());
        if(data.size()>10*1024*1024 || !std::regex_search(data,std::regex("^\\s*\\(export[\\s(]"))) return false;
        int depth=0;bool quoted=false,escaped=false,ended=false;
        for(unsigned char c:data) {
            if(ended && !std::isspace(c))return false;
            if(quoted) { if(escaped)escaped=false;else if(c=='\\')escaped=true;else if(c=='"')quoted=false;continue; }
            if(c=='"'){quoted=true;continue;}
            if(c=='(')++depth;
            else if(c==')'){if(--depth<0)return false;if(!depth)ended=true;}
        }
        if(depth || quoted || !ended)return false;
        wxVector<Component> loaded;wxVector<Wire> raw;
        if(!ParseKiCadNetlistUnchecked(text,loaded,raw) || loaded.empty())return false;
        for(const auto& c:loaded) if(c.id<=0 || c.id==INT_MAX)return false;
        for(const auto& w:raw) {
            const auto a=CircuitComponent(loaded,w.comp1),b=CircuitComponent(loaded,w.comp2);
            if(!a || !b || w.pin1<0 || w.pin2<0 || w.pin1>=GatePinCount(*a) || w.pin2>=GatePinCount(*b))return false;
        }
        wxVector<Wire> result;
        for(const auto& net:BuildNets(raw)) {
            int drivers=0;std::pair<int,int> source;
            for(const auto& node:net) if(IsOutputPin(*CircuitComponent(loaded,node.first),node.second)){source=node;++drivers;}
            if(drivers>1)return false;
            if(drivers==1) {
                for(const auto& target:net) if(target!=source) {
                    Wire w={source.first,source.second,target.first,target.second};
                    if(!NormalizeConnection(loaded,result,w))return false;
                    result.push_back(w);
                }
            } else {
                // Passive reference components can be inspected, without inventing a driver.
                for(size_t i=1;i<net.size();++i)result.push_back({net[i-1].first,net[i-1].second,net[i].first,net[i].second});
            }
        }
        comps=loaded;wires=result;return true;
    } catch(const std::exception&) {return false;}
}

// 读文件并解析
inline bool LoadNetlist(const wxString& path,
                        wxVector<Component>& comps, wxVector<Wire>& wires) {
    wxFile f;
    if (!f.Open(path, wxFile::read))
        return false;
    wxString content;
    if (!f.ReadAll(&content, wxConvUTF8))
        return false;
    f.Close();
    return ParseKiCadNetlist(content, comps, wires);
}
