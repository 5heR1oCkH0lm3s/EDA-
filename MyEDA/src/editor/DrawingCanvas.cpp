#include "editor/DrawingCanvas.h"
#include <algorithm>
#include <wx/dcbuffer.h>
#include <wx/graphics.h>
#include <memory>

DrawingCanvas::DrawingCanvas(wxWindow* parent)
    : wxPanel(parent, wxID_ANY)
{
    SetBackgroundStyle(wxBG_STYLE_PAINT); // Paint the background once, in the buffer.
    Bind(wxEVT_SIZE, &DrawingCanvas::OnSize, this);
    Bind(wxEVT_PAINT, &DrawingCanvas::OnPaint, this);
    Bind(wxEVT_LEFT_DOWN, &DrawingCanvas::OnLeftDown, this);
    Bind(wxEVT_MOTION, &DrawingCanvas::OnMotion, this);
    Bind(wxEVT_LEFT_UP, &DrawingCanvas::OnLeftUp, this);
    Bind(wxEVT_KEY_DOWN, &DrawingCanvas::OnKeyDown, this);
    Bind(wxEVT_MOUSEWHEEL, &DrawingCanvas::OnWheel, this);
    Bind(wxEVT_MIDDLE_DOWN, &DrawingCanvas::OnMiddleDown, this);
    Bind(wxEVT_MIDDLE_UP, &DrawingCanvas::OnMiddleUp, this);
    Bind(wxEVT_MOUSE_CAPTURE_LOST, [this](wxMouseCaptureLostEvent&) {
        if (dragging && dragStartSnapshot != CircuitToJson(components,wires,customDefs,moduleDefs)) CommitSnapshot(dragStartSnapshot);
        dragging = rubberBand = panning = false; NotifyChanged();
    });
    savedSnapshot = CircuitToJson(components, wires, customDefs, moduleDefs);
    SetFocus();
}

// ================================================================
// 撤销/重做:快照式
// 每步改动电路之前,先把当前电路存成一段 JSON 文本压栈;
// 撤销 = 弹出一段旧文本读回来。因为 JSON 往返已被验证无损,
// 这个做法天然可靠,不用为每种操作单独写"反操作"代码
// ================================================================
void DrawingCanvas::PushUndo()
{
    undoStack.push_back(CircuitToJson(components, wires, customDefs, moduleDefs));
    if (undoStack.size() > 100)                       // 只留最近 100 步
        undoStack.erase(undoStack.begin());
    redoStack.clear();                                // 有了新动作,重做链失效
}

void DrawingCanvas::CommitSnapshot(const wxString& snap)
{
    undoStack.push_back(snap);
    if (undoStack.size() > 100)
        undoStack.erase(undoStack.begin());
    redoStack.clear();
}

void DrawingCanvas::RestoreSnapshot(const wxString& snap)
{
    int next = 1;
    if(!JsonToCircuit(std::string((const char*)snap.utf8_str()), components, wires, next, &customDefs, &moduleDefs)){Hint("无法恢复历史记录，当前电路保留");return;}
    nextId = next;
    selectedWire = -1;
    sel.clear();
    wireMode = false;
    wireFromComp = -1;
    NotifyChanged();
}

bool DrawingCanvas::Undo()
{
    if (simRunning) { Hint("请先停止仿真再撤销"); return false; }
    if (undoStack.size() == 0) return false;
    redoStack.push_back(CircuitToJson(components, wires, customDefs, moduleDefs));   // 当前状态留给重做
    wxString snap = undoStack.back();
    undoStack.pop_back();
    RestoreSnapshot(snap);
    return true;
}

bool DrawingCanvas::Redo()
{
    if (simRunning) { Hint("请先停止仿真再重做"); return false; }
    if (redoStack.size() == 0) return false;
    undoStack.push_back(CircuitToJson(components, wires, customDefs, moduleDefs));
    wxString snap = redoStack.back();
    redoStack.pop_back();
    RestoreSnapshot(snap);
    return true;
}

// ================================================================
// 选择:单选 = 集合里只有一个元素;多选就是这么自然实现的
// ================================================================
void DrawingCanvas::SetSelectMode()
{
    CancelWirePreview();
    selectMode = true;
    wireMode = false;
    selectedWire = -1;
    sel.clear();
    NotifyChanged();
}

void DrawingCanvas::SelectAll()
{
    selectedWire = -1;
    sel.clear();
    for (size_t i = 0; i < components.size(); i++)
        sel.insert(components[i].id);
    NotifyChanged();
}

void DrawingCanvas::GetSelectedIds(wxVector<int>& out) const
{
    out.clear();
    for (std::set<int>::const_iterator it = sel.begin(); it != sel.end(); ++it)
        out.push_back(*it);
}

// ================================================================
// 复制/剪切/粘贴(支持多个;粘贴到偏移 +30 并自动分配新编号)
// ================================================================
void DrawingCanvas::CopySelected()
{
    clipComps.clear();
    clipWires.clear();
    for (size_t i = 0; i < components.size(); i++)
        if (sel.count(components[i].id))
            clipComps.push_back(components[i]);
    for (const auto& wire : wires)
        if (sel.count(wire.comp1) && sel.count(wire.comp2)) clipWires.push_back(wire);
}

void DrawingCanvas::CutSelected()
{
    if (simRunning) { Hint("请先停止仿真再剪切"); return; }
    CopySelected();
    DeleteSelected();
}

void DrawingCanvas::PasteClipboard()
{
    if (simRunning) { Hint("请先停止仿真再粘贴"); return; }
    if (clipComps.size() == 0) return;
    auto candidate=components;auto candidateWires=wires;int candidateNext=nextId;
    std::map<int,int> candidateIds;
    for(auto c:clipComps){candidateIds[c.id]=candidateNext;c.id=candidateNext++;candidate.push_back(c);}
    for(auto w:clipWires){w.comp1=candidateIds[w.comp1];w.comp2=candidateIds[w.comp2];candidateWires.push_back(w);}
    if(!FitsDocument(candidate,candidateWires,moduleDefs)){Hint("粘贴后工程超过存储限制，请减少子电路实例");return;}
    PushUndo();
    selectedWire = -1;
    sel.clear();
    std::map<int,int> pastedIds;
    for (size_t i = 0; i < clipComps.size(); i++) {
        Component c = clipComps[i];
        const int oldId = c.id;
        c.id = nextId++;
        pastedIds[oldId] = c.id;
        c.x += 30;          // 往右下偏一点,不和原件重叠
        c.y += 30;
        components.push_back(c);
        sel.insert(c.id);   // 粘贴出来的直接选中,方便接着拖
    }
    for (auto w : clipWires) {
        w.comp1 = pastedIds[w.comp1]; w.comp2 = pastedIds[w.comp2];
        for (auto& p : w.bends) p += wxPoint(30,30);
        wires.push_back(w);
    }
    dirty = true;
    NotifyChanged();
}

