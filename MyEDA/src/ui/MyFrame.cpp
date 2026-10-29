#include "ui/MyFrame.h"
#include <wx/stdpaths.h>
#include <wx/filename.h>
#include <wx/bmpbndl.h>
#include <wx/artprov.h>
#include <wx/settings.h>
#include <wx/tokenzr.h>
#include <wx/choicdlg.h>

MyFrame::MyFrame(const wxString& title)
    : wxFrame(nullptr, wxID_ANY, title, wxDefaultPosition, wxSize(1200, 800))
{
    CreateMenuBar();
    CreateToolBar();
    CreateStatusBar();
    CreateClientArea();

    SetStatusText("就绪");

    // ============ 绑定菜单事件 ============
    Bind(wxEVT_MENU, &MyFrame::OnPackModule, this, ID_MENU_PACK_MODULE);
    Bind(wxEVT_MENU, &MyFrame::OnManageModule, this, ID_MENU_MANAGE_MODULE);
    Bind(wxEVT_MENU, &MyFrame::OnImportModule, this, ID_MENU_IMPORT_MODULE);
    Bind(wxEVT_MENU, &MyFrame::OnExportModule, this, ID_MENU_EXPORT_MODULE);
    Bind(wxEVT_MENU, &MyFrame::OnInspectModule, this, ID_MENU_INSPECT_MODULE);
    Bind(wxEVT_MENU, &MyFrame::OnNew,            this, ID_MENU_NEW);
    Bind(wxEVT_MENU, &MyFrame::OnOpen,           this, ID_MENU_OPEN);
    Bind(wxEVT_MENU, &MyFrame::OnSave,           this, ID_MENU_SAVE);
    Bind(wxEVT_MENU, &MyFrame::OnSaveAs,         this, ID_MENU_SAVE_AS);
    Bind(wxEVT_MENU, &MyFrame::OnExportNetlist,  this, ID_MENU_EXPORT_NETLIST);
    Bind(wxEVT_MENU, &MyFrame::OnExportKicadSch,  this, ID_MENU_EXPORT_SCH);
    Bind(wxEVT_MENU, &MyFrame::OnImportNetlist,  this, ID_MENU_IMPORT_NETLIST);
    Bind(wxEVT_MENU, &MyFrame::OnUndo,           this, ID_MENU_UNDO);
    Bind(wxEVT_MENU, &MyFrame::OnRedo,           this, ID_MENU_REDO);
    Bind(wxEVT_MENU, &MyFrame::OnCut,            this, ID_MENU_CUT);
    Bind(wxEVT_MENU, &MyFrame::OnCopy,           this, ID_MENU_COPY);
    Bind(wxEVT_MENU, &MyFrame::OnPaste,          this, ID_MENU_PASTE);
    Bind(wxEVT_MENU, &MyFrame::OnDelete,         this, ID_MENU_DELETE);
    Bind(wxEVT_MENU, &MyFrame::OnSimulateStart,  this, ID_MENU_SIMULATE_START);
    Bind(wxEVT_MENU, &MyFrame::OnSimulateStop,   this, ID_MENU_SIMULATE_STOP);
    Bind(wxEVT_MENU, &MyFrame::OnCustomGate,     this, ID_MENU_CUSTOM_GATE);
    Bind(wxEVT_MENU, &MyFrame::OnManageCustom,   this, ID_MENU_MANAGE_CUSTOM);
    Bind(wxEVT_MENU, &MyFrame::OnAnnotate,       this, ID_MENU_ANNOTATE);
    Bind(wxEVT_MENU, &MyFrame::OnSelectAll,      this, ID_MENU_SELECT_ALL);
    Bind(wxEVT_CLOSE_WINDOW, &MyFrame::OnClose,  this);
    Bind(wxEVT_MENU, [this](wxCommandEvent&) { canvas->ResetView(); SetStatusText("视图已复位为 100%"); }, ID_MENU_RESET_VIEW);
    simTimer.Bind(wxEVT_TIMER, [this](wxTimerEvent&) { canvas->SimTick(); RefreshPropertyTable(); });

    Bind(wxEVT_MENU, &MyFrame::OnAbout,          this, ID_MENU_ABOUT);
    Bind(wxEVT_MENU, &MyFrame::OnExit,           this, wxID_EXIT);

    // ============ 绑定工具栏事件 ============
    Bind(wxEVT_TOOL, &MyFrame::OnToolSelect,   this, ID_TOOL_SELECT);
    Bind(wxEVT_TOOL, &MyFrame::OnToolWire,     this, ID_TOOL_WIRE);
    Bind(wxEVT_TOOL, &MyFrame::OnToolDelete,   this, ID_TOOL_DELETE);
    Bind(wxEVT_TOOL, &MyFrame::OnToolAnd,      this, ID_TOOL_AND);
    Bind(wxEVT_TOOL, &MyFrame::OnToolOr,       this, ID_TOOL_OR);
    Bind(wxEVT_TOOL, &MyFrame::OnToolNot,      this, ID_TOOL_NOT);
    Bind(wxEVT_TOOL, &MyFrame::OnToolSimulate, this, ID_TOOL_SIMULATE);
}

