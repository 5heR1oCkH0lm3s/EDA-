#pragma once
#include <wx/wx.h>
#include <wx/vector.h>
#include <wx/file.h>
#include <fstream>
#include "file/Json.h"
#include "core/CircuitValidation.h"
#include "core/Subcircuit.h"
#include <limits>
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
    if (t == GATE_MODULE) return "MODULE";
    if (t == GATE_CUSTOM) return "CUSTOM";   // 自定义元件:名字另存在 name 字段
    if (t == SW_INPUT) return "SW";
    return "LED";
}
inline GateType GateTypeFromName(const std::string& s) {
    if (s == "OR")  return GATE_OR;
    if (s == "NOT") return GATE_NOT;
    if (s == "XOR") return GATE_XOR;
    if (s == "NAND") return GATE_NAND;
    if (s == "NOR") return GATE_NOR;
    if (s == "MODULE") return GATE_MODULE;
    if (s == "CUSTOM") return GATE_CUSTOM;
    if (s == "SW")  return SW_INPUT;
    if (s == "LED") return SW_LED;
    return GATE_AND;
}


inline wxString JsonEscape(const wxString& name) {
    const std::string utf8(name.utf8_str()); std::string escaped;
    const char* hex="0123456789abcdef";
    for(unsigned char c:utf8) {
        if(c=='"' || c=='\\'){escaped+='\\';escaped+=(char)c;}
        else if(c<32){escaped+="\\u00";escaped+=hex[c>>4];escaped+=hex[c&15];}
        else escaped+=(char)c;
    }
    return wxString::FromUTF8(escaped.c_str());
}


inline wxString SubcircuitToJson(const SubcircuitDef& d);
inline ModulePtr SubcircuitFromJson(const ProjectJson::Value& value,int depth);
inline bool JsonToCircuit(const std::string&,wxVector<Component>&,wxVector<Wire>&,int&,wxVector<CustomDef>*,ModuleLibrary*,int);

inline wxString CircuitToJson(const wxVector<Component>& comps,const wxVector<Wire>& wires,
    const wxVector<CustomDef>& defs=wxVector<CustomDef>(),const ModuleLibrary& modules=ModuleLibrary()) {
    wxString s="{\n  \"components\": [\n";
    for(size_t i=0;i<comps.size();++i) {
        const auto& c=comps[i];
        s+=wxString::Format("    {\"id\":%d,\"type\":\"%s\",\"x\":%d,\"y\":%d,\"state\":%d,\"name\":\"%s\",\"truth\":%d",
            c.id,GateTypeName(c.type),c.x,c.y,c.state?1:0,JsonEscape(c.customName),c.truth);
        if(c.type==GATE_MODULE && c.module)s+=",\"module\":"+SubcircuitToJson(*c.module);
        s+=wxString::Format("}%s\n",i+1<comps.size()?",":"");
    }
    s+="  ],\n  \"wires\": [\n";
    for(size_t i=0;i<wires.size();++i) {
        const auto& w=wires[i];s+=wxString::Format("    [%d,%d,%d,%d]%s\n",w.comp1,w.pin1,w.comp2,w.pin2,i+1<wires.size()?",":"");
    }
    s+="  ],\n  \"wireRoutes\": [\n";bool first=true;
    for(size_t i=0;i<wires.size();++i) {
        if(wires[i].bends.empty())continue;
        if(!first)s+=",\n";first=false;
        s+=wxString::Format("    {\"wire\":%d,\"points\":[",(int)i);
        for(size_t j=0;j<wires[i].bends.size();++j){if(j)s+=",";s+=wxString::Format("[%d,%d]",wires[i].bends[j].x,wires[i].bends[j].y);}
        s+="]}";
    }
    s+="\n  ],\n  \"customGates\": [\n";
    for(size_t i=0;i<defs.size();++i)s+=wxString::Format("    {\"name\":\"%s\",\"truth\":%d}%s\n",JsonEscape(defs[i].name),defs[i].truth,i+1<defs.size()?",":"");
    s+="  ],\n  \"moduleLibrary\": [\n";
    for(size_t i=0;i<modules.size();++i){if(i)s+=",\n";s+=SubcircuitToJson(*modules[i]);}
    s+="\n  ]\n}\n";return s;
}
inline wxString SubcircuitToJson(const SubcircuitDef& d) {
    wxString s="{\"name\":\""+JsonEscape(d.name)+"\",\"inputs\":[";
    for(size_t i=0;i<d.inputIds.size();++i){if(i)s+=",";s+=wxString::Format("{\"id\":%d,\"name\":\"%s\"}",d.inputIds[i],JsonEscape(d.inputNames[i]));}
    s+="],\"outputs\":[";
    for(size_t i=0;i<d.outputIds.size();++i){if(i)s+=",";s+=wxString::Format("{\"id\":%d,\"name\":\"%s\"}",d.outputIds[i],JsonEscape(d.outputNames[i]));}
    return s+"],\"circuit\":"+CircuitToJson(d.components,d.wires)+"}";
}