void DrawingCanvas::NotifyChanged()
{
    if (placeType == GATE_CUSTOM) {
        bool found=false;
        for (const auto& d : customDefs) if (d.name == placeCustomName) { placeCustomTruth=d.truth; found=true; break; }
        if (!found) { placeType=GATE_AND; selectMode=true; wireMode=false; CancelWirePreview(); }
    }
    if(placeType==GATE_MODULE) {
        ModulePtr current;
        if(placeModule)for(const auto& d:moduleDefs)if(d->name==placeModule->name){current=d;break;}
        if(current)placeModule=current;
        else {placeModule.reset();placeType=GATE_AND;selectMode=true;wireMode=false;CancelWirePreview();}
    }
    dirty = CircuitToJson(components, wires, customDefs, moduleDefs) != savedSnapshot;
    InvalidateScene();                   // Content or selection changed: rebuild cached scene.
    if (onSelection) onSelection();     // 通知外面(属性表)刷新
}

// ================================================================
// 画图:静态电路缓存 + 双缓冲预览;内容变化时才重建静态图
// ================================================================
void DrawingCanvas::InvalidateScene()
{
    sceneDirty = true;
    Refresh(false);
}

void DrawingCanvas::OnSize(wxSizeEvent& e)
{
    InvalidateScene();
    e.Skip();
}

void DrawingCanvas::CancelWirePreview()
{
    const bool hadPreview = wireFromComp != -1;
    wireFromComp = wireFromPin = -1;
    wireBends.clear();
    if (hadPreview) Refresh(false);
}

void DrawingCanvas::RefreshWirePreview(const wxPoint& endpoint)
{
    if (SnapGrid(endpoint) == wireEnd) return;
    Component* source = FindById(wireFromComp);
    if (!source) { CancelWirePreview(); return; }
    const wxPoint start = GetPinPos(*source, wireFromPin);
    auto refreshPath = [&](const wxPoint& end) {
        const auto path = BuildWirePath(start, wireBends, end);
        for (size_t i = 1; i < path.size(); ++i)
            RefreshRect(wxRect(WorldToScreen(path[i-1]), WorldToScreen(path[i])).Inflate(8), false);
    };
    refreshPath(wireEnd);
    wireEnd = SnapGrid(endpoint);
    refreshPath(wireEnd);
}

void DrawingCanvas::OnPaint(wxPaintEvent&)
{
    wxAutoBufferedPaintDC dc(this);
    const wxSize size = GetClientSize();
    if (size.x <= 0 || size.y <= 0) return;
    const double scale = GetContentScaleFactor();
    if (!sceneBitmap.IsOk() || sceneSize != size || sceneScale != scale) {
        sceneBitmap.CreateWithDIPSize(size, scale);
        sceneSize = size;
        sceneScale = scale;
        sceneDirty = true;
    }
    if (sceneDirty) {
        wxMemoryDC sceneDC(sceneBitmap);
        sceneDC.SetFont(GetFont());
        sceneDC.SetUserScale(viewZoom, viewZoom);
        sceneDC.SetDeviceOrigin(viewPan.x, viewPan.y);
        DrawScene(sceneDC);
        sceneDC.SelectObject(wxNullBitmap);
        sceneDirty = false;
    }
    dc.DrawBitmap(sceneBitmap, 0, 0, false);
    dc.SetUserScale(viewZoom, viewZoom);
    dc.SetDeviceOrigin(viewPan.x, viewPan.y);
    // 连线模式下的橡皮筋预览:灰色虚线跟着鼠标走
    if (wireMode && wireFromComp != -1) {
        Component* a = FindById(wireFromComp);
        if (a) {
            dc.SetPen(wxPen(*wxLIGHT_GREY, 1, wxPENSTYLE_SHORT_DASH));
            const auto path = BuildWirePath(GetPinPos(*a, wireFromPin), wireBends, wireEnd);
            if (path.size() > 1) dc.DrawLines((int)path.size(), &path[0]);
        }
    }

    // 框选中的选择框:蓝色虚线矩形
    if (rubberBand) {
        dc.SetPen(wxPen(*wxBLUE, 1, wxPENSTYLE_SHORT_DASH));
        dc.SetBrush(*wxTRANSPARENT_BRUSH);
        int x1 = std::min(rbStart.x, rbEnd.x), x2 = std::max(rbStart.x, rbEnd.x);
        int y1 = std::min(rbStart.y, rbEnd.y), y2 = std::max(rbStart.y, rbEnd.y);
        dc.DrawRectangle(x1, y1, x2 - x1, y2 - y1);
    }

}

void DrawingCanvas::DrawScene(wxDC& dc)
{
    dc.SetBackground(*wxWHITE_BRUSH);
    dc.Clear();

    const wxPoint top = ScreenToWorld(wxPoint(0,0));
    const wxSize size = GetClientSize();
    const wxPoint bottom = ScreenToWorld(wxPoint(size.x,size.y));
    const int left = (int)std::floor((double)top.x/CircuitGrid)*CircuitGrid;
    const int upper = (int)std::floor((double)top.y/CircuitGrid)*CircuitGrid;
    for (int x=left; x<=bottom.x+CircuitGrid; x+=CircuitGrid) {
        dc.SetPen(wxPen(x%50==0 ? wxColour(208,220,230) : wxColour(235,241,245)));
        dc.DrawLine(x,upper,x,bottom.y+CircuitGrid);
    }
    for (int y=upper; y<=bottom.y+CircuitGrid; y+=CircuitGrid) {
        dc.SetPen(wxPen(y%50==0 ? wxColour(208,220,230) : wxColour(235,241,245)));
        dc.DrawLine(left,y,bottom.x+CircuitGrid,y);
    }

    // 先画连线(在元件下层)
    for (size_t i = 0; i < wires.size(); i++) {
        Component* a = FindById(wires[i].comp1);
        Component* b = FindById(wires[i].comp2);
        if (!a || !b) continue;
        // 仿真中:高电位的线画红色,低电位画黑色(Logisim 风格)
        if ((int)i == selectedWire)
            dc.SetPen(wxPen(wxColour(0,126,189), 3));
        else if (simRunning && GetOutputPinValue(wires[i].comp1, wires[i].pin1) == 1)
            dc.SetPen(wxPen(*wxRED, 2));
        else
            dc.SetPen(wxPen(*wxBLACK, 2));
        const auto path = BuildWirePath(GetPinPos(*a, wires[i].pin1), wires[i].bends, GetPinPos(*b, wires[i].pin2));
        if (path.size() > 1) dc.DrawLines((int)path.size(), &path[0]);
    }

    // 再画元件(选中的会被 DrawGate 用蓝框标出)
    for (size_t i = 0; i < components.size(); i++)
        DrawGate(dc, components[i]);
}

