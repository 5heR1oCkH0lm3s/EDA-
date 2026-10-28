#pragma once
#include "core/CircuitValidation.h"
#include <functional>
#include <map>
#include <set>

using PinSignals = std::map<std::pair<int,int>,int>;
struct SimulationResult : public std::map<int,int> { PinSignals pins; };
inline int ReadInputSignal(int id,int pin,const wxVector<Wire>& wires,const PinSignals& signals) {
    for(const auto& w:wires)if(w.comp2==id && w.pin2==pin) {
        auto it=signals.find({w.comp1,w.pin1});return it==signals.end()?0:it->second;
    }
    return 0;
}
inline SimulationResult SimulateCircuit(const wxVector<Component>& comps,const wxVector<Wire>& wires,int depth=0) {
    SimulationResult result;
    if(depth>8)return result;
    std::map<int,const Component*> byId;
    for(const auto& c:comps)byId[c.id]=&c;
    std::set<int> done,active;
    std::function<void(int)> evaluate;
    auto input=[&](int id,int pin) {
        for(const auto& w:wires)if(w.comp2==id && w.pin2==pin) {
            evaluate(w.comp1);auto it=result.pins.find({w.comp1,w.pin1});
            return it==result.pins.end()?0:it->second;
        }
        return 0;
    };
    evaluate=[&](int id) {
        if(done.count(id) || active.count(id) || !byId.count(id))return;
        active.insert(id);const Component& c=*byId[id];
        if(c.type==SW_INPUT)result.pins[{id,0}]=c.state?1:0;
        else if(c.type==GATE_MODULE && c.module) {
            wxVector<Component> internal=c.module->components;
            for(size_t i=0;i<c.module->inputIds.size();++i) {
                const bool value=input(id,(int)i)!=0;
                for(auto& port:internal)if(port.id==c.module->inputIds[i])port.state=value;
            }
            const auto child=SimulateCircuit(internal,c.module->wires,depth+1);
            for(size_t i=0;i<c.module->outputIds.size();++i)
                result.pins[{id,(int)(c.module->inputIds.size()+i)}]=ReadInputSignal(c.module->outputIds[i],0,c.module->wires,child.pins);
        } else if(c.type!=SW_LED) {
            const int a=input(id,0),b=c.type==GATE_NOT?0:input(id,1);int value=0;
            switch(c.type) {
            case GATE_AND:value=a&b;break;case GATE_OR:value=a|b;break;
            case GATE_NOT:value=!a;break;case GATE_XOR:value=a^b;break;
            case GATE_NAND:value=!(a&b);break;case GATE_NOR:value=!(a|b);break;
            case GATE_CUSTOM:value=(c.truth>>(a*2+b))&1;break;default:break;
            }
            result.pins[{id,GatePinCount(c)-1}]=value;
        }
        const int firstOutput=c.type==GATE_MODULE && c.module?(int)c.module->inputIds.size():GatePinCount(c)-1;
        auto first=result.pins.find({id,firstOutput});result[id]=first==result.pins.end()?0:first->second;
        active.erase(id);done.insert(id);
    };
    for(const auto& c:comps)evaluate(c.id);
    return result;
}
inline int FindSource(const wxVector<Wire>& wires,int id,int pin) {
    for(const auto& w:wires)if(w.comp2==id && w.pin2==pin)return w.comp1;return -1;
}
inline int LedValue(const Component& led,const wxVector<Wire>& wires,const SimulationResult& result) {
    return ReadInputSignal(led.id,0,wires,result.pins);
}
inline int LedValue(const Component& led,const wxVector<Wire>& wires,const std::map<int,int>& result) {
    auto it=result.find(FindSource(wires,led.id,0));return it==result.end()?0:it->second;
}