// 解析 JSON:结构固定,用正则表达式把每个元件/连线"抓"出来即可。
// (真实项目会用现成 JSON 库如 nlohmann/json;这里手写最简版便于教学)
inline bool JsonToCircuit(const std::string& text,
                          wxVector<Component>& comps, wxVector<Wire>& wires, int& nextId,
                          wxVector<CustomDef>* defsOut = nullptr, ModuleLibrary* modulesOut = nullptr, int nesting = 0) {
    try {
        if(nesting>8)return false;
        const auto root=ProjectJson::Reader(text).parse();
        wxVector<Component> loaded; wxVector<Wire> loadedWires; wxVector<CustomDef> definitions;
        const auto& componentArray=root.at("components").array();
        const auto& wireArray=root.at("wires").array();
        if(componentArray.size()>10000 || wireArray.size()>100000) return false;
        std::set<int> ids; int maxId=0;
        for(const auto& value:componentArray) {
            Component c; c.id=value.at("id").integer();
            if(c.id<=0 || c.id==INT_MAX || !ids.insert(c.id).second) return false;
            const auto& type=value.at("type").string();
            const std::set<std::string> types={"AND","OR","NOT","XOR","NAND","NOR","CUSTOM","SW","LED","MODULE"};
            if(!types.count(type)) return false;
            c.type=GateTypeFromName(type);c.x=value.at("x").integer();c.y=value.at("y").integer();
            if(c.x < -1000000 || c.x > 1000000 || c.y < -1000000 || c.y > 1000000) return false;
            const auto& state=value.at("state");
            if(state.kind!=ProjectJson::Value::Boolean && state.kind!=ProjectJson::Value::Number) return false;
            if(state.number!=0 && state.number!=1) return false;
            c.state=state.number!=0;
            if(value.has("name")) c.customName=wxString::FromUTF8(value.at("name").string().c_str());
            if(value.has("truth")) c.truth=value.at("truth").integer();
            if(c.truth<0 || c.truth>15) return false;
            if(c.type==GATE_MODULE) {
                c.module=SubcircuitFromJson(value.at("module"),nesting+1);
                if(!c.module)return false;
                c.customName=c.module->name;
            }
            loaded.push_back(c);maxId=std::max(maxId,c.id);
        }
        for(const auto& value:wireArray) {
            const auto& numbers=value.array();if(numbers.size()!=4) return false;
            Wire w={numbers[0].integer(),numbers[1].integer(),numbers[2].integer(),numbers[3].integer()};
            loadedWires.push_back(w);
        }
        if(root.has("wireRoutes")) {
            std::set<int> routed;
            for(const auto& value:root.at("wireRoutes").array()) {
                const int index=value.at("wire").integer();
                if(index<0 || (size_t)index>=loadedWires.size() || !routed.insert(index).second) return false;
                if(value.at("points").array().size()>10000) return false;
                for(const auto& point:value.at("points").array()) {
                    const auto& xy=point.array();if(xy.size()!=2) return false;
                    const int x=xy[0].integer(),y=xy[1].integer();
                    if(x < -1000000 || x > 1000000 || y < -1000000 || y > 1000000) return false;
                    loadedWires[index].bends.push_back(SnapGrid(wxPoint(x,y)));
                }
            }
        }
        wxVector<Wire> normalized;
        for(auto w:loadedWires) {
            if(!NormalizeConnection(loaded,normalized,w)) return false;
            normalized.push_back(w);
        }
        if(root.has("customGates")) {
            std::set<wxString> names;
            for(const auto& value:root.at("customGates").array()) {
                CustomDef d;d.name=wxString::FromUTF8(value.at("name").string().c_str());d.truth=value.at("truth").integer();
                if(d.name.empty() || d.truth<0 || d.truth>15 || !names.insert(d.name).second) return false;
                definitions.push_back(d);
            }
        }
        ModuleLibrary modules;
        if(root.has("moduleLibrary")) {
            std::set<wxString> names;
            if(root.at("moduleLibrary").array().size()>256)return false;
            for(const auto& value:root.at("moduleLibrary").array()) {
                auto d=SubcircuitFromJson(value,nesting+1);
                if(!d || !names.insert(d->name).second)return false;
                modules.push_back(d);
            }
        }
        comps=loaded;wires=normalized;nextId=maxId+1;
        if(modulesOut)*modulesOut=modules;
        if(defsOut) *defsOut=definitions;
        return true;
    } catch(const std::exception&) { return false; }
}