// ================================================================
// 鼠标左键按下:仿真模式切开关;连线模式走引脚逻辑;
//   否则 点中元件=选中(可拖动),点空白=放置(放置模式)或清空选择(选择模式)
// ================================================================
void DrawingCanvas::OnLeftDown(wxMouseEvent& e)
{
    const wxPoint point = ScreenToWorld(e.GetPosition());
    if(readOnly)return;
    SetFocus();   // 让画布获得键盘焦点,Delete 键才有效

    // ----- 仿真运行中:点到开关 = 切换它的开/关,然后重算整个电路 -----
    if (simRunning) {
        int id = HitTest(point.x, point.y);
        Component* c = (id != -1) ? FindById(id) : nullptr;
        if (c && c->type == SW_INPUT) {
            c->state = !c->state;   // 切换!
            dirty = true;
            SimTick();               // 前进一个仿真步(不是 RunSim,保持时序状态)
        } else {
            Hint("仿真运行中:点开关切换，时钟自动步进；停止仿真后才能编辑");
        }
        return;
    }

    // ----- 连线模式 -----
    if (wireMode) {
        int cid, pin;
        if (!HitPin(point.x, point.y, &cid, &pin)) {
            if (wireFromComp != -1) {
                const wxPoint waypoint = SnapGrid(point);
                const wxPoint previous = wireBends.empty() ? GetPinPos(*FindById(wireFromComp), wireFromPin) : wireBends.back();
                if (waypoint != previous) wireBends.push_back(waypoint);
                wireEnd = waypoint;
                Refresh(false);
                Hint("已添加拐点：继续点网格添加，点目标引脚完成，Esc 取消");
            }
            return;
        }

        if (wireFromComp == -1) {
            wireBends.clear();
            wireFromComp = cid;      // 第一下:选定起点引脚
            wireFromPin = pin;
            wireEnd = GetPinPos(*FindById(cid), pin); // Never reuse an earlier endpoint.
        } else {
            if (cid == wireFromComp && pin == wireFromPin) return;
            Wire w = { wireFromComp, wireFromPin, cid, pin };
            w.bends = wireBends;
            wxString error;
            if (!NormalizeConnection(components, wires, w, &error)) { Hint(error); return; }
            PushUndo();
            wires.push_back(w);
            dirty = true;
            CancelWirePreview();     // 可以继续连下一根
            NotifyChanged();
        }
        Refresh(false);
        return;
    }

    // ----- 普通/选择模式 -----
    int id = HitTest(point.x, point.y);
    bool add = e.ShiftDown() || e.ControlDown();   // Shift/Ctrl = 加选/取消选择

    if (id != -1) {
        selectedWire = -1;
        if (add) {
            // 加选:已经在集合里就移出(再点一次 = 取消选中)
            if (sel.count(id)) sel.erase(id);
            else               sel.insert(id);
            NotifyChanged();
            return;
        }
        // 选中它(如果它不在已选集合里,就改成只选它),然后开始拖动
        if (!sel.count(id)) {
            selectedWire = -1;
    sel.clear();
            sel.insert(id);
        }
        dragging = true;
        dragItems.clear();
        for (std::set<int>::iterator it = sel.begin(); it != sel.end(); ++it) {
            Component* c = FindById(*it);
            if (!c) continue;
            DragItem d;
            d.id = *it;
            d.offX = point.x - c->x;
            d.offY = point.y - c->y;
            dragItems.push_back(d);
        }
        // 先不急着入撤销栈(可能只是点一下没拖);存个快照,松手时若移动了再入栈
        dragStartSnapshot = CircuitToJson(components, wires, customDefs, moduleDefs);
        CaptureMouse();   // 拖出画布外鼠标消息也归我们,松开才不会丢
        NotifyChanged();
        return;
    }

    if (selectMode) {
        int hit = HitWire(point);
        if (hit >= 0) { sel.clear(); selectedWire = hit; NotifyChanged(); return; }
    }
    // 点在空白处
    if (selectMode) {
        selectedWire = -1;
        // 选择模式:开始拉框(Shift/Ctrl 则保留原来的选择,做"追加框选")
        if (!add) sel.clear();
        rubberBand = true;
        rbStart = wxPoint(point.x, point.y);
        rbEnd = rbStart;
        CaptureMouse();
        NotifyChanged();
        return;
    }

    // 放置模式:放一个当前类型的元件
    if(placeType==GATE_MODULE && !placeModule){Hint("请先选择子电路元件");return;}
    auto candidate=components;Component proposed;proposed.id=nextId;proposed.type=placeType;proposed.module=placeModule;
    candidate.push_back(proposed);
    if(!FitsDocument(candidate,wires,moduleDefs)){Hint("工程超过存储限制，请减少子电路实例");return;}
    PushUndo();
    Component c;
    c.type = placeType;
    const wxPoint placed = SnapGrid(point);
    c.x = placed.x;
    c.y = placed.y;
    c.id = nextId++;
    if (placeType == GATE_MODULE) { c.module=placeModule; if(!c.module) { Hint("请先从子电路元件库选择元件"); return; } c.customName=c.module->name; }
    if (placeType == GATE_CUSTOM) {
        c.customName = placeCustomName;
        c.truth = placeCustomTruth;
    }
    components.push_back(c);
    selectedWire = -1;
    sel.clear();
    dirty = true;
    NotifyChanged();
}

// 拖动中:所有选中的元件跟着鼠标走;框选中:框跟着鼠标走;连线模式:预览线跟着走
void DrawingCanvas::OnMotion(wxMouseEvent& e)
{
    const wxPoint point = ScreenToWorld(e.GetPosition());
    if (panning) { viewPan = panOrigin + (e.GetPosition()-panStart); InvalidateScene(); return; }
    if (simRunning) return;
    if (rubberBand) {
        rbEnd = wxPoint(point.x, point.y);
        Refresh(false);
        return;
    }
    if (wireMode && wireFromComp != -1) {
        RefreshWirePreview(point);
        return;
    }
    if (!dragging)
        return;
    for (size_t i = 0; i < dragItems.size(); i++) {
        Component* c = FindById(dragItems[i].id);
        if (!c) continue;
        const wxPoint moved = SnapGrid(wxPoint(point.x - dragItems[i].offX, point.y - dragItems[i].offY));
        c->x = moved.x;
        c->y = moved.y;
    }
    InvalidateScene();
}

void DrawingCanvas::OnLeftUp(wxMouseEvent&)
{
    // 框选结束:把落在框里的元件选中
    if (rubberBand) {
        rubberBand = false;
        if (HasCapture()) ReleaseMouse();
        int x1 = std::min(rbStart.x, rbEnd.x), x2 = std::max(rbStart.x, rbEnd.x);
        int y1 = std::min(rbStart.y, rbEnd.y), y2 = std::max(rbStart.y, rbEnd.y);
        if (x2 - x1 > 3 || y2 - y1 > 3) {           // 太小的框当作"点一下"处理
            for (size_t i = 0; i < components.size(); i++)
                if (InRect(components[i], x1, y1, x2, y2))
                    sel.insert(components[i].id);
        }
        NotifyChanged();
        return;
    }

    if (dragging) {
        dragging = false;
        if (HasCapture()) ReleaseMouse();
        // 真的移动过才记一步撤销(纯点选不污染撤销链)
        if (dragStartSnapshot != CircuitToJson(components, wires, customDefs, moduleDefs)) {
            CommitSnapshot(dragStartSnapshot);
            dirty = true;
        }
        NotifyChanged();
    }
}

