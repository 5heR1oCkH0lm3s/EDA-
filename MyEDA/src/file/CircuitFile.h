#pragma once
#include <wx/wx.h>
#include <wx/vector.h>
#include <wx/file.h>
#include <fstream>
#include <regex>
#include <string>
#include "core/Component.h"

// ================================================================
// 电路的保存与加载(JSON 文本格式),与界面完全解耦——
// 只有函数,不碰任何窗口,以后网表导出、仿真也都照这个思路分层
// ================================================================

// 元件类型 ↔ 名字(存进文件的是名字,人能看懂)
inline wxString GateTypeName(GateType t) {
    if (t == GATE_AND) return "AND";
    if (t == GATE_OR)  return "OR";
    if (t == GATE_NOT) return "NOT";
    if (t == GATE_XOR) return "XOR";
    if (t == GATE_NAND) return "NAND";
    if (t == GATE_NOR) return "NOR";
    if (t == GATE_CUSTOM) return "CUSTOM";
    if (t == GATE_DFF) return "DFF";   // 自定义元件:名字另存在 name 字段
    if (t == SW_INPUT) return "SW";
    if (t == SW_CLOCK) return "CLK";
    return "LED";
}
inline GateType GateTypeFromName(const std::string& s) {
    if (s == "OR")  return GATE_OR;
    if (s == "NOT") return GATE_NOT;
    if (s == "XOR") return GATE_XOR;
    if (s == "NAND") return GATE_NAND;
    if (s == "NOR") return GATE_NOR;
    if (s == "CUSTOM") return GATE_CUSTOM;
    if (s == "DFF") return GATE_DFF;
    if (s == "SW")  return SW_INPUT;
    if (s == "CLK") return SW_CLOCK;
    if (s == "LED") return SW_LED;
    return GATE_AND;
}

// 把整份电路拼成 JSON 文本。
// 格式是我们自己定的,结构固定:一个 components 数组 + 一个 wires 数组
inline wxString CircuitToJson(const wxVector<Component>& comps, const wxVector<Wire>& wires,
                              const wxVector<CustomDef>& defs = wxVector<CustomDef>()) {
    wxString s = "{\n  \"components\": [\n";
    for (size_t i = 0; i < comps.size(); i++) {
        const Component& c = comps[i];
        s += wxString::Format("    {\"id\":%d,\"type\":\"%s\",\"x\":%d,\"y\":%d,\"state\":%d,\"name\":\"%s\",\"truth\":%d}%s\n",
                              c.id, GateTypeName(c.type), c.x, c.y, c.state ? 1 : 0,
                              c.customName, c.truth,
                              (i + 1 < comps.size()) ? "," : "");
    }
    s += "  ],\n  \"wires\": [\n";
    for (size_t i = 0; i < wires.size(); i++) {
        const Wire& w = wires[i];
        s += wxString::Format("    [%d,%d,%d,%d]%s\n",
                              w.comp1, w.pin1, w.comp2, w.pin2,
                              (i + 1 < wires.size()) ? "," : "");
    }
    s += "  ],\n";
    // Keep the legacy four-number wires array; optional drawing routes are separate.
    s += "  \"wireRoutes\": [\n";
    bool firstRoute = true;
    for (size_t i = 0; i < wires.size(); ++i) {
        if (wires[i].bends.empty()) continue;
        if (!firstRoute) s += ",\n";
        firstRoute = false;
        s += wxString::Format("    {\"wire\":%d,\"points\":[", (int)i);
        for (size_t j = 0; j < wires[i].bends.size(); ++j) {
            if (j) s += ",";
            s += wxString::Format("[%d,%d]", wires[i].bends[j].x, wires[i].bends[j].y);
        }
        s += "]}";
    }
    s += "\n  ],\n";
    // 自定义元件库也一起存:否则存盘重开后,已定义的自定义元件就再也改不了了
    s += "  \"customGates\": [\n";
    for (size_t i = 0; i < defs.size(); i++)
        s += wxString::Format("    {\"name\":\"%s\",\"truth\":%d}%s\n",
                              defs[i].name, defs[i].truth, (i + 1 < defs.size()) ? "," : "");
    s += "  ]\n}\n";
    return s;
}