// ================================================================
// 创建菜单栏
// ================================================================
void MyFrame::CreateMenuBar()
{
    wxMenuBar* menuBar = new wxMenuBar();

    // ---------- 文件 ----------
    wxMenu* fileMenu = new wxMenu();
    fileMenu->Append(ID_MENU_NEW,       "新建(&N)\tCtrl+N",  "新建电路");
    fileMenu->Append(ID_MENU_OPEN,      "打开(&O)\tCtrl+O",  "打开电路文件");
    fileMenu->AppendSeparator();
    fileMenu->Append(ID_MENU_SAVE,      "保存(&S)\tCtrl+S",  "保存电路文件");
    fileMenu->Append(ID_MENU_SAVE_AS,   "另存为(&A)\tCtrl+Shift+S", "另存为");
    fileMenu->AppendSeparator();
    fileMenu->Append(ID_MENU_IMPORT_NETLIST, "导入网表(&I)...", "导入标准网表文件");
    fileMenu->Append(ID_MENU_EXPORT_NETLIST, "导出网表(&E)...", "导出标准网表文件（供 PCB 软件使用）");
    fileMenu->Append(ID_MENU_EXPORT_SCH, "导出 KiCad 原理图(&K)...", "导出 .kicad_sch，可在 KiCad 里打开并按 F8 更新 PCB");
    fileMenu->AppendSeparator();
    fileMenu->Append(wxID_EXIT,         "退出(&Q)\tCtrl+Q",  "退出程序");
    menuBar->Append(fileMenu, "文件(&F)");

    // ---------- 编辑 ----------
    wxMenu* editMenu = new wxMenu();
    editMenu->Append(ID_MENU_UNDO,   "撤销(&U)\tCtrl+Z",       "撤销上一步操作");
    editMenu->Append(ID_MENU_REDO,   "重做(&R)\tCtrl+Y",       "重做");
    editMenu->AppendSeparator();
    editMenu->Append(ID_MENU_CUT,    "剪切(&T)\tCtrl+X",       "剪切选中元件");
    editMenu->Append(ID_MENU_COPY,   "复制(&C)\tCtrl+C",       "复制选中元件");
    editMenu->Append(ID_MENU_PASTE,  "粘贴(&P)\tCtrl+V",       "粘贴元件");
    editMenu->Append(ID_MENU_DELETE, "删除(&D)\tDelete",       "删除选中元件");
    editMenu->AppendSeparator();
    editMenu->Append(ID_MENU_SELECT_ALL, "全选(&A)\tCtrl+A", "选中画布上所有元件");
    menuBar->Append(editMenu, "编辑(&E)");

    // ---------- 工程 ----------
    wxMenu* projectMenu = new wxMenu();
    projectMenu->Append(ID_MENU_NEW, "新建工程(&N)...", "新建工程");
    projectMenu->Append(ID_MENU_ANNOTATE, "原理图位号重排(&R)\tCtrl+R", "按画布坐标从上到下、从左到右重新连续编号所有元件(U1, U2...)");
    projectMenu->Append(ID_MENU_CUSTOM_GATE, "新建自定义元件(&C)...", "用真值表定义一个新元件");
    projectMenu->Append(ID_MENU_MANAGE_CUSTOM, "管理自定义元件(&M)...", "编辑/删除已定义的自定义元件");
    projectMenu->AppendSeparator();
    projectMenu->Append(ID_MENU_PACK_MODULE,"将选中电路封装为元件...","以开关和指示灯定义输入输出端口");
    projectMenu->Append(ID_MENU_MANAGE_MODULE,"管理子电路元件(&U)...","查看并删除已定义的子电路元件");
    projectMenu->Append(ID_MENU_IMPORT_MODULE,"导入子电路元件...","导入可复用的 .edasub 文件");
    projectMenu->Append(ID_MENU_EXPORT_MODULE,"导出子电路元件...","将元件库中的子电路保存为 .edasub 文件");
    projectMenu->Append(ID_MENU_INSPECT_MODULE,"查看选中子电路内部...","只读查看方块内部的实际电路");
    menuBar->Append(projectMenu, "工程(&P)");

    // ---------- 仿真 ----------
    wxMenu* simulateMenu = new wxMenu();
    simulateMenu->Append(ID_MENU_SIMULATE_START, "开始仿真(&S)\tF5", "开始信号传播仿真");
    simulateMenu->Append(ID_MENU_SIMULATE_STOP,  "停止仿真(&T)\tF6", "停止仿真");
    menuBar->Append(simulateMenu, "仿真(&S)");

    // ---------- 窗口 ----------
    wxMenu* windowMenu = new wxMenu();
    windowMenu->Append(ID_MENU_RESET_VIEW, "复位视图(&R)\tCtrl+0", "复位画布视图");
    menuBar->Append(windowMenu, "窗口(&W)");

    // ---------- 帮助 ----------
    wxMenu* helpMenu = new wxMenu();
    helpMenu->Append(ID_MENU_ABOUT, "关于(&A)...", "关于本软件");
    menuBar->Append(helpMenu, "帮助(&H)");

    SetMenuBar(menuBar);
}