// 元件是否落在选择框里(用元件的方框和选择框求交)
bool DrawingCanvas::InRect(const Component& c, int x1, int y1, int x2, int y2) const
{
    int r = (c.type == GATE_CUSTOM) ? 26 : 25;
    const int rx=c.type==GATE_MODULE?75:r,ry=c.type==GATE_MODULE?ModuleHeight(c)/2+4:r;
    return !(c.x + rx < x1 || c.x - rx > x2 || c.y + ry < y1 || c.y - ry > y2);
}

// Delete 删除选中元件;Esc 取消选择
void DrawingCanvas::OnKeyDown(wxKeyEvent& e)
{
    if (e.GetKeyCode() == WXK_DELETE) {
        if (!DeleteSelected())
            Hint("没有可删除的元件:先点选元件(变蓝框),仿真中请先停止仿真");
    } else if (e.GetKeyCode() == WXK_ESCAPE) {
        if (dragging) {
            int next=1;
            JsonToCircuit(std::string(dragStartSnapshot.utf8_str()),components,wires,next,&customDefs,&moduleDefs);
            nextId=next;
        }
        dragging=rubberBand=panning=false;
        if(HasCapture())ReleaseMouse();
        CancelWirePreview();
        selectedWire = -1;
        sel.clear();
        NotifyChanged();
    } else {
        e.Skip();
    }
}

// ================================================================
// 新建/保存/打开:数据在画布上,文件读写交给 CircuitFile.h
// ================================================================
void DrawingCanvas::NewDocument()
{
    ResetView();
    components.clear();
    customDefs.clear();
    moduleDefs.clear();
    placeModule.reset();
    wires.clear();
    nextId = 1;
    selectedWire = -1;
    sel.clear();
    wireMode = false;
    wireFromComp = -1;
    simRunning = false;
    simOut.clear(); simOut.pins.clear();
    undoStack.clear();
    redoStack.clear();
    filePath = "";
    savedSnapshot = CircuitToJson(components, wires, customDefs, moduleDefs);
    dirty = false;
    NotifyChanged();
}

bool DrawingCanvas::SaveFile(const wxString& path)
{
    if (!SaveCircuit(path, components, wires, customDefs, moduleDefs))
        return false;
    filePath = path;   // 记住存到哪了,下次 Ctrl+S 直接覆盖
    savedSnapshot = CircuitToJson(components, wires, customDefs, moduleDefs);
    dirty = false;     // 存过盘了,没有未保存修改
    return true;
}

bool DrawingCanvas::LoadFile(const wxString& path)
{
    wxVector<CustomDef> loadedDefs;
    ModuleLibrary loadedModules;
    if (!LoadCircuit(path, components, wires, nextId, &loadedDefs, &loadedModules))
        return false;
    ResetView();
    customDefs = loadedDefs;
    moduleDefs = loadedModules;
    selectedWire = -1;
    sel.clear();           // 旧电路的选中状态全部作废
    wireMode = false;
    wireFromComp = -1;
    simRunning = false;
    simOut.clear(); simOut.pins.clear();
    undoStack.clear();     // 换了一份图纸,旧的撤销历史没有意义
    redoStack.clear();
    filePath = path;
    savedSnapshot = CircuitToJson(components, wires, customDefs, moduleDefs);
    dirty = false;         // 刚打开的图纸是干净的
    NotifyChanged();
    return true;
}

bool DrawingCanvas::ImportNetlist(const wxString& path)
{
    wxVector<Component> loaded;
    wxVector<Wire> imported, normalized;
    if (!LoadNetlist(path, loaded, imported)) return false;
    for (auto wire : imported) {
        if (!NormalizeConnection(loaded, normalized, wire)) return false;
        normalized.push_back(wire);
    }
    components=loaded; wires=normalized;
    ResetView();
    customDefs.clear();
    moduleDefs.clear();
    placeModule.reset();
    for (const auto& c : components) if (c.type == GATE_CUSTOM) {
        bool known=false;for(const auto& d:customDefs)if(d.name==c.customName)known=true;
        if(!known){CustomDef d;d.name=c.customName;d.truth=c.truth;customDefs.push_back(d);}
    }
    savedSnapshot.clear(); // An imported netlist is a new unsaved document.
    int maxId = 0;
    for (size_t i = 0; i < components.size(); i++)
        if (components[i].id > maxId) maxId = components[i].id;
    nextId = maxId + 1;
    selectedWire = -1;
    sel.clear();
    wireMode = false;
    wireFromComp = -1;
    simRunning = false;
    simOut.clear(); simOut.pins.clear();
    undoStack.clear();
    redoStack.clear();
    filePath = "";   // 网表不是工程文件,之后另存为 .eda
    dirty = true;    // 导入出来的电路还没存过盘
    NotifyChanged();
    return true;
}

// ================================================================
// 删除所有选中元件(它身上的连线也要一起删,不然连线会"悬空"指向不存在的元件)
// ================================================================
bool DrawingCanvas::DeleteSelected()
{
    if(readOnly)return false;
    if (simRunning) {
        Hint("仿真运行中不能删除元件,请先停止仿真(工具栏[仿真]或 F6)");
        return false;
    }
    if (selectedWire >= 0 && (size_t)selectedWire < wires.size()) {
        PushUndo(); wires.erase(wires.begin()+selectedWire); selectedWire=-1; NotifyChanged(); return true;
    }
    if (sel.empty()) {
        Hint("请先点选元件(选中后元件变蓝框),再删除");
        return false;
    }
    PushUndo();

    // 从后往前删元件,避免删除时下标错位
    for (int i = (int)components.size() - 1; i >= 0; i--) {
        if (!sel.count(components[i].id)) continue;
        int goneId = components[i].id;
        components.erase(components.begin() + i);
        // 顺带删掉挂在这个元件上的连线
        for (int j = (int)wires.size() - 1; j >= 0; j--) {
            if (wires[j].comp1 == goneId || wires[j].comp2 == goneId)
                wires.erase(wires.begin() + j);
        }
    }
    selectedWire = -1;
    sel.clear();
    dirty = true;
    NotifyChanged();
    return true;
}

