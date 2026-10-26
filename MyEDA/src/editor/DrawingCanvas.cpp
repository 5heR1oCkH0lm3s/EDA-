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
    undoStack.push_back(CircuitToJson(components, wires));
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
    JsonToCircuit(std::string((const char*)snap.utf8_str()), components, wires, next);
    nextId = next;
    sel.clear();
    wireMode = false;
    wireFromComp = -1;
    NotifyChanged();
}

bool DrawingCanvas::Undo()
{
    if (undoStack.size() == 0) return false;
    redoStack.push_back(CircuitToJson(components, wires));   // 当前状态留给重做
    wxString snap = undoStack.back();
    undoStack.pop_back();
    RestoreSnapshot(snap);
    dirty = true;
    return true;
}

bool DrawingCanvas::Redo()
{
    if (redoStack.size() == 0) return false;
    undoStack.push_back(CircuitToJson(components, wires));
    wxString snap = redoStack.back();
    redoStack.pop_back();
    RestoreSnapshot(snap);
    dirty = true;
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
    sel.clear();
    NotifyChanged();
}

void DrawingCanvas::SelectAll()
{
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
    for (size_t i = 0; i < components.size(); i++)
        if (sel.count(components[i].id))
            clipComps.push_back(components[i]);
}

void DrawingCanvas::CutSelected()
{
    CopySelected();
    DeleteSelected();
}

void DrawingCanvas::PasteClipboard()
{
    if (clipComps.size() == 0) return;
    PushUndo();
    sel.clear();
    for (size_t i = 0; i < clipComps.size(); i++) {
        Component c = clipComps[i];
        c.id = nextId++;
        c.x += 30;          // 往右下偏一点,不和原件重叠
        c.y += 30;
        components.push_back(c);
        sel.insert(c.id);   // 粘贴出来的直接选中,方便接着拖
    }
    dirty = true;
    NotifyChanged();
}