inline ModulePtr SubcircuitFromJson(const ProjectJson::Value& value,int depth) {
    if(depth>8)return {};
    auto d=std::make_shared<SubcircuitDef>();d->name=wxString::FromUTF8(value.at("name").string().c_str());
    for(const auto& p:value.at("inputs").array()){d->inputIds.push_back(p.at("id").integer());d->inputNames.push_back(wxString::FromUTF8(p.at("name").string().c_str()));}
    for(const auto& p:value.at("outputs").array()){d->outputIds.push_back(p.at("id").integer());d->outputNames.push_back(wxString::FromUTF8(p.at("name").string().c_str()));}
    int next=1;
    if(!JsonToCircuit(ProjectJson::Stringify(value.at("circuit")),d->components,d->wires,next,nullptr,nullptr,depth))return {};
    if(!ValidateSubcircuit(*d))return {};
    return d;
}

// 写文件
inline bool SaveCircuit(const wxString& path,
                        const wxVector<Component>& comps, const wxVector<Wire>& wires,
                        const wxVector<CustomDef>& defs = wxVector<CustomDef>(), const ModuleLibrary& modules=ModuleLibrary()) {
    const wxString json = CircuitToJson(comps, wires, defs, modules);
    wxVector<Component> checked;wxVector<Wire> checkedWires;int checkedNext=1;
    if(!JsonToCircuit(json.ToStdString(wxConvUTF8),checked,checkedWires,checkedNext))return false;
    wxTempFile f(path);
    if (!f.IsOpened())
        return false;                       // 打不开(路径不对/没权限)
    return f.Write(json, wxConvUTF8) && f.Commit();
}

// 读文件
inline bool LoadCircuit(const wxString& path,
                        wxVector<Component>& comps, wxVector<Wire>& wires, int& nextId,
                        wxVector<CustomDef>* defsOut = nullptr, ModuleLibrary* modulesOut = nullptr) {
    wxFile f;
    if (!f.Open(path, wxFile::read))
        return false;
    wxString content;
    if (!f.ReadAll(&content, wxConvUTF8))
        return false;
    f.Close();
    return JsonToCircuit(std::string((const char*)content.utf8_str()), comps, wires, nextId, defsOut, modulesOut);
}