// ================================================================
// 原理图位号重排(Annotate Schematics):
// 遵循工业标准,按几何空间(从上到下、从左到右)将元件重新顺次编号为 U1, U2, U3...
// 同步更新所有连接线(Wire)中的元件 ID 引用,并支持 Ctrl+Z 一键撤销
// ================================================================
bool DrawingCanvas::AnnotateComponents()
{
    if (readOnly) return false;
    if (simRunning) {
        Hint("仿真运行中不能重新编号元件，请先停止仿真");
        return false;
    }
    if (components.empty()) {
        Hint("画布上没有元件");
        return false;
    }

    // 垂直方向按 40 像素作为行容差带(同一行内的元件按 X 坐标从左到右编号)
    constexpr int RowBand = 40;
    std::vector<size_t> indices(components.size());
    for (size_t i = 0; i < components.size(); ++i) indices[i] = i;

    std::sort(indices.begin(), indices.end(), [this, RowBand](size_t i1, size_t i2) {
        const auto& a = components[i1];
        const auto& b = components[i2];
        int rowA = (int)std::floor((double)a.y / RowBand);
        int rowB = (int)std::floor((double)b.y / RowBand);
        if (rowA != rowB) return rowA < rowB;
        if (a.x != b.x) return a.x < b.x;
        return a.id < b.id;
    });

    std::map<int, int> idMap;
    int curId = 1;
    bool alreadySequential = true;
    for (size_t idx : indices) {
        idMap[components[idx].id] = curId;
        if (components[idx].id != curId) {
            alreadySequential = false;
        }
        curId++;
    }

    if (alreadySequential && nextId == curId) {
        Hint("元件位号已是最佳连续顺序，无需重排");
        return true;
    }

    PushUndo();

    // 1. 更新所有元件的 ID
    for (auto& c : components) {
        c.id = idMap[c.id];
    }

    // 2. 更新所有连线引用的元件 ID
    for (auto& w : wires) {
        auto it1 = idMap.find(w.comp1);
        if (it1 != idMap.end()) w.comp1 = it1->second;
        auto it2 = idMap.find(w.comp2);
        if (it2 != idMap.end()) w.comp2 = it2->second;
    }

    // 3. 更新当前选中的元件 ID 集合
    if (!sel.empty()) {
        std::set<int> newSel;
        for (int oldId : sel) {
            auto it = idMap.find(oldId);
            if (it != idMap.end()) newSel.insert(it->second);
        }
        sel = newSel;
    }

    // 4. 重置下一个分配 ID
    nextId = (int)components.size() + 1;
    dirty = true;
    NotifyChanged();

    Hint(wxString::Format("已完成原理图位号重排，共重排 %d 个元件(U1~U%d)",
                          (int)components.size(), (int)components.size()));
    return true;
}

// ================================================================
// 命中测试:这个点落在哪个元件上?(用包围盒粗略判断)
// ================================================================
int DrawingCanvas::HitTest(int mx, int my)
{
    // 从后往前找:后画的在上面,先点中"最上层"的
    for (int i = (int)components.size() - 1; i >= 0; i--) {
        const Component& c = components[i];
        int r = (c.type == GATE_CUSTOM) ? 26 : 25;   // 自定义元件的方框略大一点
        const int rx=c.type==GATE_MODULE?75:r,ry=c.type==GATE_MODULE?ModuleHeight(c)/2+4:r;
        if (mx >= c.x - rx && mx <= c.x + rx && my >= c.y - ry && my <= c.y + ry)
            return c.id;
    }
    return -1;
}

// ================================================================
// 命中测试:这个点落在哪个引脚上?(引脚周围 9 像素内算命中)
// ================================================================
bool DrawingCanvas::HitPin(int mx, int my, int* compId, int* pin)
{
    for (int i = (int)components.size() - 1; i >= 0; i--) {
        Component& c = components[i];
        for (int p = 0; p < GatePinCount(c); p++) {
            wxPoint pp = GetPinPos(c, p);
            int dx = mx - pp.x, dy = my - pp.y;
            if (dx * dx + dy * dy <= (9.0/viewZoom)*(9.0/viewZoom)) {
                *compId = c.id;
                *pin = p;
                return true;
            }
        }
    }
    return false;
}

Component* DrawingCanvas::FindById(int id)
{
    for (size_t i = 0; i < components.size(); i++)
        if (components[i].id == id)
            return &components[i];
    return nullptr;
}

const Component* DrawingCanvas::GetSelected() const
{
    if (sel.size() != 1) return nullptr;   // 多选时不返回单个(属性表会显示"N 个元件")
    int id = *sel.begin();
    for (size_t i = 0; i < components.size(); i++)
        if (components[i].id == id)
            return &components[i];
    return nullptr;
}