// ================================================================
// 创建工具栏
// ================================================================
void MyFrame::CreateToolBar()
{
    wxToolBar* toolBar = wxFrame::CreateToolBar(wxTB_HORIZONTAL | wxTB_FLAT | wxTB_TEXT | wxTB_HORZ_LAYOUT);
    toolBar->SetToolBitmapSize(FromDIP(wxSize(28, 28)));
    toolBar->SetFont(wxSystemSettings::GetFont(wxSYS_DEFAULT_GUI_FONT));
    toolBar->SetMargins(FromDIP(8), FromDIP(6));
    toolBar->SetToolPacking(FromDIP(6));
    toolBar->SetBackgroundColour(wxColour(246,249,252));

    // 找到 res 目录（跟可执行文件同级）
    wxFileName exeFile(wxStandardPaths::Get().GetExecutablePath());
    wxString resDir = exeFile.GetPath() + "/res/";

    // SVG bundles stay crisp when Windows display scaling changes.
    // Existing PNGs remain compatible with builds lacking SVG support.
    auto loadIcon = [&](const wxString& name) -> wxBitmapBundle {
#ifdef wxHAS_SVG
        const wxString svgPath = resDir + name + ".svg";
        if (wxFileExists(svgPath)) {
            wxBitmapBundle icon = wxBitmapBundle::FromSVGFile(svgPath, wxSize(28, 28));
            if (icon.IsOk()) return icon;
        }
#endif
        wxBitmap bitmap(resDir + name + ".png", wxBITMAP_TYPE_PNG);
        if (bitmap.IsOk()) return wxBitmapBundle::FromBitmap(bitmap);
        return wxArtProvider::GetBitmapBundle(wxART_MISSING_IMAGE, wxART_TOOLBAR, wxSize(24, 24));
    };

    wxBitmapBundle bmpNew       = loadIcon("new");
    wxBitmapBundle bmpOpen      = loadIcon("open");
    wxBitmapBundle bmpSave      = loadIcon("save");
    wxBitmapBundle bmpSelect    = loadIcon("select");
    wxBitmapBundle bmpWire      = loadIcon("wire");
    wxBitmapBundle bmpDelete    = loadIcon("delete");
    wxBitmapBundle bmpAnd       = loadIcon("and");
    wxBitmapBundle bmpOr        = loadIcon("or");
    wxBitmapBundle bmpNot       = loadIcon("not");
    wxBitmapBundle bmpSimulate  = loadIcon("simulate");

    // 文件操作
    toolBar->AddTool(ID_MENU_NEW,    "新建", bmpNew,  "新建电路");
    toolBar->AddTool(ID_MENU_OPEN,   "打开", bmpOpen, "打开电路文件");
    toolBar->AddTool(ID_MENU_SAVE,   "保存", bmpSave, "保存电路文件");
    toolBar->AddSeparator();

    // 编辑工具
    toolBar->AddTool(ID_TOOL_SELECT, "选择", bmpSelect, "选择元件");
    toolBar->AddTool(ID_TOOL_WIRE,   "连线", bmpWire,   "绘制连线");
    toolBar->AddTool(ID_TOOL_DELETE, "删除", bmpDelete, "删除选中元件");
    toolBar->AddSeparator();

    // 元件放置
    toolBar->AddTool(ID_TOOL_AND, "与门", bmpAnd, "放置与门");
    toolBar->AddTool(ID_TOOL_OR,  "或门", bmpOr,  "放置或门");
    toolBar->AddTool(ID_TOOL_NOT, "非门", bmpNot, "放置非门");
    toolBar->AddSeparator();

    // 仿真
    toolBar->AddTool(ID_TOOL_SIMULATE, "仿真", bmpSimulate, "开始/停止仿真");

    toolBar->Realize();
}

// ================================================================
// 创建中间三栏:元件库树 + 绘图区 + 属性表
// ================================================================
void MyFrame::CreateClientArea()
{
    // ---------- 左:元件库树 ----------
    componentTree = new wxTreeCtrl(this, wxID_ANY, wxDefaultPosition, wxSize(200, -1),
                                   wxTR_HAS_BUTTONS | wxTR_LINES_AT_ROOT);
    wxTreeItemId root = componentTree->AddRoot("元件库");

    wxTreeItemId gates = componentTree->AppendItem(root, "基本门电路");
    // 给每个树节点挂上"这是什么门"的数据,点击树时就能查到(见 OnTreeSelect)
    componentTree->AppendItem(gates, "与门 AND", -1, -1, new GateItemData(GATE_AND));
    componentTree->AppendItem(gates, "或门 OR",  -1, -1, new GateItemData(GATE_OR));
    componentTree->AppendItem(gates, "非门 NOT", -1, -1, new GateItemData(GATE_NOT));
    componentTree->AppendItem(gates, "异或门 XOR", -1, -1, new GateItemData(GATE_XOR));
    componentTree->AppendItem(gates, "与非门 NAND", -1, -1, new GateItemData(GATE_NAND));
    componentTree->AppendItem(gates, "或非门 NOR", -1, -1, new GateItemData(GATE_NOR));

    wxTreeItemId io = componentTree->AppendItem(root, "输入/输出");
    componentTree->AppendItem(io, "开关（电路输入）", -1, -1, new GateItemData(SW_INPUT));
    componentTree->AppendItem(io, "指示灯（电路输出）", -1, -1, new GateItemData(SW_LED));
    componentTree->AppendItem(root,"D 触发器",-1,-1,new GateItemData(GATE_DFF));
    componentTree->AppendItem(root,"时钟源",-1,-1,new GateItemData(SW_CLOCK));

    // 自定义元件分支:一个"新建"入口,以后每定义一个就往这里挂一个节点
    customRootItem = componentTree->AppendItem(root, "真值表逻辑门");
    moduleRootItem = componentTree->AppendItem(root, "子电路元件");
    // 注意:这里先不填充分支 —— 自定义元件库在画布上,而画布是后面才创建的

    componentTree->Expand(root);
    componentTree->Expand(gates);
    componentTree->Expand(customRootItem);

    // 点击树节点 → 告诉画布"接下来要放这种元件"
    componentTree->Bind(wxEVT_TREE_SEL_CHANGED, &MyFrame::OnTreeSelect, this);

    // ---------- 中:绘图区(自定义控件) ----------
    canvas = new DrawingCanvas(this);

    // ---------- 右:属性表 ----------
    propertyList = new wxListCtrl(this, wxID_ANY, wxDefaultPosition, wxSize(230, -1),
                                  wxLC_REPORT | wxLC_SINGLE_SEL);
    propertyList->InsertColumn(0, "属性", wxLIST_FORMAT_LEFT, 90);
    propertyList->InsertColumn(1, "值", wxLIST_FORMAT_LEFT, 130);

    // 画布上选中/改动元件时,回调这里刷新属性表
    canvas->SetSelectionCallback([this]() { RefreshPropertyTable(); UpdateTitle(); });
    RebuildCustomTree();   // 画布已就绪,现在填充"自定义元件"分支
    RefreshPropertyTable();

    // ---------- 用 sizer 把三块拼起来 ----------
    // Add(控件, 比例, 样式):比例 0 = 保持自己的固定宽度,比例 1 = 分走所有剩余空间
    wxBoxSizer* sizer = new wxBoxSizer(wxHORIZONTAL);
    sizer->Add(componentTree, 0, wxEXPAND);
    sizer->Add(canvas,         1, wxEXPAND);
    sizer->Add(propertyList,   0, wxEXPAND);
    SetSizer(sizer);
}

