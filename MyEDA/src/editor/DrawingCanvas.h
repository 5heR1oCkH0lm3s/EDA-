#pragma once
#include <wx/wx.h>
#include <wx/vector.h>
#include <functional>
#include <map>
#include <set>
#include "core/Component.h"
#include "file/CircuitFile.h"
#include "file/NetlistExport.h"
#include "file/KicadSchExport.h"
#include "sim/Simulation.h"

// 绘图区:显示网格和所有已放置的元件。
// 交互约定:
//   放置模式(默认):点空白=放一个元件;点元件=选中(可拖动);
//                   Shift/Ctrl+点=加选/取消;拖动=移动所有选中的元件
//   选择模式(工具栏"选择"):空白处拖动=框选一片;点空白=清空选择
//   连线模式:点两个引脚完成一根连线
//   仿真模式:点开关切换 0/1 并实时重算
class DrawingCanvas : public wxPanel {
public:
    DrawingCanvas(wxWindow* parent);

    void SetPlaceType(GateType t) { CancelWirePreview(); placeType = t; wireMode = false; selectMode = false; }
    void SetCustomPlace(const wxString& name, int truth) {   // 放置用户自定义元件
        CancelWirePreview();
        placeType = GATE_CUSTOM; placeCustomName = name; placeCustomTruth = truth;
        wireMode = false; selectMode = false;
    }
    void SetWireMode()  { CancelWirePreview(); wireMode = true; selectMode = false; }
    void SetSelectMode();                              // 进入"框选"模式
    void ResetView();
    double GetViewZoom() const { return viewZoom; }
    bool IsSelectMode() const { return selectMode; }
    void SelectAll();                                  // 全选(编辑菜单 Ctrl+A)
    bool DeleteSelected();                             // 删除所有选中元件;返回是否真的删了
    bool AnnotateComponents();                        // 原理图位号重排(按几何位置重新编号 U1, U2...)

    // 保存/打开:真正干活的是 CircuitFile.h 里的函数,这里只负责"拿数据"和"换数据"
    bool SaveFile(const wxString& path);
    bool LoadFile(const wxString& path);
    void NewDocument();                                // 新建:清空一切,回到初始状态
    const wxString& GetFilePath() const { return filePath; }

    // "有未保存的修改"标记:关闭/新建/打开前用它提醒用户存盘
    bool IsDirty() const { return dirty; }

    // 导出/导入 KiCad 网表(任务5)
    bool ExportNetlist(const wxString& path) { return SaveNetlist(path, components, wires); }
    bool ImportNetlist(const wxString& path);

    // 导出 KiCad 原理图(.kicad_sch):在 KiCad 里打开后按 F8 即可"从原理图更新 PCB"
    bool ExportKicadSch(const wxString& path) {
        const auto text=ExportKicadSchematic(components,wires);
        if(text.empty())return false;
        wxFile f;
        if (!f.Open(path, wxFile::write)) return false;
        const bool written=f.Write(text,wxConvUTF8);
        return f.Close() && written;
    }

    // 逻辑仿真(任务6)
    void StartSim() { CancelWirePreview(); simRunning = true; simTick=0;prevClk.clear(); RunSim(); for(const auto& c:components)if(c.type==GATE_DFF)prevClk[c.id]=GetPinValue(c.id,1); NotifyChanged(); }
    void StopSim()  { simRunning = false; NotifyChanged(); }
    void SimTick();
    bool IsSimRunning() const { return simRunning; }

    // 编辑操作(撤销/重做/复制/剪切/粘贴)
    bool Undo();
    bool Redo();
    bool CanUndo() const { return undoStack.size() > 0; }
    bool CanRedo() const { return redoStack.size() > 0; }
    void CopySelected();
    void CutSelected();
    void PasteClipboard();
    bool HasClipboard() const { return clipComps.size() > 0; }

    // 给右侧属性表用的查询接口(属性表由 MyFrame 更新,画布只提供数据)
    int  GetSelectionCount() const { return (int)sel.size() + (selectedWire >= 0 ? 1 : 0); }
    const Component* GetSelected() const;       // 只选中一个时返回它,否则 nullptr
    void GetSelectedIds(wxVector<int>& out) const;
    int GetOutputPinValue(int compId, int pin) const;
    int  GetOutputValue(int compId) const;      // 某元件输出引脚的仿真值
    int  GetPinValue(int compId, int pin) const;// 某输入引脚连到的来源值
    void SetSelectionCallback(std::function<void()> cb) { onSelection = cb; }
    // 提示回调:画布遇到"删不掉/点不中"这类情况时,把原因送到状态栏,不再静默
    void SetHintCallback(std::function<void(const wxString&)> cb) { onHint = cb; }
    void Hint(const wxString& msg) { if (onHint) onHint(msg); }

    // 自定义元件库(属于文档数据:跟着电路一起存盘)
    const wxVector<CustomDef>& CustomDefs() const { return customDefs; }
    bool AddCustomDefinition(const wxString& name, int truth);
    bool EditCustomDefinition(const wxString& oldName, const wxString& name, int truth);
    bool DeleteCustomDefinition(const wxString& name);
    const ModuleLibrary& Modules() const { return moduleDefs; }
    void SetModulePlace(ModulePtr module);
    void ShowSubcircuit(ModulePtr module);
    bool PackSelection(const wxString& name, const wxVector<wxString>& inputs, const wxVector<wxString>& outputs, wxString* error=nullptr);
    void SelectedInterfaces(wxVector<int>& inputs, wxVector<int>& outputs) const;
    bool ImportModules(const wxString& path, wxString* error=nullptr);
    bool ExportModule(const wxString& path, size_t index) const;
    bool DeleteModuleDefinition(const wxString& name);
    bool HasSelectedWire() const { return selectedWire >= 0; }