// ================================================================
// 画一个门电路符号(与门=D形,或门=弯月形,非门=三角+小圆圈,自定义=方框)
// 选中状态的元件用蓝色描边
// ================================================================
void DrawingCanvas::DrawGate(wxDC& dc, const Component& c)
{
    bool isSel = sel.count(c.id) > 0;
    dc.SetPen(isSel ? wxPen(*wxBLUE, 2) : wxPen(*wxBLACK, 1));
    dc.SetBrush(*wxWHITE_BRUSH);
    dc.SetTextForeground(*wxBLACK);

    int x = c.x, y = c.y;

    // Smooth vector outlines fit inside the existing grid-aligned pin positions.
    const bool andShape = c.type == GATE_AND || c.type == GATE_NAND;
    const bool orShape = c.type == GATE_OR || c.type == GATE_XOR || c.type == GATE_NOR;
    bool vectorPainted = false;
    if (andShape || orShape) {
        std::unique_ptr<wxGraphicsContext> gc(wxGraphicsContext::CreateFromUnknownDC(dc));
        if (gc) {
            gc->SetAntialiasMode(wxANTIALIAS_DEFAULT);
            gc->SetPen(wxPen(isSel ? wxColour(0,126,189) : wxColour(42,64,82), isSel ? 2 : 1));
            gc->SetBrush(wxBrush(isSel ? wxColour(229,246,255) : wxColour(253,254,255)));
            auto body = gc->CreatePath();
            if (andShape) {
                body.MoveToPoint(x-14,y-17);
                body.AddLineToPoint(x-2,y-17);
                body.AddCurveToPoint(x+18,y-17,x+18,y+17,x-2,y+17);
                body.AddLineToPoint(x-14,y+17);
                body.CloseSubpath();
            } else {
                body.MoveToPoint(x-15,y-17);
                body.AddCurveToPoint(x-1,y-18,x+8,y-11,x+14,y);
                body.AddCurveToPoint(x+8,y+11,x-1,y+18,x-15,y+17);
                body.AddCurveToPoint(x-5,y+6,x-5,y-6,x-15,y-17);
                body.CloseSubpath();
            }
            gc->DrawPath(body);
            if (c.type == GATE_XOR) {
                auto extra = gc->CreatePath();
                extra.MoveToPoint(x-20,y-17);
                extra.AddCurveToPoint(x-10,y-6,x-10,y+6,x-20,y+17);
                gc->StrokePath(extra);
            }
            const bool inverted = c.type == GATE_NAND || c.type == GATE_NOR;
            if (inverted) gc->DrawEllipse(x+14,y-3,6,6);
            auto leads = gc->CreatePath();
            for (int pin=0; pin<2; ++pin) {
                const wxPoint port = GetPinPos(c,pin);
                const int bodyX = andShape ? x-14 : x-10;
                leads.MoveToPoint(port.x,port.y);
                leads.AddLineToPoint(bodyX,port.y);
            }
            const wxPoint output = GetPinPos(c,2);
            leads.MoveToPoint(inverted ? x+20 : x+14,y);
            leads.AddLineToPoint(output.x,y);
            leads.AddLineToPoint(output.x,output.y);
            gc->StrokePath(leads);
            vectorPainted = true;
        }
    }
    if (vectorPainted) {
        // Pin markers and the label are shared with the remaining component types.
    } else if (c.type == GATE_AND || c.type == GATE_NAND) {
        // 与门/与非门:D 形;与非门右侧多一个小圆圈
        dc.DrawLine(x - 20, y - 20, x - 20, y + 20);
        dc.DrawLine(x - 20, y - 20, x, y - 20);
        dc.DrawLine(x - 20, y + 20, x, y + 20);
        dc.DrawArc(x, y + 20, x, y - 20, x, y);
        if (c.type == GATE_NAND) dc.DrawCircle(x + 24, y, 4);
    } else if (c.type == GATE_OR || c.type == GATE_XOR || c.type == GATE_NOR) {
        // 或门/异或门/或非门:折线弯月形;异或门左边多一条凹线,或非门右侧多小圆圈
        wxPoint pts[6] = {
            wxPoint(x - 22, y - 18),   // 左上
            wxPoint(x + 8,  y - 18),   // 上边
            wxPoint(x + 20, y),        // 右尖
            wxPoint(x + 8,  y + 18),   // 下边
            wxPoint(x - 22, y + 18),   // 左下
            wxPoint(x - 12, y),        // 左边中点(往右凹)
        };
        dc.DrawPolygon(6, pts);
        if (c.type == GATE_XOR) {
            // 异或门的"第二条输入线":在左边再画一条小凹折线
            wxPoint pts2[4] = {
                wxPoint(x - 30, y - 18),
                wxPoint(x - 22, y - 6),
                wxPoint(x - 22, y + 6),
                wxPoint(x - 30, y + 18),
            };
            dc.DrawLines(4, pts2);
        }
        if (c.type == GATE_NOR) dc.DrawCircle(x + 25, y, 4);
    } else if (c.type == GATE_NOT) {
        // 非门:三角形 + 右边小圆圈
        dc.DrawLine(x - 16, y - 14, x - 16, y + 14);
        dc.DrawLine(x - 16, y - 14, x + 10, y);
        dc.DrawLine(x - 16, y + 14, x + 10, y);
        dc.DrawCircle(x + 15, y, 4);
    } else if (c.type == GATE_MODULE && c.module) {
        const int height=ModuleHeight(c);
        dc.SetPen(wxPen(isSel?wxColour(0,126,189):wxColour(42,83,119),2));
        dc.SetBrush(wxBrush(isSel?wxColour(229,246,255):wxColour(245,250,255)));
        dc.DrawRoundedRectangle(x-60,y-height/2,120,height,6);
        const wxFont original=dc.GetFont();dc.SetFont(original.Bold());
        wxString title=c.module->name;
        while(title.size()>1 && dc.GetTextExtent(title).x>108)title.RemoveLast();
        dc.DrawText(title,x-dc.GetTextExtent(title).x/2,y-height/2+5);dc.SetFont(original);
        dc.SetPen(wxPen(wxColour(202,222,237)));dc.DrawLine(x-54,y-height/2+25,x+54,y-height/2+25);
        dc.SetPen(wxPen(isSel?wxColour(0,126,189):wxColour(42,83,119),1));
        const int inputs=(int)c.module->inputNames.size();
        for(int pin=0;pin<GatePinCount(c);++pin) {
            const auto port=GetPinPos(c,pin);const bool in=pin<inputs;
            dc.DrawLine(port,wxPoint(in?x-60:x+60,port.y));
            wxString name=in?c.module->inputNames[pin]:c.module->outputNames[pin-inputs];
            while(name.size()>1 && dc.GetTextExtent(name).x>46)name.RemoveLast();
            dc.DrawText(name,in?x-54:x+54-dc.GetTextExtent(name).x,port.y-7);
        }
    } else if (c.type == GATE_CUSTOM) {
        // 自定义元件:方框 + 名字(名字太长就截断,画得下为止)
        dc.SetBrush(wxBrush(wxColour(245, 245, 200)));   // 淡黄底色,和内置门区分开
        dc.DrawRoundedRectangle(x - 24, y - 24, 48, 48, 4);
        dc.SetBrush(*wxWHITE_BRUSH);
        wxString nm = c.customName.empty() ? "自定义" : c.customName;
        if (nm.length() > 3) nm = nm.Left(3);
        int w = 0, h = 0;
        dc.GetTextExtent(nm, &w, &h);
        dc.DrawText(nm, x - w / 2, y - h / 2);
    } else if (c.type == GATE_DFF || c.type == SW_CLOCK) {
        const bool flipFlop=c.type==GATE_DFF;
        const bool high=simRunning && GetOutputPinValue(c.id,flipFlop?2:0)!=0;
        const wxColour outline=isSel?wxColour(0,126,189):(high?wxColour(24,133,104):wxColour(42,83,119));
        const wxColour fill=isSel?wxColour(229,246,255):(high?wxColour(231,249,241):wxColour(244,249,255));
        dc.SetPen(wxPen(outline,1));dc.SetBrush(wxBrush(fill));
        std::unique_ptr<wxGraphicsContext> gc(wxGraphicsContext::CreateFromUnknownDC(dc));
        if(gc) {
            gc->SetAntialiasMode(wxANTIALIAS_DEFAULT);
            gc->SetPen(wxPen(outline,1));gc->SetBrush(wxBrush(fill));
            gc->DrawRoundedRectangle(x-(flipFlop?14:16),y-(flipFlop?18:14),flipFlop?28:32,flipFlop?36:28,3);
        } else dc.DrawRoundedRectangle(x-(flipFlop?14:16),y-(flipFlop?18:14),flipFlop?28:32,flipFlop?36:28,3);
        dc.SetTextForeground(outline);
        const auto originalFont=dc.GetFont();
        if(flipFlop) {
            const auto data=GetPinPos(c,0),clock=GetPinPos(c,1),q=GetPinPos(c,2);
            dc.DrawLine(data,wxPoint(x-14,data.y));
            dc.DrawLine(wxPoint(x+14,q.y),q);
            dc.DrawLine(wxPoint(x,y+18),clock);
            // Triangle at the clock terminal denotes rising-edge sampling.
            dc.DrawLine(x-4,y+18,x,y+12);dc.DrawLine(x,y+12,x+4,y+18);
            dc.SetFont(originalFont.Scaled(0.85).Bold());
            dc.DrawText("D",x-10,y-16);dc.DrawText("Q",x+4,y-7);
            dc.SetFont(originalFont.Scaled(0.65));dc.DrawText("CLK",x-11,y+2);
        } else {
            dc.DrawLine(wxPoint(x+16,y),GetPinPos(c,0));
            dc.SetPen(wxPen(outline,2));
            wxPoint wave[]={wxPoint(x-11,y+5),wxPoint(x-6,y+5),wxPoint(x-6,y-5),wxPoint(x+2,y-5),wxPoint(x+2,y+5),wxPoint(x+10,y+5),wxPoint(x+10,y-5)};
            dc.DrawLines(7,wave);
            dc.SetFont(originalFont.Scaled(0.85));
            dc.DrawText("CLK",x-dc.GetTextExtent("CLK").x/2,y+18);
        }
        dc.SetFont(originalFont);dc.SetTextForeground(*wxBLACK);
    } else if (c.type == SW_INPUT) {
        // 开关:圆角方框 + "1"/"0";闭合时底色变绿
        dc.SetBrush(c.state ? wxBrush(wxColour(144, 238, 144)) : *wxWHITE_BRUSH);
        dc.DrawRoundedRectangle(x - 18, y - 14, 36, 28, 4);
        dc.DrawText(c.state ? "1" : "0", x - 5, y - 9);
        dc.DrawLine(wxPoint(x+18,y),GetPinPos(c,0));
    } else {
        // 指示灯:圆;仿真中亮=红填充,灭=白
        int v = simRunning ? GetPinValue(c.id, 0) : 0;
        dc.SetBrush((simRunning && v == 1) ? *wxRED_BRUSH : *wxWHITE_BRUSH);
        dc.DrawCircle(x, y, 14);
        dc.DrawLine(GetPinPos(c,0),wxPoint(x-14,y));
    }

    dc.SetPen(wxPen(wxColour(62,91,116), 1));
    dc.SetBrush(*wxWHITE_BRUSH);
    for (int pin = 0; pin < GatePinCount(c); ++pin) {
        const wxPoint raw = GetRawPinPos(c, pin);
        const wxPoint snapped = GetPinPos(c, pin);
        if (!vectorPainted) {
            dc.DrawLine(raw, wxPoint(snapped.x, raw.y));
            dc.DrawLine(wxPoint(snapped.x, raw.y), snapped);
        }
        dc.DrawCircle(snapped, 2);
    }

    if(c.type==SW_INPUT || c.type==SW_LED) {
        const auto previousFont=dc.GetFont();dc.SetFont(previousFont.Scaled(0.85));
        dc.SetTextForeground(wxColour(62,91,116));
        const wxString port=c.type==SW_INPUT?"OUT":"IN";
        dc.DrawText(port,c.type==SW_INPUT?x+12:x-22-dc.GetTextExtent(port).x/2,y+17);
        dc.SetFont(previousFont);dc.SetTextForeground(*wxBLACK);
    }

    // 编号标签,比如 U1
    dc.DrawText(wxString::Format("U%d", c.id), x - 12, c.type==GATE_MODULE ? y-ModuleHeight(c)/2-18 : y-36);
}