// ================================================================
// 创建状态栏
// ================================================================
void MyFrame::CreateStatusBar()
{
    wxFrame::CreateStatusBar(2);
    int widths[] = { -1, 200 };
    SetStatusWidths(2, widths);
    SetStatusText("就绪", 0);
    SetStatusText("工具：无", 1);
}

// ================================================================
// 属性表:显示当前选中元件的属性(没选中就显示提示)
// ================================================================
void MyFrame::RefreshPropertyTable()
{
    propertyList->DeleteAllItems();          // 每次全部重建,行数不固定也不怕
    const Component* c = canvas->GetSelected();

    if (!c && canvas->GetSelectionCount() > 1) {
        // 多选:显示汇总(单个元件的细节只在单选时显示)
        wxVector<int> ids;
        canvas->GetSelectedIds(ids);
        wxString idList;
        for (size_t i = 0; i < ids.size() && i < 8; i++) {
            if (i) idList += ", ";
            idList += wxString::Format("U%d", ids[i]);
        }
        if (ids.size() > 8) idList += " ...";
        long r1 = propertyList->InsertItem(propertyList->GetItemCount(), "已选中");
        propertyList->SetItem(r1, 1, wxString::Format("%d 个元件", (int)ids.size()));
        long r2 = propertyList->InsertItem(propertyList->GetItemCount(), "编号");
        propertyList->SetItem(r2, 1, idList);
        long r3 = propertyList->InsertItem(propertyList->GetItemCount(), "提示");
        propertyList->SetItem(r3, 1, "可整体拖动 / 删除 / 复制");
        return;
    }

    if (canvas->HasSelectedWire()) {
        long row = propertyList->InsertItem(0, "连线");
        propertyList->SetItem(row, 1, "已选中：Delete 删除，Ctrl+Z 撤销");
        return;
    }
    if (!c) {
        long row = propertyList->InsertItem(0, "选中");
        propertyList->SetItem(row, 1, "(未选中元件)：点画布上的元件查看");
        return;
    }

    auto addRow = [this](const wxString& k, const wxString& v) {
        long r = propertyList->InsertItem(propertyList->GetItemCount(), k);
        propertyList->SetItem(r, 1, v);
    };

    addRow("名称", wxString::Format("U%d", c->id));
    addRow("类型", (c->type == GATE_CUSTOM)
                            ? wxString::Format("自定义(%s)", c->customName)
                            : GateTypeName(c->type));
    if(c->type==SW_INPUT){addRow("用途","电路输入源");addRow("引脚 0","右侧 OUT（输出信号）");}
    if(c->type==SW_LED){addRow("用途","电路输出显示");addRow("引脚 0","左侧 IN（接收信号）");}
    addRow("位置", wxString::Format("(%d, %d)", c->x, c->y));

    if(c->type==GATE_MODULE && c->module) {
        addRow("子电路",c->module->name);
        int gates=0;for(const auto& inner:c->module->components)if(inner.type!=SW_INPUT && inner.type!=SW_LED)++gates;
        addRow("内部元件",wxString::Format("%d 个逻辑元件",gates));
        const int inputs=(int)c->module->inputIds.size();
        for(int i=0;i<inputs;++i)addRow("输入 "+c->module->inputNames[i],canvas->IsSimRunning()?wxString::Format("%d",canvas->GetPinValue(c->id,i)):"未仿真");
        for(size_t i=0;i<c->module->outputIds.size();++i)addRow("输出 "+c->module->outputNames[i],canvas->IsSimRunning()?wxString::Format("%d",canvas->GetOutputPinValue(c->id,inputs+(int)i)):"未仿真");
        return;
    }

    if (c->type == GATE_CUSTOM) {
        // 真值表拆成四行显示,最直观:每种输入组合对应什么输出
        const char* combos[4] = { "真值 00", "真值 01", "真值 10", "真值 11" };
        for (int i = 0; i < 4; i++)
            addRow(combos[i], ((c->truth >> i) & 1) ? "输出 1" : "输出 0");
    }

    if (c->type == SW_INPUT)
        addRow("状态", c->state ? "开(1)" : "关(0)");
    else if (c->type == SW_LED)
        addRow("状态", canvas->IsSimRunning()
                                ? (canvas->GetPinValue(c->id, 0) ? "亮(1)" : "灭(0)")
                                : "未仿真");
    else
        addRow("输出", canvas->IsSimRunning()
                                ? wxString::Format("%d", canvas->GetOutputValue(c->id))
                                : "未仿真");
}

// ================================================================
// 自定义元件:弹对话框→用户填名字勾真值表
// →存进 customDefs →挂到树上(以后可反复放置)
// ================================================================
void MyFrame::DefineCustomGate()
{
    if (canvas->IsSimRunning()) { canvas->Hint("请先停止仿真再定义元件"); return; }
    // 预填上一个定义,方便"改改再做一个新的"
    wxString defName = wxString::Format("MYGATE%d", (int)canvas->CustomDefs().size() + 1);
    int defTruth = 0;
    if (canvas->CustomDefs().size() > 0) {
        defName = canvas->CustomDefs().back().name + "2";
        defTruth = canvas->CustomDefs().back().truth;
    }

    CustomGateDialog dlg(this, defName, defTruth);
    if (dlg.ShowModal() != wxID_OK) return;

    CustomDef def;
    def.name = dlg.GetGateName();
    def.name.Trim(true).Trim(false);        // 去掉首尾空格
    if (def.name.empty())
        def.name = wxString::Format("自定义%d", (int)canvas->CustomDefs().size() + 1);
    def.truth = dlg.GetTruth();
    if (!canvas->AddCustomDefinition(def.name, def.truth)) return;

    RebuildCustomTree();
    // 展开并选中刚建的那个节点(选中→触发 OnTreeSelect→进入放置模式)
    componentTree->Expand(customRootItem);
    wxTreeItemIdValue cookie;
    wxTreeItemId item = componentTree->GetFirstChild(customRootItem, cookie);
    while (item.IsOk()) {
        if (componentTree->GetItemText(item) == def.name) { componentTree->SelectItem(item); break; }
        item = componentTree->GetNextChild(customRootItem, cookie);
    }
    SetStatusText(wxString::Format("自定义元件「%s」已定义,点击画布放置", def.name), 1);
}

