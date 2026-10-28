#pragma once
#include "core/Component.h"
#include <algorithm>
#include <set>

inline bool IsOutputPin(const Component& c,int pin) {
    if(pin<0 || pin>=GatePinCount(c)) return false;
    if(c.type==GATE_MODULE)return c.module && pin>=(int)c.module->inputIds.size();
    if(c.type==SW_INPUT || c.type==SW_CLOCK) return true;
    if(c.type==SW_LED) return false;
    return pin==GatePinCount(c)-1;
}
inline const Component* CircuitComponent(const wxVector<Component>& cs,int id) {
    for(const auto& c:cs) if(c.id==id) return &c;
    return nullptr;
}
inline bool NormalizeConnection(const wxVector<Component>& cs,const wxVector<Wire>& existing,Wire& w,wxString* error=nullptr) {
    auto reject=[&](const wxString& s){if(error)*error=s;return false;};
    auto a=CircuitComponent(cs,w.comp1),b=CircuitComponent(cs,w.comp2);
    if(!a || !b || w.pin1<0 || w.pin2<0 || w.pin1>=GatePinCount(*a) || w.pin2>=GatePinCount(*b)) return reject("连线引用的元件或引脚无效");
    const bool outputA=IsOutputPin(*a,w.pin1),outputB=IsOutputPin(*b,w.pin2);
    if(outputA==outputB) return reject("请连接一个输出引脚和一个输入引脚");
    if(!outputA){std::swap(w.comp1,w.comp2);std::swap(w.pin1,w.pin2);std::reverse(w.bends.begin(),w.bends.end());}
    if(w.comp1==w.comp2 && CircuitComponent(cs,w.comp2)->type!=GATE_DFF) return reject("组合逻辑不能连接自身形成反馈");
    for(const auto& e:existing) if(e.comp2==w.comp2 && e.pin2==w.pin2) return reject("该输入已有连线，请先删除原连线");
    // Adding source -> target must not close a directed cycle.
    std::set<int> seen;wxVector<int> pending;pending.push_back(w.comp2);
    while(!pending.empty()) {
        int id=pending.back();pending.pop_back();
        const auto node=CircuitComponent(cs,id);
        if(node && node->type==GATE_DFF)continue;
        if(id==w.comp1) return reject("该连线会形成反馈环，当前只支持无环组合逻辑");
        if(!seen.insert(id).second) continue;
        for(const auto& e:existing) if(e.comp1==id) pending.push_back(e.comp2);
    }
    return true;
}