// ================================================================
// 自定义元件管理:数一数画布上有几个用了这个名字的元件
// ================================================================
int DrawingCanvas::CountCustomInstances(const wxString& name) const
{
    int n = 0;
    for (size_t i = 0; i < components.size(); i++)
        if (components[i].type == GATE_CUSTOM && components[i].customName == name)
            n++;
    return n;
}

// 元件库里的定义改了(改名/改真值表)之后,把画布上已放置的同名元件一起更新,
// 否则"库"和"画布"就对不上了。改动前先存快照,可以撤销。
void DrawingCanvas::UpdateCustomInstances(const wxString& oldName, const wxString& newName, int truth)
{
    bool changed = false;
    for (size_t i = 0; i < components.size(); i++) {
        Component& c = components[i];
        if (c.type == GATE_CUSTOM && c.customName == oldName) {
            if (!changed) { PushUndo(); changed = true; }
            c.customName = newName;
            c.truth = truth;
        }
    }
    if (changed) {
        dirty = true;
        RunSim();     // 真值表变了,如果正在仿真要重算
        NotifyChanged();
    }
}

// ================================================================
// 仿真:跑一遍并重画
// ================================================================
void DrawingCanvas::RunSim()
{
    simOut = SimulateCircuit(components, wires);
    NotifyChanged();
}

// 前进一个仿真步:时钟翻转 + 组合门传播(触发器在下一个 Tick 捕获)
void DrawingCanvas::SimTick() {
    if(!simRunning)return;
    SimulationTick(components,wires,simOut,simTick,prevClk);
    NotifyChanged();
}

int DrawingCanvas::GetOutputValue(int compId) const
{
    auto it = simOut.find(compId);
    return (it != simOut.end()) ? it->second : 0;
}

// 某输入引脚连到的来源输出值(画灯的亮灭、属性表显示用)
int DrawingCanvas::GetPinValue(int compId, int pin) const
{
    return ReadInputSignal(compId,pin,wires,simOut.pins);
}


bool DrawingCanvas::AddCustomDefinition(const wxString& rawName, int truth) {
    wxString name=rawName; name.Trim(true).Trim(false);
    if(simRunning || name.empty() || truth<0 || truth>15) return false;
    for(const auto& def:customDefs) if(def.name==name) {Hint("自定义元件名称已存在");return false;}
    PushUndo();CustomDef def;def.name=name;def.truth=truth;customDefs.push_back(def);NotifyChanged();return true;
}
bool DrawingCanvas::EditCustomDefinition(const wxString& oldName,const wxString& rawName,int truth) {
    wxString name=rawName;name.Trim(true).Trim(false);
    if(simRunning || name.empty() || truth<0 || truth>15) return false;
    for(const auto& def:customDefs) if(def.name==name && name!=oldName) {Hint("自定义元件名称已存在");return false;}
    for(auto& def:customDefs) if(def.name==oldName) {
        if(def.name==name && def.truth==truth)return true;
        PushUndo();def.name=name;def.truth=truth;
        if(placeType==GATE_CUSTOM && placeCustomName==oldName){placeCustomName=name;placeCustomTruth=truth;}
        for(auto& c:components) if(c.type==GATE_CUSTOM && c.customName==oldName){c.customName=name;c.truth=truth;}
        NotifyChanged();return true;
    }
    return false;
}
bool DrawingCanvas::DeleteCustomDefinition(const wxString& name) {
    if(simRunning)return false;
    for(size_t i=0;i<customDefs.size();++i) if(customDefs[i].name==name) {
        PushUndo();customDefs.erase(customDefs.begin()+i);NotifyChanged();return true;
    }
    return false;
}
int DrawingCanvas::HitWire(const wxPoint& p) const {
    for(int i=(int)wires.size()-1;i>=0;--i) {
        auto a=CircuitComponent(components,wires[i].comp1), b=CircuitComponent(components,wires[i].comp2);
        if(!a || !b)continue;
        const auto path=BuildWirePath(GetPinPos(*a,wires[i].pin1),wires[i].bends,GetPinPos(*b,wires[i].pin2));
        for(size_t k=1;k<path.size();++k) {
            const auto a=path[k-1],b=path[k];
            const double tolerance=5.0/viewZoom;
            if(p.x>=std::min(a.x,b.x)-tolerance && p.x<=std::max(a.x,b.x)+tolerance && p.y>=std::min(a.y,b.y)-tolerance && p.y<=std::max(a.y,b.y)+tolerance) return i;
        }
    }
    return -1;
}