// ================================================================
// 重建树上的"自定义元件"分支
// 为什么要整个重建:每个节点上挂着"每个定义在列表里的下标",
// 删除/改名后下标会错位,与其逐个修正不如全部重建
// ================================================================
void MyFrame::RebuildCustomTree()
{
    // 防御:自定义元件库挂在画布上,建界面时树可能比画布先创建 ——
    // 提前访问空指针会直接崩溃(踩过的真 bug)
    if (!canvas || !customRootItem.IsOk()) return;
    componentTree->DeleteChildren(customRootItem);

    componentTree->AppendItem(customRootItem, "＋ 新建自定义元件...",
                              -1, -1, new GateItemData(GATE_AND, -1, true));
    for (size_t i = 0; i < canvas->CustomDefs().size(); i++) {
        componentTree->AppendItem(customRootItem, canvas->CustomDefs()[i].name, -1, -1,
                                  new GateItemData(GATE_CUSTOM, (int)i));
    }
    componentTree->Expand(customRootItem);
    componentTree->DeleteChildren(moduleRootItem);
    componentTree->AppendItem(moduleRootItem,"＋ 封装选中电路...",-1,-1,new GateItemData(GATE_MODULE,-1,true));
    for(size_t i=0;i<canvas->Modules().size();++i)
        componentTree->AppendItem(moduleRootItem,canvas->Modules()[i]->name,-1,-1,new GateItemData(GATE_MODULE,(int)i));
    componentTree->Expand(moduleRootItem);
}

// ================================================================
// 管理自定义元件:弹列表对话框,关闭后把用户做过的操作
// 逐条落到模型(定义列表)和画布(已放置的同名元件)上
// ================================================================
void MyFrame::ManageCustomGates()
{
    if (canvas->IsSimRunning()) { canvas->Hint("请先停止仿真再管理元件库"); return; }
    if (canvas->CustomDefs().empty()) {
        wxMessageBox("还没有自定义元件。可以用菜单 工程 → 新建自定义元件 先定义一个。",
                     "提示", wxOK | wxICON_INFORMATION, this);
        return;
    }

    CustomManagerDialog dlg(this, canvas->CustomDefs());
    dlg.ShowModal();

    const wxVector<CustomDefAction>& acts = dlg.GetActions();
    for (size_t i = 0; i < acts.size(); i++) {
        const CustomDefAction& a = acts[i];
        if (a.kind == CustomDefAction::ADD) canvas->AddCustomDefinition(a.newName, a.truth);
        else if (a.kind == CustomDefAction::EDIT) canvas->EditCustomDefinition(a.oldName, a.newName, a.truth);
        else canvas->DeleteCustomDefinition(a.oldName);
    }

    if (!acts.empty()) {
        RebuildCustomTree();
        RefreshPropertyTable();
    }
}

void MyFrame::OnManageCustom(wxCommandEvent&) { ManageCustomGates(); }

void MyFrame::ManageModules()
{
    if (canvas->IsSimRunning()) { canvas->Hint("请停止仿真后再管理子电路"); return; }
    if (canvas->Modules().empty()) {
        wxMessageBox("当前工程还没有子电路元件。\n可以先选中画布上的电路，点击菜单「工程 → 将选中电路封装为元件」，或者点击「导入子电路元件」。",
                     "提示", wxOK | wxICON_INFORMATION, this);
        return;
    }
    SubcircuitManagerDialog dlg(this, canvas->Modules());
    dlg.ShowModal();
    const wxVector<wxString>& deleted = dlg.GetDeletedModules();
    for (size_t i = 0; i < deleted.size(); ++i) {
        canvas->DeleteModuleDefinition(deleted[i]);
    }
    if (!deleted.empty()) {
        RebuildCustomTree();
        RefreshPropertyTable();
        UpdateTitle();
    }
}

void MyFrame::OnManageModule(wxCommandEvent&) { ManageModules(); }

void MyFrame::OnAnnotate(wxCommandEvent&)
{
    if (canvas->AnnotateComponents()) {
        RefreshPropertyTable();
        UpdateTitle();
    }
}

void MyFrame::OnSelectAll(wxCommandEvent&)
{
    canvas->SelectAll();
    SetStatusText(wxString::Format("已全选 %d 个元件", canvas->GetSelectionCount()), 1);
}

// ================================================================
// 标题栏:显示文件名和"有未保存修改"星号
// ================================================================
void MyFrame::UpdateTitle()
{
    wxString t = "工业软件创新训练 - 电路原理图编辑器";
    if (!canvas->GetFilePath().empty()) t += "  —  " + wxFileName(canvas->GetFilePath()).GetFullName();
    if (canvas->IsDirty()) t += "  *";
    SetTitle(t);
}

// ================================================================
// 有未保存修改时问一句:保存 / 不保存 / 取消
// 返回 true = 可以继续(已保存或用户放弃),false = 用户取消了
// ================================================================
bool MyFrame::ConfirmDiscardChanges()
{
    if (!canvas->IsDirty()) return true;

    wxMessageDialog dlg(this, "当前电路有未保存的修改。要先保存吗?",
                        "MyEDA", wxYES_NO | wxCANCEL | wxICON_QUESTION);
    dlg.SetYesNoLabels("保存(&S)", "不保存(&D)");
    int r = dlg.ShowModal();
    if (r == wxID_CANCEL) return false;
    if (r == wxID_NO) return true;

    wxCommandEvent dummy;
    OnSave(dummy);                     // 没存过会弹"另存为"
    return !canvas->IsDirty();         // 保存成功才能继续(在另存为里取消则原地不动)
}

