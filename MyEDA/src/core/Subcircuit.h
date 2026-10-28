#pragma once
#include "core/CircuitValidation.h"
#include <functional>
#include <map>
#include <set>

inline bool ValidateSubcircuit(const SubcircuitDef& d, wxString* error=nullptr,int depth=0) {
    auto fail=[&](const wxString& message){if(error)*error=message;return false;};
    if(depth>=8)return fail("子电路嵌套超过 8 层");
    if(d.name.empty() || d.name.size()>80)return fail("请为子电路填写名称（不超过 80 个字符）");
    if(d.inputIds.empty() || d.outputIds.empty() || d.inputIds.size()>16 || d.outputIds.size()>16)
        return fail("子电路需要 1～16 个输入开关和 1～16 个输出指示灯");
    if(d.inputIds.size()!=d.inputNames.size() || d.outputIds.size()!=d.outputNames.size())return fail("端口名称数量与输入输出数量不一致");
    if(d.components.size()>10000)return fail("子电路元件数量过多");
    std::set<int> allIds,ports;std::set<wxString> names;
    for(const auto& c:d.components) {
        if(c.type==GATE_DFF || c.type==SW_CLOCK)return fail("当前封装支持组合逻辑，请将触发器和时钟源保留在顶层电路");
        if(c.id<=0 || !allIds.insert(c.id).second)return fail("子电路内部元件编号无效");
        if(c.type==GATE_MODULE && (!c.module || !ValidateSubcircuit(*c.module,error,depth+1)))return false;
    }
    for(size_t i=0;i<d.inputIds.size();++i) {
        const auto c=CircuitComponent(d.components,d.inputIds[i]);
        if(!c || c->type!=SW_INPUT || !ports.insert(c->id).second)return fail("输入端口必须对应不同的开关");
        if(d.inputNames[i].empty() || d.inputNames[i].size()>24 || !names.insert(d.inputNames[i]).second)return fail("端口名称不能为空、重复或超过 24 个字符");
    }
    for(size_t i=0;i<d.outputIds.size();++i) {
        const auto c=CircuitComponent(d.components,d.outputIds[i]);
        if(!c || c->type!=SW_LED || !ports.insert(c->id).second)return fail("输出端口必须对应不同的指示灯");
        if(d.outputNames[i].empty() || d.outputNames[i].size()>24 || !names.insert(d.outputNames[i]).second)return fail("端口名称不能为空、重复或超过 24 个字符");
        bool connected=false;for(const auto& w:d.wires)if(w.comp2==c->id && w.pin2==0)connected=true;
        if(!connected)return fail("输出指示灯尚未连接内部电路");
    }
    for(const auto& c:d.components)if((c.type==SW_INPUT || c.type==SW_LED) && !ports.count(c.id))return fail("所有内部开关和指示灯都必须定义为端口");
    wxVector<Wire> checked;
    for(auto w:d.wires){if(!NormalizeConnection(d.components,checked,w,error))return false;checked.push_back(w);}
    return true;
}

inline ModulePtr CreateSubcircuit(const wxVector<Component>& components,const wxVector<Wire>& wires,
    const std::set<int>& selection,const wxString& name,const wxVector<wxString>& inputs,
    const wxVector<wxString>& outputs,wxString* error=nullptr) {
    auto d=std::make_shared<SubcircuitDef>();d->name=name;
    for(const auto& c:components)if(selection.count(c.id))d->components.push_back(c);
    std::sort(d->components.begin(),d->components.end(),[](const Component& a,const Component& b){return a.id<b.id;});
    for(const auto& w:wires) {
        const bool a=selection.count(w.comp1)!=0,b=selection.count(w.comp2)!=0;
        if(a!=b){if(error)*error="选区存在外部连接，请把接口开关、指示灯及完整电路一并选中";return {};}
        if(a)d->wires.push_back(w);
    }
    for(const auto& c:d->components) {
        if(c.type==SW_INPUT)d->inputIds.push_back(c.id);
        if(c.type==SW_LED)d->outputIds.push_back(c.id);
    }
    d->inputNames=inputs;d->outputNames=outputs;
    if(!ValidateSubcircuit(*d,error))return {};
    return d;
}