void DrawingCanvas::NotifyChanged()
{
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
            RefreshRect(wxRect(path[i-1], path[i]).Inflate(4), false);
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
        DrawScene(sceneDC);
        sceneDC.SelectObject(wxNullBitmap);
        sceneDirty = false;
    }
    dc.DrawBitmap(sceneBitmap, 0, 0, false);
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

    // Fine 10-pixel grid with a stronger line every 50 pixels.
    const wxSize size = GetClientSize();
    for (int x = 0; x < size.x; x += CircuitGrid) {
        dc.SetPen(wxPen(x % 50 == 0 ? wxColour(208,220,230) : wxColour(235,241,245)));
        dc.DrawLine(x, 0, x, size.y);
    }
    for (int y = 0; y < size.y; y += CircuitGrid) {
        dc.SetPen(wxPen(y % 50 == 0 ? wxColour(208,220,230) : wxColour(235,241,245)));
        dc.DrawLine(0, y, size.x, y);
    }

    // 先画连线(在元件下层)
    for (size_t i = 0; i < wires.size(); i++) {
        Component* a = FindById(wires[i].comp1);
        Component* b = FindById(wires[i].comp2);
        if (!a || !b) continue;
        // 仿真中:高电位的线画红色,低电位画黑色(Logisim 风格)
        if (simRunning && GetOutputValue(wires[i].comp1) == 1)
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
    SetFocus();   // 让画布获得键盘焦点,Delete 键才有效

    // ----- 仿真运行中:点到开关 = 切换它的开/关,然后重算整个电路 -----
    if (simRunning) {
        int id = HitTest(e.GetX(), e.GetY());
        Component* c = (id != -1) ? FindById(id) : nullptr;
        if (c && c->type == SW_INPUT) {
            c->state = !c->state;   // 切换!
            dirty = true;
            SimTick();               // 前进一个仿真步(不是 RunSim,保持时序状态)
        } else {
            Hint("仿真运行中:点开关切换;点时钟源翻转;停止仿真后才能编辑");
        }
        return;
    }

    // ----- 连线模式 -----
    if (wireMode) {
        int cid, pin;
        if (!HitPin(e.GetX(), e.GetY(), &cid, &pin)) {
            if (wireFromComp != -1) {
                const wxPoint point = SnapGrid(e.GetPosition());
                const wxPoint previous = wireBends.empty() ? GetPinPos(*FindById(wireFromComp), wireFromPin) : wireBends.back();
                if (point != previous) wireBends.push_back(point);
                wireEnd = point;
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
            PushUndo();              // Click target pin to commit the entire route.
            Wire w = { wireFromComp, wireFromPin, cid, pin };
            w.bends = wireBends;
            wires.push_back(w);
            dirty = true;
            CancelWirePreview();     // 可以继续连下一根
            NotifyChanged();
        }
        Refresh(false);
        return;
    }

    // ----- 普通/选择模式 -----
    int id = HitTest(e.GetX(), e.GetY());
    bool add = e.ShiftDown() || e.ControlDown();   // Shift/Ctrl = 加选/取消选择

    if (id != -1) {
        if (add) {
            // 加选:已经在集合里就移出(再点一次 = 取消选中)
            if (sel.count(id)) sel.erase(id);
            else               sel.insert(id);
            NotifyChanged();
            return;
        }
        // 选中它(如果它不在已选集合里,就改成只选它),然后开始拖动
        if (!sel.count(id)) {
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
            d.offX = e.GetX() - c->x;
            d.offY = e.GetY() - c->y;
            dragItems.push_back(d);
        }
        // 先不急着入撤销栈(可能只是点一下没拖);存个快照,松手时若移动了再入栈
        dragStartSnapshot = CircuitToJson(components, wires);
        CaptureMouse();   // 拖出画布外鼠标消息也归我们,松开才不会丢
        NotifyChanged();
        return;
    }

    // 点在空白处
    if (selectMode) {
        // 选择模式:开始拉框(Shift/Ctrl 则保留原来的选择,做"追加框选")
        if (!add) sel.clear();
        rubberBand = true;
        rbStart = wxPoint(e.GetX(), e.GetY());
        rbEnd = rbStart;
        CaptureMouse();
        NotifyChanged();
        return;
    }

    // 放置模式:放一个当前类型的元件
    PushUndo();
    Component c;
    c.type = placeType;
    const wxPoint placed = SnapGrid(e.GetPosition());
    c.x = placed.x;
    c.y = placed.y;
    c.id = nextId++;
    if (placeType == GATE_CUSTOM) {
        c.customName = placeCustomName;
        c.truth = placeCustomTruth;
    }
    components.push_back(c);
    sel.clear();
    dirty = true;
    NotifyChanged();
}

// 拖动中:所有选中的元件跟着鼠标走;框选中:框跟着鼠标走;连线模式:预览线跟着走
void DrawingCanvas::OnMotion(wxMouseEvent& e)
{
    if (simRunning) return;
    if (rubberBand) {
        rbEnd = wxPoint(e.GetX(), e.GetY());
        Refresh(false);
        return;
    }
    if (wireMode && wireFromComp != -1) {
        RefreshWirePreview(e.GetPosition());
        return;
    }
    if (!dragging)
        return;
    for (size_t i = 0; i < dragItems.size(); i++) {
        Component* c = FindById(dragItems[i].id);
        if (!c) continue;
        const wxPoint moved = SnapGrid(wxPoint(e.GetX() - dragItems[i].offX, e.GetY() - dragItems[i].offY));
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
        ReleaseMouse();
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
        ReleaseMouse();
        // 真的移动过才记一步撤销(纯点选不污染撤销链)
        if (dragStartSnapshot != CircuitToJson(components, wires)) {
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
    return !(c.x + r < x1 || c.x - r > x2 || c.y + r < y1 || c.y - r > y2);
}

// Delete 删除选中元件;Esc 取消选择
void DrawingCanvas::OnKeyDown(wxKeyEvent& e)
{
    if (e.GetKeyCode() == WXK_DELETE) {
        if (!DeleteSelected())
            Hint("没有可删除的元件:先点选元件(变蓝框),仿真中请先停止仿真");
    } else if (e.GetKeyCode() == WXK_ESCAPE) {
        CancelWirePreview();
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
    components.clear();
    wires.clear();
    nextId = 1;
    sel.clear();
    wireMode = false;
    wireFromComp = -1;
    simRunning = false;
    simOut.clear();
    undoStack.clear();
    redoStack.clear();
    filePath = "";
    dirty = false;
    NotifyChanged();
}

bool DrawingCanvas::SaveFile(const wxString& path)
{
    if (!SaveCircuit(path, components, wires, customDefs))
        return false;
    filePath = path;   // 记住存到哪了,下次 Ctrl+S 直接覆盖
    dirty = false;     // 存过盘了,没有未保存修改
    return true;
}

bool DrawingCanvas::LoadFile(const wxString& path)
{
    wxVector<CustomDef> loadedDefs;
    if (!LoadCircuit(path, components, wires, nextId, &loadedDefs))
        return false;
    // 文件里的自定义元件库并进当前库(同名则以文件里的为准)
    for (size_t i = 0; i < loadedDefs.size(); i++) {
        bool merged = false;
        for (size_t k = 0; k < customDefs.size(); k++)
            if (customDefs[k].name == loadedDefs[i].name) { customDefs[k].truth = loadedDefs[i].truth; merged = true; break; }
        if (!merged) customDefs.push_back(loadedDefs[i]);
    }
    sel.clear();           // 旧电路的选中状态全部作废
    wireMode = false;
    wireFromComp = -1;
    simRunning = false;
    simOut.clear();
    undoStack.clear();     // 换了一份图纸,旧的撤销历史没有意义
    redoStack.clear();
    filePath = path;
    dirty = false;         // 刚打开的图纸是干净的
    NotifyChanged();
    return true;
}

bool DrawingCanvas::ImportNetlist(const wxString& path)
{
    if (!LoadNetlist(path, components, wires))
        return false;
    int maxId = 0;
    for (size_t i = 0; i < components.size(); i++)
        if (components[i].id > maxId) maxId = components[i].id;
    nextId = maxId + 1;
    sel.clear();
    wireMode = false;
    wireFromComp = -1;
    simRunning = false;
    simOut.clear();
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
    if (simRunning) {
        Hint("仿真运行中不能删除元件,请先停止仿真(工具栏[仿真]或 F6)");
        return false;
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
    sel.clear();
    dirty = true;
    NotifyChanged();
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
        if (mx >= c.x - r && mx <= c.x + r && my >= c.y - r && my <= c.y + r)
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
            if (dx * dx + dy * dy <= 9 * 9) {
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
    } else if (c.type == GATE_DFF) {
        // D 触发器:方框 + "D" 标签
        dc.DrawRoundedRectangle(x - 22, y - 18, 44, 36, 4);
        dc.DrawText("D", x - 14, y - 9);
        dc.SetTextForeground(*wxBLUE);
        dc.DrawText("CLK", x - 8, y + 8);
        dc.SetTextForeground(*wxBLACK);
    } else if (c.type == SW_CLOCK) {
        // 时钟源:圆角方框 + "CLK" 标签 + 方波符号
        dc.SetBrush(wxBrush(wxColour(230, 240, 255)));
        dc.DrawRoundedRectangle(x - 20, y - 14, 40, 28, 4);
        dc.SetBrush(*wxWHITE_BRUSH);
        // 画方波符号
        dc.DrawLine(x - 12, y + 4, x - 12, y - 4);
        dc.DrawLine(x - 12, y - 4, x - 4, y - 4);
        dc.DrawLine(x - 4, y - 4, x - 4, y + 4);
        dc.DrawLine(x - 4, y + 4, x + 4, y + 4);
        dc.DrawLine(x + 4, y + 4, x + 4, y - 4);
        dc.DrawLine(x + 4, y - 4, x + 12, y - 4);
        dc.SetTextForeground(*wxBLACK);
    } else if (c.type == SW_INPUT) {
        // 开关:圆角方框 + "1"/"0";闭合时底色变绿
        dc.SetBrush(c.state ? wxBrush(wxColour(144, 238, 144)) : *wxWHITE_BRUSH);
        dc.DrawRoundedRectangle(x - 18, y - 14, 36, 28, 4);
        dc.DrawText(c.state ? "1" : "0", x - 5, y - 9);
    } else {
        // 指示灯:圆;仿真中亮=红填充,灭=白
        int v = simRunning ? GetPinValue(c.id, 0) : 0;
        dc.SetBrush((simRunning && v == 1) ? *wxRED_BRUSH : *wxWHITE_BRUSH);
        dc.DrawCircle(x, y, 14);
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

    // 编号标签,比如 U1
    dc.DrawText(wxString::Format("U%d", c.id), x - 12, y - 36);
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
void DrawingCanvas::SimTick()
{
    if (!simRunning) return;
    simTick++;
    // 时钟源翻转
    for (size_t i = 0; i < components.size(); i++) {
        if (components[i].type == SW_CLOCK)
            simOut[components[i].id] = (simTick / 2) % 2;
    }
    // 组合门传播
    for (size_t i = 0; i < components.size(); i++) {
        const Component& c = components[i];
        if (c.type == SW_INPUT || c.type == SW_LED || c.type == SW_CLOCK || c.type == GATE_DFF)
            continue;
        int v0 = GetPinValue(c.id, 0);
        int res;
        if (c.type == GATE_AND)       res = v0 & GetPinValue(c.id, 1);
        else if (c.type == GATE_OR)   res = v0 | GetPinValue(c.id, 1);
        else if (c.type == GATE_XOR)  res = v0 ^ GetPinValue(c.id, 1);
        else if (c.type == GATE_NAND) res = !(v0 & GetPinValue(c.id, 1));
        else if (c.type == GATE_NOR)  res = !(v0 | GetPinValue(c.id, 1));
        else /* GATE_NOT */           res = !v0;
        simOut[c.id] = res;
    }
    // D 触发器:CLK=1 时 Q=D
    for (size_t i = 0; i < components.size(); i++) {
        if (components[i].type != GATE_DFF) continue;
        int clkVal = GetPinValue(components[i].id, 1);
        if (clkVal) simOut[components[i].id] = GetPinValue(components[i].id, 0);
    }
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
    int src = FindSource(wires, compId, pin);
    if (src < 0) return 0;
    return GetOutputValue(src);
}