wxPoint DrawingCanvas::ScreenToWorld(const wxPoint& p) const {
    return wxPoint((int)std::lround((p.x-viewPan.x)/viewZoom),(int)std::lround((p.y-viewPan.y)/viewZoom));
}
wxPoint DrawingCanvas::WorldToScreen(const wxPoint& p) const {
    return wxPoint((int)std::lround(p.x*viewZoom+viewPan.x),(int)std::lround(p.y*viewZoom+viewPan.y));
}
void DrawingCanvas::OnWheel(wxMouseEvent& e) {
    if(dragging || rubberBand || panning)return;
    const double steps=(double)e.GetWheelRotation()/std::max(1,e.GetWheelDelta());
    const double next=std::clamp(viewZoom*std::pow(1.2,steps),0.4,3.0);
    const wxPoint cursor=e.GetPosition();
    const double worldX=(cursor.x-viewPan.x)/viewZoom,worldY=(cursor.y-viewPan.y)/viewZoom;
    viewPan=wxPoint((int)std::lround(cursor.x-worldX*next),(int)std::lround(cursor.y-worldY*next));
    viewZoom=next;InvalidateScene();
    Hint(wxString::Format("缩放 %d%%；中键拖动平移，Ctrl+0 复位",(int)std::lround(viewZoom*100)));
}
void DrawingCanvas::OnMiddleDown(wxMouseEvent& e) {
    if(dragging || rubberBand || HasCapture())return;
    panning=true;panStart=e.GetPosition();panOrigin=viewPan;CaptureMouse();
}
void DrawingCanvas::OnMiddleUp(wxMouseEvent&) {
    if(!panning)return;
    panning=false;if(HasCapture())ReleaseMouse();InvalidateScene();
}
void DrawingCanvas::ResetView() {
    const bool moved=dragging && dragStartSnapshot!=CircuitToJson(components,wires,customDefs,moduleDefs);
    if(moved) CommitSnapshot(dragStartSnapshot);
    dragging=rubberBand=panning=false;
    if(HasCapture())ReleaseMouse();
    viewZoom=1.0;viewPan=wxPoint(0,0);
    if(moved)NotifyChanged();else InvalidateScene();
}


int DrawingCanvas::GetOutputPinValue(int id,int pin) const {
    auto it=simOut.pins.find({id,pin});return it==simOut.pins.end()?0:it->second;
}
void DrawingCanvas::SelectedInterfaces(wxVector<int>& inputs,wxVector<int>& outputs) const {
    inputs.clear();outputs.clear();
    for(const auto& c:components)if(sel.count(c.id)){if(c.type==SW_INPUT)inputs.push_back(c.id);if(c.type==SW_LED)outputs.push_back(c.id);}
    std::sort(inputs.begin(),inputs.end());std::sort(outputs.begin(),outputs.end());
}
void DrawingCanvas::SetModulePlace(ModulePtr module) {
    if(simRunning || !module)return;
    CancelWirePreview();placeModule=module;placeType=GATE_MODULE;wireMode=selectMode=false;
    sel.clear();selectedWire=-1;NotifyChanged();
}
bool DrawingCanvas::PackSelection(const wxString& rawName,const wxVector<wxString>& inputs,const wxVector<wxString>& outputs,wxString* error) {
    if(simRunning){if(error)*error="请先停止仿真再封装";return false;}
    wxString name=rawName;name.Trim(true).Trim(false);
    for(const auto& d:moduleDefs)if(d->name==name){if(error)*error="子电路名称已存在";return false;}
    if(moduleDefs.size()>=256){if(error)*error="当前工程的子电路元件库已满";return false;}
    auto d=CreateSubcircuit(components,wires,sel,name,inputs,outputs,error);
    if(!d)return false;
    auto candidate=moduleDefs;candidate.push_back(d);
    if(!FitsDocument(components,wires,candidate)){if(error)*error="封装后工程超过存储限制，请减少重复嵌套";return false;}
    PushUndo();moduleDefs.push_back(d);NotifyChanged();return true;
}
bool DrawingCanvas::ExportModule(const wxString& path,size_t index) const {
    if(index>=moduleDefs.size())return false;
    ModuleLibrary one;one.push_back(moduleDefs[index]);
    return SaveCircuit(path,wxVector<Component>(),wxVector<Wire>(),wxVector<CustomDef>(),one);
}
bool DrawingCanvas::ImportModules(const wxString& path,wxString* error) {
    if(simRunning){if(error)*error="请先停止仿真再导入元件";return false;}
    wxVector<Component> cs;wxVector<Wire> ws;ModuleLibrary imported;int next=1;
    if(!LoadCircuit(path,cs,ws,next,nullptr,&imported) || !cs.empty() || !ws.empty() || imported.empty()) {
        if(error)*error="不是有效的子电路元件文件（请导入 .edasub 文件）";return false;
    }
    if(moduleDefs.size()+imported.size()>256){if(error)*error="子电路元件库数量超过限制";return false;}
    for(const auto& d:imported)for(const auto& existing:moduleDefs)if(d->name==existing->name){if(error)*error="元件库已有同名子电路："+d->name;return false;}
    auto candidate=moduleDefs;for(const auto& d:imported)candidate.push_back(d);
    if(!FitsDocument(components,wires,candidate)){if(error)*error="导入后工程超过存储限制";return false;}
    PushUndo();for(const auto& d:imported)moduleDefs.push_back(d);NotifyChanged();return true;
}

bool DrawingCanvas::DeleteModuleDefinition(const wxString& name) {
    if (simRunning) return false;
    for (size_t i = 0; i < moduleDefs.size(); ++i) {
        if (moduleDefs[i]->name == name) {
            PushUndo();
            if (placeModule && placeModule->name == name) {
                placeModule.reset();
                placeType = GATE_AND;
                selectMode = true;
                wireMode = false;
                CancelWirePreview();
            }
            moduleDefs.erase(moduleDefs.begin() + i);
            NotifyChanged();
            return true;
        }
    }
    return false;
}

void DrawingCanvas::ShowSubcircuit(ModulePtr module) {
    if(!module)return;
    components=module->components;wires=module->wires;readOnly=true;
    sel.clear();selectedWire=-1;wireMode=false;wireFromComp=-1;
    simRunning=false;undoStack.clear();redoStack.clear();
    savedSnapshot=CircuitToJson(components,wires,customDefs,moduleDefs);dirty=false;InvalidateScene();
}

bool DrawingCanvas::FitsDocument(const wxVector<Component>& cs,const wxVector<Wire>& ws,const ModuleLibrary& library) const {
    const auto json=CircuitToJson(cs,ws,customDefs,library).ToStdString(wxConvUTF8);
    return cs.size()<=10000 && ws.size()<=50000 && json.size()<=9*1024*1024;
}
