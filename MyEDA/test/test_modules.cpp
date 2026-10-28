#include "file/CircuitFile.h"
#include "sim/Simulation.h"
#include <iostream>
#include "file/NetlistExport.h"
#include <filesystem>
#include <wx/filename.h>

int main(int argc,char** argv) {
    const char* project=R"json({
      "components":[
        {"id":1,"type":"SW","x":100,"y":100,"state":1},
        {"id":2,"type":"SW","x":100,"y":200,"state":1},
        {"id":3,"type":"MODULE","name":"HalfAdder","x":300,"y":150,"state":0,
         "module":{"name":"HalfAdder","inputs":[{"id":1,"name":"A"},{"id":2,"name":"B"}],
         "outputs":[{"id":5,"name":"S"},{"id":6,"name":"C"}],"circuit":{
          "components":[
           {"id":1,"type":"SW","x":100,"y":100,"state":0},
           {"id":2,"type":"SW","x":100,"y":200,"state":0},
           {"id":3,"type":"XOR","x":300,"y":100,"state":0},
           {"id":4,"type":"AND","x":300,"y":200,"state":0},
           {"id":5,"type":"LED","x":500,"y":100,"state":0},
           {"id":6,"type":"LED","x":500,"y":200,"state":0}],
          "wires":[[1,0,3,0],[2,0,3,1],[1,0,4,0],[2,0,4,1],[3,2,5,0],[4,2,6,0]]}}},
        {"id":4,"type":"LED","x":500,"y":100,"state":0},
        {"id":5,"type":"LED","x":500,"y":200,"state":0}],
      "wires":[[1,0,3,0],[2,0,3,1],[3,2,4,0],[3,3,5,0]]})json";
    wxVector<Component> cs;wxVector<Wire> ws;int next=1;
    if(!JsonToCircuit(project,cs,ws,next)) {
        std::cout<<"[FAIL] half-adder subcircuit project recognized\n";return 1;
    }
    std::cout<<"[PASS] half-adder subcircuit project recognized\n";
    auto signals=SimulateCircuit(cs,ws);
    const bool ok=LedValue(cs[3],ws,signals)==0 && LedValue(cs[4],ws,signals)==1;
    std::cout<<(ok?"[PASS] ":"[FAIL] ")<<"half-adder has separate sum and carry outputs\n";

    int failures=ok?0:1,checks=2;
    auto check=[&](bool pass,const char* name){++checks;std::cout<<(pass?"[PASS] ":"[FAIL] ")<<name<<"\n";if(!pass)++failures;};
    auto half=cs[2].module;
    for(int n=0;n<4;++n){cs[0].state=n&1;cs[1].state=(n>>1)&1;auto v=SimulateCircuit(cs,ws);
        check(LedValue(cs[3],ws,v)==((n&1)^((n>>1)&1)) && LedValue(cs[4],ws,v)==(n==3),"half-adder truth table");}
    auto make=[](int id,GateType type,int x,int y,ModulePtr module=ModulePtr()){
        Component c;c.id=id;c.type=type;c.x=x;c.y=y;c.module=module;if(module)c.customName=module->name;return c;};
    wxVector<Component> full;
    full.push_back(make(1,SW_INPUT,100,100));full.push_back(make(2,SW_INPUT,100,200));full.push_back(make(3,SW_INPUT,100,300));
    full.push_back(make(4,GATE_MODULE,320,150,half));full.push_back(make(5,GATE_MODULE,540,220,half));
    full.push_back(make(6,GATE_OR,720,320));full.push_back(make(7,SW_LED,820,180));full.push_back(make(8,SW_LED,820,320));
    wxVector<Wire> fw;
    for(auto w:std::vector<Wire>{{1,0,4,0},{2,0,4,1},{4,2,5,0},{3,0,5,1},{4,3,6,0},{5,3,6,1},{5,2,7,0},{6,2,8,0}})fw.push_back(w);
    for(int n=0;n<8;++n){for(int i=0;i<3;++i)full[i].state=(n>>i)&1;auto v=SimulateCircuit(full,fw);int count=(n&1)+((n>>1)&1)+((n>>2)&1);
        check(LedValue(full[6],fw,v)==(count&1) && LedValue(full[7],fw,v)==(count>=2),"two independent half-adder instances make full-adder");}
    wxVector<wxString> ins,outs;ins.push_back("A");ins.push_back("B");ins.push_back("Cin");outs.push_back("S");outs.push_back("Cout");
    std::set<int> selection;for(auto c:full)selection.insert(c.id);wxString error;
    auto combined=CreateSubcircuit(full,fw,selection,"FullAdder",ins,outs,&error);
    check(bool(combined),"pack nested full-adder");if(!combined)return 1;
    auto nested=full;nested.clear();
    for(int i=0;i<3;++i)nested.push_back(make(i+1,SW_INPUT,100,100+i*100));
    nested.push_back(make(4,GATE_MODULE,400,200,combined));nested.push_back(make(5,SW_LED,700,160));nested.push_back(make(6,SW_LED,700,260));
    wxVector<Wire> nw;for(auto w:std::vector<Wire>{{1,0,4,0},{2,0,4,1},{3,0,4,2},{4,3,5,0},{4,4,6,0}})nw.push_back(w);
    for(int n=0;n<8;++n){for(int i=0;i<3;++i)nested[i].state=(n>>i)&1;auto v=SimulateCircuit(nested,nw);int count=(n&1)+((n>>1)&1)+((n>>2)&1);
        check(LedValue(nested[4],nw,v)==(count&1) && LedValue(nested[5],nw,v)==(count>=2),"nested full-adder truth table");}
    ModuleLibrary lib;lib.push_back(half);lib.push_back(combined);wxVector<Component> loaded;wxVector<Wire> lw;ModuleLibrary ll;
    check(JsonToCircuit(CircuitToJson(nested,nw,wxVector<CustomDef>(),lib).ToStdString(wxConvUTF8),loaded,lw,next,nullptr,&ll) && ll.size()==2 && loaded[3].module->components[3].module,"nested project and library persist");
    check(JsonToCircuit(CircuitToJson({}, {},wxVector<CustomDef>(),lib).ToStdString(wxConvUTF8),loaded,lw,next,nullptr,&ll) && loaded.empty() && ll.size()==2,"unused library definitions persist");
    wxVector<Component> flat;wxVector<Wire> flatw;
    check(FlattenCircuit(nested,nw,flat,flatw,&error),"flatten nested modules for KiCad");
    check(std::none_of(flat.begin(),flat.end(),[](const Component& c){return c.type==GATE_MODULE;}),"flatten contains only real leaf gates");
    for(int n=0;n<8;++n){for(auto& c:flat)if(c.id<=3)c.state=(n>>(c.id-1))&1;auto v=SimulateCircuit(flat,flatw);int count=(n&1)+((n>>1)&1)+((n>>2)&1);
        check(LedValue(*CircuitComponent(flat,5),flatw,v)==(count&1) && LedValue(*CircuitComponent(flat,6),flatw,v)==(count>=2),"flattened circuit matches hierarchy");}
    auto net=ExportKiCadNetlist(nested,nw);
    check(ParseKiCadNetlist(net,loaded,lw) && loaded.size()==flat.size(),"KiCad netlist exports actual internal gates");
    auto bad=*half;bad.outputNames[1]=bad.inputNames[0];check(!ValidateSubcircuit(bad),"duplicate port names rejected");
    bad=*half;bad.outputIds[0]=99;check(!ValidateSubcircuit(bad),"missing output marker rejected");
    selection.erase(8);check(!CreateSubcircuit(full,fw,selection,"Broken",ins,outs,&error),"partial circuit with crossing wire rejected");
    auto before=CircuitToJson(nested,nw);
    check(!JsonToCircuit("{}",nested,nw,next) && CircuitToJson(nested,nw)==before,"failed module project load preserves document");
    const auto safePath=wxFileName::CreateTempFileName("myeda-module-limit");
    check(SaveCircuit(safePath,cs,ws),"valid module document saves");
    wxFile original(safePath);wxString originalText;original.ReadAll(&originalText);original.Close();
    auto oversized=cs;oversized[2].customName=wxString('X',11*1024*1024);
    check(!SaveCircuit(safePath,oversized,ws),"oversized document save rejected");
    wxFile unchanged(safePath);wxString unchangedText;unchanged.ReadAll(&unchangedText);unchanged.Close();
    check(unchangedText==originalText,"rejected oversized save preserves existing file");wxRemoveFile(safePath);
    if(argc==2) {
        const auto dir=std::filesystem::u8path(argv[1]);std::filesystem::create_directories(dir);
        auto path=[&](const char* name){return wxString((dir/name).wstring());};
        ModuleLibrary h;h.push_back(half);ModuleLibrary f;f.push_back(combined);
        check(SaveCircuit(path("half_adder.edasub"),{}, {},{},h),"write reusable half-adder example");
        check(SaveCircuit(path("full_adder.edasub"),{}, {},{},f),"write reusable full-adder example");
        check(SaveCircuit(path("full_adder.eda"),full,fw,{},lib),"write two-block full-adder example");
        check(SaveCircuit(path("nested_full_adder.eda"),nested,nw,{},lib),"write nested full-adder example");
    }
    std::cout<<checks<<" checks, "<<failures<<" failures\n";return failures?1:0;
}