// Flatten only at interchange boundaries. Simulation and storage retain hierarchy.
inline bool FlattenCircuit(const wxVector<Component>& input,const wxVector<Wire>& inputWires,
    wxVector<Component>& result,wxVector<Wire>& resultWires,wxString* error=nullptr) {
    bool modules=false;int next=1;
    for(const auto& c:input){modules=modules || c.type==GATE_MODULE;next=std::max(next,c.id+1);}
    if(!modules){result=input;resultWires=inputWires;return true;}
    std::map<std::string,std::string> parents;
    std::map<std::string,std::pair<int,int>> leafPins;
    std::function<std::string(const std::string&)> root=[&](const std::string& key)->std::string {
        auto it=parents.find(key);if(it==parents.end()){parents[key]=key;return key;}
        if(it->second==key)return key;
        return it->second=root(it->second);
    };
    auto unite=[&](const std::string& a,const std::string& b){parents[root(a)]=root(b);};
    auto key=[](const std::string& prefix,int id,int pin){return prefix+"/"+std::to_string(id)+":"+std::to_string(pin);};
    wxVector<Component> flat;
    std::function<bool(const wxVector<Component>&,const wxVector<Wire>&,const SubcircuitDef*,const std::string&,wxPoint,int)> visit;
    visit=[&](const wxVector<Component>& cs,const wxVector<Wire>& ws,const SubcircuitDef* boundary,const std::string& prefix,wxPoint offset,int depth) {
        if(depth>8)return false;
        std::set<int> markers;
        if(boundary){for(int id:boundary->inputIds)markers.insert(id);for(int id:boundary->outputIds)markers.insert(id);}
        for(const auto& c:cs) {
            for(int pin=0;pin<GatePinCount(c);++pin)root(key(prefix,c.id,pin));
            if(c.type==GATE_MODULE) {
                if(!c.module || !ValidateSubcircuit(*c.module,error))return false;
                const auto& d=*c.module;
                const std::string child=prefix+"/module"+std::to_string(c.id);
                wxPoint centre;
                if(!d.components.empty()) {
                    int minX=d.components[0].x,maxX=minX,minY=d.components[0].y,maxY=minY;
                    for(const auto& inner:d.components){minX=std::min(minX,inner.x);maxX=std::max(maxX,inner.x);minY=std::min(minY,inner.y);maxY=std::max(maxY,inner.y);}
                    centre=SnapGrid(wxPoint((minX+maxX)/2,(minY+maxY)/2));
                }
                if(!visit(d.components,d.wires,&d,child,offset+wxPoint(c.x,c.y)-centre,depth+1))return false;
                for(size_t i=0;i<d.inputIds.size();++i)unite(key(prefix,c.id,(int)i),key(child,d.inputIds[i],0));
                for(size_t i=0;i<d.outputIds.size();++i)unite(key(prefix,c.id,(int)(d.inputIds.size()+i)),key(child,d.outputIds[i],0));
            } else if(!markers.count(c.id)) {
                Component copy=c;if(boundary)copy.id=next++;
                copy.x+=offset.x;copy.y+=offset.y;flat.push_back(copy);
                for(int pin=0;pin<GatePinCount(c);++pin)leafPins[key(prefix,c.id,pin)]={copy.id,pin};
            }
        }
        for(const auto& w:ws)unite(key(prefix,w.comp1,w.pin1),key(prefix,w.comp2,w.pin2));
        return true;
    };
    if(!visit(input,inputWires,nullptr,"root",wxPoint(0,0),0))return false;
    std::map<std::string,std::vector<std::pair<int,int>>> nets;
    for(const auto& pin:leafPins)nets[root(pin.first)].push_back(pin.second);
    wxVector<Wire> flatWires;
    for(const auto& net:nets) {
        if(net.second.size()<2)continue;
        int drivers=0;std::pair<int,int> source;
        for(const auto& p:net.second)if(IsOutputPin(*CircuitComponent(flat,p.first),p.second)){source=p;++drivers;}
        if(drivers>1){if(error)*error="展开子电路后出现多个输出驱动同一网络";return false;}
        if(!drivers)continue; // A disconnected module input remains logically low.
        for(const auto& p:net.second)if(p!=source){Wire w={source.first,source.second,p.first,p.second};if(!NormalizeConnection(flat,flatWires,w,error))return false;flatWires.push_back(w);}
    }
    result=flat;resultWires=flatWires;return true;
}