    // 自定义元件的管理(配合主窗口的"管理自定义元件"对话框)
    int  CountCustomInstances(const wxString& name) const;
    void UpdateCustomInstances(const wxString& oldName, const wxString& newName, int truth);

private:
    bool FitsDocument(const wxVector<Component>& cs,const wxVector<Wire>& ws,const ModuleLibrary& library) const;
    void OnPaint(wxPaintEvent&);
    void OnSize(wxSizeEvent&);
    void OnWheel(wxMouseEvent&);
    void OnMiddleDown(wxMouseEvent&);
    void OnMiddleUp(wxMouseEvent&);
    wxPoint ScreenToWorld(const wxPoint& p) const;
    wxPoint WorldToScreen(const wxPoint& p) const;
    void DrawScene(wxDC& dc);             // Cache the grid, committed wires and components.
    void CancelWirePreview();
    void RefreshWirePreview(const wxPoint& endpoint);
    void InvalidateScene();
    void OnLeftDown(wxMouseEvent&);
    void OnMotion(wxMouseEvent&);
    void OnLeftUp(wxMouseEvent&);
    void OnKeyDown(wxKeyEvent&);

    int HitWire(const wxPoint& point) const;
    int  HitTest(int mx, int my);          // 返回点中的元件编号,没点中返回 -1
    bool HitPin(int mx, int my, int* compId, int* pin);   // 连线模式:命中的引脚
    Component* FindById(int id);           // 按编号找元件,找不到返回 nullptr
    void DrawGate(wxDC& dc, const Component& c);
    void RunSim();                         // 跑一遍仿真并重画
    void NotifyChanged();                  // 重画 + 通知属性表刷新
    void PushUndo();                       // 改动前存档(快照式撤销)
    void CommitSnapshot(const wxString& snap);   // 外部算好的快照入栈(拖动用)
    void RestoreSnapshot(const wxString& snap);
    bool InRect(const Component& c, int x1, int y1, int x2, int y2) const;  // 框选:元件是否落在框里

    bool readOnly = false;
    double viewZoom = 1.0;
    wxPoint viewPan{0,0};
    bool panning = false;
    wxPoint panStart, panOrigin;
    wxBitmap sceneBitmap;
    bool sceneDirty = true;
    wxSize sceneSize;
    double sceneScale = 0.0;

    wxVector<Component> components;   // 画布上所有元件(核心数据!)
    ModuleLibrary moduleDefs;
    ModulePtr placeModule;
    wxVector<CustomDef> customDefs;   // 用户自定义元件的库(名字+真值表)
    wxVector<Wire> wires;             // 所有连线
    GateType placeType = GATE_AND;    // 当前放置工具
    wxString placeCustomName;         // 当前要放置的自定义元件名
    int placeCustomTruth = 0;         // 当前要放置的自定义元件真值表
    int nextId = 1;                   // 下一个元件的编号

    // 选择:用集合支持多选(单选就是"集合里只有一个元素")
    std::set<int> sel;
    int selectedWire = -1;
    bool selectMode = false;          // "选择"工具:空白拖动 = 框选
    bool rubberBand = false;          // 正在拉框?
    wxPoint rbStart, rbEnd;           // 框的两个角

    // 拖动:一次拖动所有选中的元件
    struct DragItem { int id; int offX; int offY; };   // 每个元件相对鼠标的偏移
    wxVector<DragItem> dragItems;
    bool dragging = false;            // 正在拖动?
    wxString dragStartSnapshot;       // 拖动开始前的快照(松手时若真的移动了才入栈)

    // 仿真状态
    bool simRunning = false;          // 仿真是否运行中
    int simTick=0;
    std::map<int,int> prevClk;
    SimulationResult simOut;         // 每个元件的输出值(仿真结果)

    // 连线模式状态:点第一个引脚记下起点,再点第二个引脚完成连线
    bool wireMode = false;
    int  wireFromComp = -1, wireFromPin = -1;   // 已选的起点引脚(-1=还没选)
    wxVector<wxPoint> wireBends;
    wxPoint wireEnd{ 0, 0 };                    // 橡皮筋终点(鼠标当前位置)

    // 撤销/重做:存的是电路 JSON 文本快照(JSON 往返已验证无损,所以快照=完整状态)
    wxVector<wxString> undoStack;
    wxVector<wxString> redoStack;

    // 复制/粘贴(支持多个)
    wxVector<Component> clipComps;
    wxVector<Wire> clipWires;
    wxString savedSnapshot;

    wxString filePath;                          // 当前电路对应的文件(空=还没保存过)
    bool dirty = false;                         // 有未保存的修改?
    std::function<void()> onSelection;          // 选中/内容变化时通知外面刷新属性表
    std::function<void(const wxString&)> onHint; // 操作没生效时,把原因送出去显示
};