// 解析 JSON:结构固定,用正则表达式把每个元件/连线"抓"出来即可。
// (真实项目会用现成 JSON 库如 nlohmann/json;这里手写最简版便于教学)
inline bool JsonToCircuit(const std::string& text,
                          wxVector<Component>& comps, wxVector<Wire>& wires, int& nextId,
                          wxVector<CustomDef>* defsOut = nullptr) {
    comps.clear();
    wires.clear();

    // name/truth 是后加的字段,用可选组 (…)? 让旧存档也能读——
    // 工程惯例:写文件严格、读文件宽容(否则老版本存的文件全打不开)
    std::regex compRe("\\{\"id\":(\\d+),\"type\":\"(AND|OR|NOT|XOR|NAND|NOR|CUSTOM|DFF|CLK|SW|LED)\",\"x\":(-?\\d+),\"y\":(-?\\d+),\"state\":(\\d)(,\"name\":\"([^\"]*)\",\"truth\":(\\d+))?\\}");
    std::regex wireRe("\\[(\\d+),(\\d+),(\\d+),(\\d+)\\]");
    auto end = std::sregex_iterator();

    int maxId = 0;
    for (auto it = std::sregex_iterator(text.begin(), text.end(), compRe); it != end; ++it) {
        Component c;
        c.id   = std::stoi((*it)[1]);
        c.type = GateTypeFromName((*it)[2]);
        c.x    = std::stoi((*it)[3]);
        c.y    = std::stoi((*it)[4]);
        c.state = ((*it)[5] == "1");
        c.customName = (*it)[7].matched ? wxString::FromUTF8(std::string((*it)[7]).c_str()) : wxString();
        c.truth = (*it)[8].matched ? std::stoi((*it)[8]) : 0;
        comps.push_back(c);
        if (c.id > maxId) maxId = c.id;
    }
    for (auto it = std::sregex_iterator(text.begin(), text.end(), wireRe); it != end; ++it) {
        Wire w;
        w.comp1 = std::stoi((*it)[1]);
        w.pin1  = std::stoi((*it)[2]);
        w.comp2 = std::stoi((*it)[3]);
        w.pin2  = std::stoi((*it)[4]);
        wires.push_back(w);
    }
    std::regex routeRe(R"route(\{"wire":(\d+),"points":\[((?:\[-?\d+,-?\d+\],?)*)\]\})route");
    std::regex pointRe(R"point(\[(-?\d+),(-?\d+)\])point");
    for (auto it = std::sregex_iterator(text.begin(), text.end(), routeRe); it != end; ++it) {
        const size_t index = (size_t)std::stoul((*it)[1]);
        if (index >= wires.size()) continue;
        const std::string points = (*it)[2];
        for (auto pt = std::sregex_iterator(points.begin(), points.end(), pointRe); pt != end; ++pt)
            wires[index].bends.push_back(SnapGrid(wxPoint(std::stoi((*pt)[1]), std::stoi((*pt)[2]))));
    }
    nextId = maxId + 1;   // 新元件从最大编号+1 开始,保证编号不重复

    // 自定义元件库:这里的 "name" 紧跟在 { 后面,不会和元件里的 "name" 字段混淆
    if (defsOut) {
        std::regex defRe("\\{\"name\":\"([^\"]*)\",\"truth\":(\\d+)\\}");
        for (auto it = std::sregex_iterator(text.begin(), text.end(), defRe); it != end; ++it) {
            CustomDef d;
            d.name = wxString::FromUTF8(std::string((*it)[1]).c_str());
            d.truth = std::stoi((*it)[2]);
            defsOut->push_back(d);
        }
    }
    return true;
}

// 写文件
inline bool SaveCircuit(const wxString& path,
                        const wxVector<Component>& comps, const wxVector<Wire>& wires,
                        const wxVector<CustomDef>& defs = wxVector<CustomDef>()) {
    wxFile f;
    if (!f.Open(path, wxFile::write))
        return false;                       // 打不开(路径不对/没权限)
    wxString json = CircuitToJson(comps, wires, defs);
    f.Write(json, wxConvUTF8);              // JSON 里全是英文,UTF-8 最保险
    f.Close();
    return true;
}

// 读文件
inline bool LoadCircuit(const wxString& path,
                        wxVector<Component>& comps, wxVector<Wire>& wires, int& nextId,
                        wxVector<CustomDef>* defsOut = nullptr) {
    wxFile f;
    if (!f.Open(path, wxFile::read))
        return false;
    wxString content;
    if (!f.ReadAll(&content, wxConvUTF8))
        return false;
    f.Close();
    JsonToCircuit(std::string((const char*)content.utf8_str()), comps, wires, nextId, defsOut);
    return true;
}