// 关闭窗口:先问存盘,用户取消就不关了
void MyFrame::OnClose(wxCloseEvent& e)
{
    if (!ConfirmDiscardChanges()) { e.Veto(); return; }
    e.Skip();
}

// ================================================================
// 菜单事件处理
// ================================================================
void MyFrame::OnNew(wxCommandEvent&)
{
    if (!ConfirmDiscardChanges()) return;   // 有未保存修改先问一句
    canvas->NewDocument();
    RebuildCustomTree();
    SetStatusText("已新建(画布清空)");
}

void MyFrame::OnOpen(wxCommandEvent&)
{
    if (!ConfirmDiscardChanges()) return;   // 有未保存修改先问一句
    wxFileDialog dlg(this, "打开电路文件", wxGetCwd(), "",
                     "MyEDA 电路文件 (*.eda)|*.eda",
                     wxFD_OPEN | wxFD_FILE_MUST_EXIST);
    if (dlg.ShowModal() != wxID_OK) return;   // 用户点了取消
    OpenPath(dlg.GetPath());
}

void MyFrame::OnSave(wxCommandEvent&)
{
    // 标准做法:从没存过 → 转到另存为;存过 → 直接覆盖
    if (canvas->GetFilePath().empty()) {
        wxCommandEvent e;
        OnSaveAs(e);
        return;
    }
    if (canvas->SaveFile(canvas->GetFilePath())) {
        SetStatusText("已保存:" + canvas->GetFilePath());
        UpdateTitle();
    } else {
        SetStatusText("保存失败!");
    }
}

void MyFrame::OnSaveAs(wxCommandEvent&)
{
    wxFileDialog dlg(this, "另存电路文件", wxGetCwd(), "未命名.eda",
                     "MyEDA 电路文件 (*.eda)|*.eda",
                     wxFD_SAVE | wxFD_OVERWRITE_PROMPT);
    if (dlg.ShowModal() != wxID_OK) return;
    if (canvas->SaveFile(dlg.GetPath())) {
        SetStatusText("已保存:" + dlg.GetPath());
        UpdateTitle();
    } else {
        SetStatusText("保存失败!");
    }
}
void MyFrame::OnExportKicadSch(wxCommandEvent&)
{
    wxFileDialog dlg(this, "导出 KiCad 原理图", wxGetCwd(), "未命名.kicad_sch",
                     "KiCad 原理图 (*.kicad_sch)|*.kicad_sch",
                     wxFD_SAVE | wxFD_OVERWRITE_PROMPT);
    if (dlg.ShowModal() != wxID_OK) return;
    if (canvas->ExportKicadSch(dlg.GetPath()))
        SetStatusText("KiCad 原理图已导出:" + dlg.GetPath() + " (在 KiCad 里打开后按 F8)");
    else
        SetStatusText("导出失败!");
}

void MyFrame::OnExportNetlist(wxCommandEvent&)
{
    wxFileDialog dlg(this, "导出 KiCad 网表", wxGetCwd(), "未命名.net",
                     "KiCad 网表 (*.net)|*.net",
                     wxFD_SAVE | wxFD_OVERWRITE_PROMPT);
    if (dlg.ShowModal() != wxID_OK) return;
    if (canvas->ExportNetlist(dlg.GetPath()))
        SetStatusText("网表已导出:" + dlg.GetPath());
    else
        SetStatusText("导出失败!");
}
void MyFrame::OnImportNetlist(wxCommandEvent&)
{
    if (!ConfirmDiscardChanges()) return;
    // 预填默认名“未命名.net”:刚导出的网表就是这个名,直接点打开即可
    wxFileDialog dlg(this, "导入 KiCad 网表", wxGetCwd(), "未命名.net",
                     "KiCad 网表 (*.net)|*.net",
                     wxFD_OPEN | wxFD_FILE_MUST_EXIST);
    if (dlg.ShowModal() != wxID_OK) return;
    if (canvas->ImportNetlist(dlg.GetPath())) {
        RebuildCustomTree();     // 网表里可能有自定义元件(按名字重建的)
        RefreshPropertyTable();
        SetStatusText("网表已导入(元件按网格重新排布):" + dlg.GetPath());
    } else {
        SetStatusText("导入失败（文件格式或引脚无效，当前电路已保留）");
    }
}
void MyFrame::OnUndo(wxCommandEvent&)         { SetStatusText(canvas->Undo() ? "已撤销" : "没有可撤销的操作"); RebuildCustomTree(); UpdateTitle(); }
void MyFrame::OnRedo(wxCommandEvent&)         { SetStatusText(canvas->Redo() ? "已重做" : "没有可重做的操作"); RebuildCustomTree(); UpdateTitle(); }
void MyFrame::OnCut(wxCommandEvent&)          { canvas->CutSelected(); SetStatusText("已剪切"); }
void MyFrame::OnCopy(wxCommandEvent&)         { canvas->CopySelected(); SetStatusText("已复制"); }
void MyFrame::OnPaste(wxCommandEvent&)        { canvas->PasteClipboard(); SetStatusText("已粘贴"); }
void MyFrame::OnDelete(wxCommandEvent&)
{
    if (canvas->IsSimRunning()) { canvas->Hint("仿真运行中不能删除,请先停止仿真(F6)"); return; }
    if (canvas->GetSelectionCount() == 0) { SetStatusText("请先点选元件(选中后变蓝框),再删除", 1); return; }
    if (canvas->DeleteSelected()) {
        SetStatusText("已删除选中元件");
        UpdateTitle();
    } else {
        SetStatusText("没有可删除的元件", 1);
    }
}
void MyFrame::OnSimulateStart(wxCommandEvent&){ canvas->StartSim(); simTimer.Start(500); SetStatusText("仿真自动步进中"); }
void MyFrame::OnSimulateStop(wxCommandEvent&) { simTimer.Stop(); canvas->StopSim(); SetStatusText("仿真已停止"); }
void MyFrame::OnAbout(wxCommandEvent&)
{
    wxMessageBox("工业软件创新训练 I\n电路原理图编辑器\n\n版本 0.1",
                 "关于", wxOK | wxICON_INFORMATION, this);
}
void MyFrame::OnCustomGate(wxCommandEvent&){ DefineCustomGate(); }
void MyFrame::OnExit(wxCommandEvent&)         { Close(true); }

