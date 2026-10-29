#include "editor/DrawingCanvas.h"
#include <wx/dcmemory.h>
#include <iostream>
#include <wx/filename.h>

class CanvasTestApp : public wxApp {
public:
    bool OnInit() override { return true; }
};
wxIMPLEMENT_APP_NO_MAIN(CanvasTestApp);

static int failures = 0;
static void Check(const char* name, bool ok) {
    std::cout << (ok ? "[PASS] " : "[FAIL] ") << name << std::endl;
    if (!ok) ++failures;
}
static void Flush(DrawingCanvas* canvas) {
    wxYield();
    canvas->Update();
}
static void Mouse(DrawingCanvas* canvas, wxEventType type, int x, int y) {
    wxMouseEvent event(type);
    event.SetPosition(wxPoint(x, y));
    canvas->GetEventHandler()->ProcessEvent(event);
}
static wxImage Capture(DrawingCanvas* canvas) {
    wxSize size = canvas->GetClientSize();
    wxBitmap bitmap(size.x, size.y);
    wxMemoryDC memory(bitmap);
    wxClientDC screen(canvas);
    memory.Blit(0, 0, size.x, size.y, &screen, 0, 0);
    memory.SelectObject(wxNullBitmap);
    return bitmap.ConvertToImage();
}
static int Differences(const wxImage& a, const wxImage& b, wxRect ignored = wxRect()) {
    int count = 0;
    for (int y = 0; y < a.GetHeight(); ++y)
        for (int x = 0; x < a.GetWidth(); ++x) {
            if (ignored.Contains(x, y)) continue;
            if (a.GetRed(x,y) != b.GetRed(x,y) || a.GetGreen(x,y) != b.GetGreen(x,y) || a.GetBlue(x,y) != b.GetBlue(x,y)) ++count;
        }
    return count;
}
int main(int argc, char** argv) {
    if (!wxEntryStart(argc, argv) || !wxTheApp->CallOnInit()) return 2;
    auto frame = new wxFrame(nullptr, wxID_ANY, "MyEDA canvas regression checks", wxPoint(50,50), wxSize(850,650));
    auto canvas = new DrawingCanvas(frame);
    frame->Show();
    frame->Raise();
    Flush(canvas);
    canvas->SetPlaceType(SW_LED);Mouse(canvas,wxEVT_LEFT_DOWN,400,250);canvas->SetWireMode();Flush(canvas);
    const auto ledPorts=Capture(canvas);
    Check("LED input lead reaches lamp body",ledPorts.GetRed(384,250)<120 && ledPorts.GetGreen(384,250)<140);
    Component input;input.type=SW_INPUT;input.x=200;input.y=250;
    Component output;output.type=SW_LED;output.x=400;output.y=250;
    Check("circuit input switch drives right pin",IsOutputPin(input,0) && GetPinPos(input,0)==wxPoint(220,250));
    Check("circuit output lamp receives left pin",!IsOutputPin(output,0) && GetPinPos(output,0)==wxPoint(380,250));
    if(argc>1){wxInitAllImageHandlers();canvas->SetPlaceType(SW_INPUT);Mouse(canvas,wxEVT_LEFT_DOWN,200,250);canvas->SetPlaceType(GATE_DFF);Mouse(canvas,wxEVT_LEFT_DOWN,300,350);canvas->SetPlaceType(SW_CLOCK);Mouse(canvas,wxEVT_LEFT_DOWN,200,350);canvas->SetWireMode();Flush(canvas);Capture(canvas).SaveFile(wxString::FromUTF8(argv[1]),wxBITMAP_TYPE_PNG);}
    canvas->NewDocument();
    canvas->SetPlaceType(SW_INPUT);
    Mouse(canvas, wxEVT_LEFT_DOWN, 200, 250);
    canvas->SetWireMode();
    Flush(canvas);
    auto baseline = Capture(canvas);
    Mouse(canvas, wxEVT_LEFT_DOWN, 222, 250);
    Flush(canvas);
    Check("first wire click does not draw towards stale origin", Differences(baseline, Capture(canvas), wxRect(218,246,9,9)) == 0);
    Mouse(canvas, wxEVT_MOTION, 500, 400);
    Flush(canvas);
    Check("moving mouse displays wire preview", Differences(baseline, Capture(canvas)) > 20);
    wxKeyEvent escape(wxEVT_KEY_DOWN);
    escape.m_keyCode = WXK_ESCAPE;
    canvas->GetEventHandler()->ProcessEvent(escape);
    Flush(canvas);
    Check("Escape clears preview pixels", Differences(baseline, Capture(canvas)) == 0);
    Mouse(canvas, wxEVT_MOTION, 600, 450);
    Flush(canvas);
    Check("Escape keeps preview cancelled on later mouse movement", Differences(baseline, Capture(canvas)) == 0);
    Mouse(canvas, wxEVT_LEFT_DOWN, 222, 250);
    Mouse(canvas, wxEVT_MOTION, 500, 400);
    Flush(canvas);
    canvas->SetPlaceType(GATE_AND);
    Flush(canvas);
    Check("switching tools clears preview immediately", Differences(baseline, Capture(canvas)) == 0);
    canvas->SetWireMode();
    Mouse(canvas, wxEVT_LEFT_DOWN, 222, 250);
    const wxPoint endpoints[] = {{80,80}, {600,80}, {80,500}, {600,500}};
    bool noTrails = true;
    for (const auto& point : endpoints) {
        Mouse(canvas, wxEVT_MOTION, point.x, point.y);
        Flush(canvas);
        auto partial = Capture(canvas);
        canvas->Refresh(false);
        Flush(canvas);
        noTrails = noTrails && Differences(partial, Capture(canvas)) == 0;
    }
    Check("partial refresh matches full repaint in all directions", noTrails);
    canvas->GetEventHandler()->ProcessEvent(escape);
    canvas->SetPlaceType(GATE_AND);
    Mouse(canvas, wxEVT_LEFT_DOWN, 480, 300);
    Flush(canvas);
    auto beforeWire = Capture(canvas);
    canvas->SetWireMode();
    Mouse(canvas, wxEVT_LEFT_DOWN, 222, 250);
    Mouse(canvas, wxEVT_LEFT_DOWN, 460, 290);
    Flush(canvas);
    auto afterWire = Capture(canvas);
    Check("completed wire is painted into scene", Differences(beforeWire, afterWire) > 20);
    canvas->Undo();
    Flush(canvas);
    Check("undo invalidates cached wires", Differences(beforeWire, Capture(canvas)) == 0);
    canvas->Redo();
    Flush(canvas);
    Check("redo restores cached wires", Differences(afterWire, Capture(canvas)) == 0);
    const auto originalSize = frame->GetSize();
    frame->SetSize(originalSize + wxSize(60,40));
    Flush(canvas);
    frame->SetSize(originalSize);
    Flush(canvas);
    Check("resize preserves scene after rebuilding buffer", Differences(afterWire, Capture(canvas)) == 0);

    canvas->Undo();
    canvas->SetWireMode();
    Mouse(canvas, wxEVT_LEFT_DOWN, 222,250);
    Mouse(canvas, wxEVT_LEFT_DOWN, 303,248);
    Mouse(canvas, wxEVT_LEFT_DOWN, 298,403);
    Mouse(canvas, wxEVT_LEFT_DOWN, 447,398);
    Mouse(canvas, wxEVT_LEFT_DOWN, 460,290);
    Flush(canvas);
    const wxString temp = wxFileName::CreateTempFileName("myeda-route-");
    wxVector<Component> cs; wxVector<Wire> ws; int next=1;
    auto readSaved = [&]() {
        return canvas->SaveFile(temp) && LoadCircuit(temp,cs,ws,next);
    };
    Check("multiple grid clicks commit one wire", readSaved() && ws.size()==1);
    Check("waypoints snap and survive save/load", ws.size()==1 && ws[0].bends.size()==3 &&
        ws[0].bends[0]==wxPoint(300,250) && ws[0].bends[1]==wxPoint(300,400) && ws[0].bends[2]==wxPoint(450,400));
    canvas->Undo();
    Check("undo removes entire routed wire", readSaved() && ws.empty());
    canvas->Redo();
    Check("redo preserves all waypoints", readSaved() && ws.size()==1 && ws[0].bends.size()==3);
    Flush(canvas);
    auto routedScene = Capture(canvas);
    Check("reload reproduces routed scene", canvas->LoadFile(temp));
    Flush(canvas);
    Check("saved route renders identically after reopen", Differences(routedScene,Capture(canvas))==0);
    wxRemoveFile(temp);


    canvas->NewDocument();
    canvas->SetPlaceType(SW_INPUT); Mouse(canvas,wxEVT_LEFT_DOWN,200,200);
    canvas->SetPlaceType(SW_LED); Mouse(canvas,wxEVT_LEFT_DOWN,400,200);
    canvas->SetWireMode();
    Mouse(canvas,wxEVT_LEFT_DOWN,380,200); // Intentionally select input first.
    Mouse(canvas,wxEVT_LEFT_DOWN,220,200);
    canvas->StartSim(); Mouse(canvas,wxEVT_LEFT_DOWN,200,200);
    Check("reverse click order still propagates from output to input",canvas->GetPinValue(2,0)==1);
    canvas->StopSim();
    canvas->SelectAll(); canvas->CopySelected(); canvas->PasteClipboard();
    const wxString auditFile=wxFileName::CreateTempFileName("myeda-readiness-");
    canvas->SaveFile(auditFile); cs.clear();ws.clear();LoadCircuit(auditFile,cs,ws,next);
    Check("copy includes internal wire",cs.size()==4 && ws.size()==2);
    canvas->NewDocument();
    canvas->SetPlaceType(SW_INPUT); Mouse(canvas,wxEVT_LEFT_DOWN,200,200);Mouse(canvas,wxEVT_LEFT_DOWN,400,200);
    canvas->SetWireMode();Mouse(canvas,wxEVT_LEFT_DOWN,220,200);Mouse(canvas,wxEVT_LEFT_DOWN,420,200);
    canvas->SaveFile(auditFile);LoadCircuit(auditFile,cs,ws,next);
    Check("output to output is rejected",ws.empty());
    wxRemoveFile(auditFile);


    canvas->NewDocument();
    Check("library add succeeds",canvas->AddCustomDefinition("LIB",8));
    Check("library-only edits mark document dirty",canvas->IsDirty());
    canvas->Undo();
    Check("undo restores library and clean saved state",canvas->CustomDefs().empty() && !canvas->IsDirty());
    canvas->Redo();
    Check("redo restores library",canvas->CustomDefs().size()==1);
    canvas->EditCustomDefinition("LIB","LIB2",6);
    Check("library rename is atomic",canvas->CustomDefs()[0].name=="LIB2");
    canvas->Undo();
    Check("library rename undo restores definition",canvas->CustomDefs()[0].name=="LIB" && canvas->CustomDefs()[0].truth==8);
    canvas->DeleteCustomDefinition("LIB");canvas->Undo();
    Check("library deletion can be undone",canvas->CustomDefs().size()==1);
    canvas->NewDocument();
    canvas->SetPlaceType(GATE_AND);Mouse(canvas,wxEVT_LEFT_DOWN,200,200);
    Flush(canvas);
    const auto viewBefore=Capture(canvas);
    wxMouseEvent wheel(wxEVT_MOUSEWHEEL);wheel.SetPosition(wxPoint(400,300));wheel.m_wheelRotation=120;wheel.m_wheelDelta=120;
    canvas->GetEventHandler()->ProcessEvent(wheel);Flush(canvas);
    Check("mouse wheel visibly zooms canvas",Differences(viewBefore,Capture(canvas))>100);
    const auto panBefore=Capture(canvas);
    Mouse(canvas,wxEVT_MIDDLE_DOWN,400,300);Mouse(canvas,wxEVT_MOTION,445,325);Mouse(canvas,wxEVT_MIDDLE_UP,445,325);Flush(canvas);
    Check("middle drag visibly pans canvas",Differences(panBefore,Capture(canvas))>100);


    canvas->ResetView();Flush(canvas);
    Check("reset view restores exact untransformed scene",Differences(viewBefore,Capture(canvas))==0 && canvas->GetViewZoom()==1.0);
    const wxString finalFile=wxFileName::CreateTempFileName("myeda-complete-");
    canvas->SaveFile(finalFile);const bool wasDirty=canvas->IsDirty();
    canvas->GetEventHandler()->ProcessEvent(wheel);
    Mouse(canvas,wxEVT_MIDDLE_DOWN,400,300);Mouse(canvas,wxEVT_MOTION,445,325);Mouse(canvas,wxEVT_MIDDLE_UP,445,325);
    Check("view operations do not dirty the saved document",canvas->IsDirty()==wasDirty);
    canvas->SetPlaceType(SW_INPUT);Mouse(canvas,wxEVT_LEFT_DOWN,325,445);
    canvas->SaveFile(finalFile);LoadCircuit(finalFile,cs,ws,next);
    Check("placement after zoom and pan uses world coordinates",cs.size()==2 && cs[1].x==300 && cs[1].y==400);
    canvas->ResetView();canvas->NewDocument();
    canvas->SetPlaceType(SW_INPUT);Mouse(canvas,wxEVT_LEFT_DOWN,200,200);
    canvas->SetPlaceType(SW_LED);Mouse(canvas,wxEVT_LEFT_DOWN,400,200);
    canvas->SetWireMode();Mouse(canvas,wxEVT_LEFT_DOWN,220,200);Mouse(canvas,wxEVT_LEFT_DOWN,380,200);
    canvas->SetSelectMode();Mouse(canvas,wxEVT_LEFT_DOWN,300,200);
    Check("single wire can be selected",canvas->HasSelectedWire());
    canvas->DeleteSelected();canvas->SaveFile(finalFile);LoadCircuit(finalFile,cs,ws,next);
    Check("deleting selected wire preserves its components",cs.size()==2 && ws.empty());
    canvas->Undo();canvas->SaveFile(finalFile);LoadCircuit(finalFile,cs,ws,next);
    Check("wire deletion undo restores connection",ws.size()==1);
    wxFile malformed(finalFile,wxFile::write);malformed.Write("not a circuit");malformed.Close();
    Check("malformed file rejected by canvas",!canvas->LoadFile(finalFile));
    canvas->SaveFile(finalFile);LoadCircuit(finalFile,cs,ws,next);
    Check("failed file open preserves current circuit",cs.size()==2 && ws.size()==1);

    wxFile badNet(finalFile,wxFile::write);badNet.Write("not a netlist");badNet.Close();
    Check("malformed netlist rejected",!canvas->ImportNetlist(finalFile));
    canvas->SaveFile(finalFile);LoadCircuit(finalFile,cs,ws,next);
    Check("failed netlist import preserves circuit",cs.size()==2 && ws.size()==1);


    canvas->NewDocument();canvas->AddCustomDefinition("ACTIVE",6);canvas->SetCustomPlace("ACTIVE",6);
    canvas->EditCustomDefinition("ACTIVE","ACTIVE",8);Mouse(canvas,wxEVT_LEFT_DOWN,200,200);
    canvas->SaveFile(finalFile);LoadCircuit(finalFile,cs,ws,next);
    Check("active custom tool uses edited truth table",cs.size()==1 && cs[0].truth==8);
    canvas->EditCustomDefinition("ACTIVE","RENAMED",8);Mouse(canvas,wxEVT_LEFT_DOWN,400,200);
    canvas->SaveFile(finalFile);LoadCircuit(finalFile,cs,ws,next);
    Check("active custom tool follows renamed definition",cs.size()==2 && cs[1].customName=="RENAMED");
    wxVector<Component> passive;Component p1;p1.id=1;p1.type=SW_LED;passive.push_back(p1);p1.id=2;passive.push_back(p1);
    wxVector<Wire> passiveW;passiveW.push_back({1,0,2,0});
    wxFile driverless(finalFile,wxFile::write);driverless.Write(ExportKiCadNetlist(passive,passiveW),wxConvUTF8);driverless.Close();
    Check("driverless netlist cannot enter logical document",!canvas->ImportNetlist(finalFile));
    canvas->NewDocument();canvas->SetPlaceType(SW_INPUT);Mouse(canvas,wxEVT_LEFT_DOWN,200,200);canvas->SaveFile(finalFile);
    canvas->SetSelectMode();Mouse(canvas,wxEVT_LEFT_DOWN,200,200);Mouse(canvas,wxEVT_MOTION,240,230);canvas->ResetView();
    Check("reset during drag preserves dirty and undo state",canvas->IsDirty() && canvas->CanUndo());

    // Test annotate / renumbering components
    canvas->NewDocument();
    cs.clear(); ws.clear();
    {
        Component ca; ca.id = 5; ca.type = GATE_NOT; ca.x = 300; ca.y = 100; cs.push_back(ca);
        Component cb; cb.id = 10; cb.type = SW_INPUT; cb.x = 100; cb.y = 100; cs.push_back(cb);
        ws.push_back({10, 0, 5, 0});
        SaveCircuit(finalFile, cs, ws);
        canvas->LoadFile(finalFile);
        Check("annotate renumbers to continuous U1, U2", canvas->AnnotateComponents());
        canvas->SaveFile(finalFile);
        LoadCircuit(finalFile, cs, ws, next);
        const int idAt100 = (cs[0].x == 100) ? cs[0].id : cs[1].id;
        const int idAt300 = (cs[0].x == 300) ? cs[0].id : cs[1].id;
        Check("left component becomes U1", idAt100 == 1);
        Check("right component becomes U2", idAt300 == 2);
        Check("wire updated to connect U1 and U2", ws[0].comp1 == 1 && ws[0].comp2 == 2);
        Check("annotate is undoable", canvas->Undo());
        canvas->SaveFile(finalFile);
        LoadCircuit(finalFile, cs, ws, next);
        Check("undo restores original discontinuous IDs", ws[0].comp1 == 10 && ws[0].comp2 == 5);
    }


    canvas->NewDocument();
    cs.clear();ws.clear();
    for(int i=0;i<3;++i){Component c;c.id=i+1;c.type=i==0?SW_INPUT:(i==1?GATE_NOT:SW_LED);c.x=100+i*140;c.y=100;cs.push_back(c);}
    ws.push_back({1,0,2,0});ws.push_back({2,1,3,0});SaveCircuit(finalFile,cs,ws);
    canvas->LoadFile(finalFile);canvas->SelectAll();wxVector<wxString> mi,mo;mi.push_back("A");mo.push_back("Y");wxString moduleError;
    Check("selection encapsulates actual circuit",canvas->PackSelection("Inverter",mi,mo,&moduleError) && canvas->Modules().size()==1);
    Check("encapsulation marks library dirty",canvas->IsDirty());
    Check("encapsulation is undoable",canvas->Undo() && canvas->Modules().empty() && !canvas->IsDirty());
    Check("encapsulation redo restores definition",canvas->Redo() && canvas->Modules().size()==1);
    canvas->SetModulePlace(canvas->Modules()[0]);Mouse(canvas,wxEVT_LEFT_DOWN,600,300);Mouse(canvas,wxEVT_LEFT_UP,600,300);
    canvas->SaveFile(finalFile);ModuleLibrary ml;LoadCircuit(finalFile,cs,ws,next,nullptr,&ml);
    Check("module placement retains original circuit",cs.size()==4 && ws.size()==2 && cs.back().module && ml.size()==1);
    Check("module placement undo keeps definition",canvas->Undo() && canvas->Modules().size()==1);
    canvas->Redo();canvas->SetSelectMode();canvas->SelectAll();canvas->CopySelected();canvas->SetModulePlace(canvas->Modules()[0]);canvas->PasteClipboard();
    canvas->SaveFile(finalFile);LoadCircuit(finalFile,cs,ws,next);
    Check("copy preserves leaf gates while module tool active",cs.size()==8 && cs[4].type==SW_INPUT && !cs[4].module && cs[7].module);
    canvas->Undo();canvas->Undo();
    canvas->Redo();canvas->SaveFile(finalFile);canvas->NewDocument();canvas->LoadFile(finalFile);
    Check("canvas reopen retains module library",canvas->Modules().size()==1);
    const wxString moduleFile=finalFile+".edasub";
    Check("standalone module export succeeds",canvas->ExportModule(moduleFile,0));
    Check("duplicate module import is rejected",!canvas->ImportModules(moduleFile,&moduleError) && canvas->Modules().size()==1);
    canvas->NewDocument();Check("standalone module import succeeds",canvas->ImportModules(moduleFile,&moduleError) && canvas->Modules().size()==1);
    Check("import library can be undone",canvas->Undo() && canvas->Modules().empty());canvas->Redo();
    Check("module definition can be deleted", canvas->DeleteModuleDefinition("Inverter") && canvas->Modules().empty());
    Check("module deletion can be undone", canvas->Undo() && canvas->Modules().size() == 1);
    Check("module deletion can be redone", canvas->Redo() && canvas->Modules().empty());
    canvas->Undo();
    canvas->ShowSubcircuit(canvas->Modules()[0]);canvas->SelectAll();
    Check("internal viewer prevents deletion",!canvas->DeleteSelected());
    Mouse(canvas,wxEVT_LEFT_DOWN,500,400);Mouse(canvas,wxEVT_LEFT_UP,500,400);canvas->SaveFile(finalFile);LoadCircuit(finalFile,cs,ws,next);
    Check("internal viewer prevents placement",cs.size()==3 && ws.size()==2);
    wxRemoveFile(moduleFile);

    wxRemoveFile(finalFile);

    frame->Destroy();
    wxYield();
    wxTheApp->OnExit();
    wxEntryCleanup();
    return failures ? 1 : 0;
}