// 打开文件(菜单和启动参数共用)
void MyFrame::OpenPath(const wxString& path)
{
    if (canvas->LoadFile(path)) {
        RebuildCustomTree();     // 文件里的自定义元件库要显示到树上
        RefreshPropertyTable();
        UpdateTitle();
        SetStatusText("已打开:" + path);
    } else {
        SetStatusText("打开失败（格式或连线无效，当前电路已保留）:" + path);
    }
}

// ================================================================
// 元件库树:点击节点 → 画布切换到对应的放置工具
// ================================================================
void MyFrame::OnTreeSelect(wxTreeEvent& e)
{
    // 防御:重建树上分支时(先删后加)会触发选中变化事件,此时节点可能已失效。
    // 不判断就会访问已删除的节点 → 程序崩溃(踩过的坑!)
    if (!e.GetItem().IsOk()) return;
    if (!componentTree->GetItemData(e.GetItem())) {
        // 分支节点没有挂数据,提示一句即可(但仍要能展开)
        SetStatusText("请展开分支选择具体元件", 1);
        return;
    }
    GateItemData* data = (GateItemData*)componentTree->GetItemData(e.GetItem());
    if (!data) {
        // 没挂数据的节点 = 分支节点(例如"基本门电路"),它们只能展开不能放置
        SetStatusText("请展开分支选择具体元件", 1);
        return;
    }

    if(data->type==GATE_MODULE) {
        if(data->defineNew){wxCommandEvent command;OnPackModule(command);return;}
        if(data->customIdx>=0 && (size_t)data->customIdx<canvas->Modules().size()) {
            const auto module=canvas->Modules()[data->customIdx];canvas->SetModulePlace(module);
            SetStatusText("放置子电路："+module->name+"；点击空白处放置",1);
        }
        return;
    }

    // 点了"新建自定义元件...":弹对话框让用户定义
    if (data->defineNew) {
        DefineCustomGate();
        return;
    }

    // 点了已定义的自定义元件:进入放置模式
    if (data->type == GATE_CUSTOM && data->customIdx >= 0 &&
        data->customIdx < (int)canvas->CustomDefs().size()) {
        const CustomDef& def = canvas->CustomDefs()[data->customIdx];
        canvas->SetCustomPlace(def.name, def.truth);
        SetStatusText("放置自定义元件：" + def.name + ",点击画布放置", 1);
        return;
    }

    canvas->SetPlaceType(data->type);
    SetStatusText("放置:" + GateTypeName(data->type) + ",点击画布放置", 1);
}

// ================================================================
// 工具栏事件处理
// ================================================================
void MyFrame::OnToolSelect(wxCommandEvent&)   { canvas->SetSelectMode(); SetStatusText("选择模式：空白处拖动=框选，Shift+点=加选，Esc=取消选择", 1); }
void MyFrame::OnToolWire(wxCommandEvent&)     { canvas->SetWireMode(); SetStatusText("连线:先点起点引脚,再点终点引脚", 1); }
void MyFrame::OnToolDelete(wxCommandEvent&)   { wxCommandEvent e; OnDelete(e); }
void MyFrame::OnToolAnd(wxCommandEvent&)      { canvas->SetPlaceType(GATE_AND); SetStatusText("放置：与门,点击画布放置", 1); }
void MyFrame::OnToolOr(wxCommandEvent&)       { canvas->SetPlaceType(GATE_OR);  SetStatusText("放置：或门,点击画布放置", 1); }
void MyFrame::OnToolNot(wxCommandEvent&)      { canvas->SetPlaceType(GATE_NOT); SetStatusText("放置：非门,点击画布放置", 1); }
void MyFrame::OnToolSimulate(wxCommandEvent&) {
    if (canvas->IsSimRunning()) { simTimer.Stop();canvas->StopSim(); SetStatusText("仿真已停止"); }
    else { canvas->StartSim();simTimer.Start(500); SetStatusText("仿真自动步进中"); }
}

namespace {
wxVector<wxString> ParsePortNames(wxString text) {
    text.Replace(wxString::FromUTF8(u8"，"),",");
    wxVector<wxString> names;wxStringTokenizer tokens(text,",",wxTOKEN_RET_EMPTY_ALL);
    while(tokens.HasMoreTokens()){wxString name=tokens.GetNextToken();name.Trim(true).Trim(false);names.push_back(name);}
    return names;
}
wxString PortList(const wxVector<int>& ids) {
    wxString value;for(int id:ids){if(!value.empty())value+=", ";value+=wxString::Format("U%d",id);}return value;
}
}
void MyFrame::OnPackModule(wxCommandEvent&) {
    if(canvas->IsSimRunning()){canvas->Hint("请先停止仿真再封装");return;}
    wxVector<int> inputs,outputs;canvas->SelectedInterfaces(inputs,outputs);
    if(inputs.empty() || outputs.empty()) {
        wxMessageBox("先用选择工具框选完整电路，包含作为输入的开关和作为输出的指示灯，再进行封装。","封装子电路",wxOK|wxICON_INFORMATION,this);return;
    }
    wxDialog dialog(this,wxID_ANY,"将选中电路封装为元件",wxDefaultPosition,wxDefaultSize);
    auto top=new wxBoxSizer(wxVERTICAL);
    top->Add(new wxStaticText(&dialog,wxID_ANY,"开关定义输入，指示灯定义输出；端口按元件编号排序。\n原电路保留，封装会加入左侧子电路元件库。"),0,wxALL,12);
    auto fields=new wxFlexGridSizer(2,8,12);fields->AddGrowableCol(1,1);
    auto name=new wxTextCtrl(&dialog,wxID_ANY,wxString::Format("Subcircuit%d",(int)canvas->Modules().size()+1),wxDefaultPosition,wxSize(330,-1));
    fields->Add(new wxStaticText(&dialog,wxID_ANY,"元件名称"),0,wxALIGN_CENTER_VERTICAL);fields->Add(name,1,wxEXPAND);
    wxString inNames,outNames;
    for(size_t i=0;i<inputs.size();++i){if(i)inNames+=",";inNames+=wxString::Format("IN%d",(int)i+1);}
    for(size_t i=0;i<outputs.size();++i){if(i)outNames+=",";outNames+=wxString::Format("OUT%d",(int)i+1);}
    auto in=new wxTextCtrl(&dialog,wxID_ANY,inNames);
    auto out=new wxTextCtrl(&dialog,wxID_ANY,outNames);
    fields->Add(new wxStaticText(&dialog,wxID_ANY,"输入名称（逗号分隔）"),0,wxALIGN_CENTER_VERTICAL);fields->Add(in,1,wxEXPAND);
    fields->Add(new wxStaticText(&dialog,wxID_ANY,"输入开关编号"),0,wxALIGN_CENTER_VERTICAL);fields->Add(new wxStaticText(&dialog,wxID_ANY,PortList(inputs)),0,wxALIGN_CENTER_VERTICAL);
    fields->Add(new wxStaticText(&dialog,wxID_ANY,"输出名称（逗号分隔）"),0,wxALIGN_CENTER_VERTICAL);fields->Add(out,1,wxEXPAND);
    fields->Add(new wxStaticText(&dialog,wxID_ANY,"输出指示灯编号"),0,wxALIGN_CENTER_VERTICAL);fields->Add(new wxStaticText(&dialog,wxID_ANY,PortList(outputs)),0,wxALIGN_CENTER_VERTICAL);
    top->Add(fields,1,wxEXPAND|wxLEFT|wxRIGHT,12);
    top->Add(dialog.CreateStdDialogButtonSizer(wxOK|wxCANCEL),0,wxALL|wxALIGN_RIGHT,12);
    dialog.SetSizerAndFit(top);dialog.CentreOnParent();
    while(dialog.ShowModal()==wxID_OK) {
        wxString error;
        if(canvas->PackSelection(name->GetValue(),ParsePortNames(in->GetValue()),ParsePortNames(out->GetValue()),&error)) {
            auto module=canvas->Modules().back();RebuildCustomTree();canvas->SetModulePlace(module);
            SetStatusText("子电路「"+module->name+"」已封装，点击空白处放置；可导出 .edasub 复用",1);return;
        }
        wxMessageBox(error.empty()?"无法封装，请检查选区和端口名称":error,"封装未完成",wxOK|wxICON_INFORMATION,&dialog);
    }
}
void MyFrame::OnImportModule(wxCommandEvent&) {
    if(canvas->IsSimRunning()){canvas->Hint("请先停止仿真");return;}
    wxFileDialog dialog(this,"导入子电路元件",wxGetCwd(),"","MyEDA 子电路 (*.edasub)|*.edasub",wxFD_OPEN|wxFD_FILE_MUST_EXIST);
    if(dialog.ShowModal()!=wxID_OK)return;
    wxString error;
    if(!canvas->ImportModules(dialog.GetPath(),&error)){wxMessageBox(error,"导入失败",wxOK|wxICON_INFORMATION,this);return;}
    RebuildCustomTree();canvas->SetModulePlace(canvas->Modules().back());SetStatusText("子电路已导入，点击空白处放置",1);
}
void MyFrame::OnExportModule(wxCommandEvent&) {
    if(canvas->Modules().empty()){wxMessageBox("请先封装或导入一个子电路元件","导出子电路",wxOK|wxICON_INFORMATION,this);return;}
    wxArrayString choices;for(const auto& d:canvas->Modules())choices.Add(d->name);
    int index=0;
    if(choices.size()>1){wxSingleChoiceDialog choice(this,"选择要导出的子电路","导出子电路",choices);if(choice.ShowModal()!=wxID_OK)return;index=choice.GetSelection();}
    wxFileDialog dialog(this,"导出子电路元件",wxGetCwd(),"subcircuit.edasub","MyEDA 子电路 (*.edasub)|*.edasub",wxFD_SAVE|wxFD_OVERWRITE_PROMPT);
    if(dialog.ShowModal()!=wxID_OK)return;
    SetStatusText(canvas->ExportModule(dialog.GetPath(),index)?"子电路元件已导出："+dialog.GetPath():"导出失败，请检查文件位置");
}
void MyFrame::OnInspectModule(wxCommandEvent&) {
    const auto selected=canvas->GetSelected();
    if(!selected || selected->type!=GATE_MODULE || !selected->module){wxMessageBox("请先选择画布上的一个子电路方块","查看内部电路",wxOK|wxICON_INFORMATION,this);return;}
    wxDialog dialog(this,wxID_ANY,"内部电路："+selected->module->name,wxDefaultPosition,wxSize(1000,750),wxDEFAULT_DIALOG_STYLE|wxRESIZE_BORDER);
    auto layout=new wxBoxSizer(wxVERTICAL);
    layout->Add(new wxStaticText(&dialog,wxID_ANY,"内部结构只读；滚轮缩放、中键平移。修改原电路后可用新名称重新封装。"),0,wxALL,10);
    auto view=new DrawingCanvas(&dialog);view->ShowSubcircuit(selected->module);
    layout->Add(view,1,wxEXPAND|wxLEFT|wxRIGHT,10);
    layout->Add(dialog.CreateButtonSizer(wxCLOSE),0,wxALL|wxALIGN_RIGHT,10);
    dialog.Bind(wxEVT_BUTTON,[&dialog](wxCommandEvent&){dialog.EndModal(wxID_CLOSE);},wxID_CLOSE);
    dialog.SetSizer(layout);dialog.CentreOnParent();dialog.ShowModal();
}
