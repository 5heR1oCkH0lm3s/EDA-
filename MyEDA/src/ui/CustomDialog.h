#pragma once
#include <wx/wx.h>
#include <wx/listbox.h>
#include <wx/statline.h>
#include <wx/vector.h>

#include "core/Component.h"   // CustomDef 定义在数据层

// 管理对话框里每做一步操作就记一条"账",关闭后由主窗口统一落到模型和画布上。
// 这样做的好处:对话框只负责"问用户",不直接改数据 —— 职责清晰,也好测试。
struct CustomDefAction {
    enum Kind { ADD, EDIT, DEL };
    Kind kind;
    wxString oldName;   // EDIT / DEL 用:原来的名字
    wxString newName;   // ADD / EDIT 用:新名字
    int truth;          // ADD / EDIT 用:新的真值表
};

// ================================================================
// 自定义元件对话框:用户填一个名字,再勾 4 个复选框定义真值表。
// 真值表 = 这个元件"每种输入组合下输出什么"的完整描述,所以
// 2 输入元件只需要 4 个勾选框,就能定义任意 2 输入 1 输出的逻辑!
// 位序约定:第 i 位对应输入组合 (A,B),i = A*2 + B
//   勾选框0 = A=0,B=0   勾选框1 = A=0,B=1
//   勾选框2 = A=1,B=0   勾选框3 = A=1,B=1
// ================================================================
class CustomGateDialog : public wxDialog {
public:
    CustomGateDialog(wxWindow* parent, const wxString& defName = "MYGATE", int defTruth = 0)
        : wxDialog(parent, wxID_ANY, "自定义元件", wxDefaultPosition, wxSize(380, 340))
    {
        wxBoxSizer* top = new wxBoxSizer(wxVERTICAL);

        // ---- 名字 ----
        wxFlexGridSizer* nameRow = new wxFlexGridSizer(2, 8, 8);
        nameRow->Add(new wxStaticText(this, wxID_ANY, "元件名称:"), 0, wxALIGN_CENTER_VERTICAL);
        nameField = new wxTextCtrl(this, wxID_ANY, defName);
        nameRow->Add(nameField, 1, wxEXPAND);
        top->Add(nameRow, 0, wxALL | wxEXPAND, 12);

        // ---- 真值表 ----
        top->Add(new wxStaticText(this, wxID_ANY, "真值表(勾选 = 该输入组合下输出为 1):"),
                 0, wxLEFT | wxRIGHT, 12);
        top->Add(new wxStaticText(this, wxID_ANY, "输入引脚:上=A,下=B;输出引脚在右侧"),
                 0, wxLEFT | wxRIGHT | wxTOP, 12);

        wxFlexGridSizer* grid = new wxFlexGridSizer(4, 12, 6);
        const char* labels[4] = { "A=0, B=0", "A=0, B=1", "A=1, B=0", "A=1, B=1" };
        for (int i = 0; i < 4; i++) {
            grid->Add(new wxStaticText(this, wxID_ANY, labels[i]), 0, wxALIGN_CENTER_VERTICAL);
            outs[i] = new wxCheckBox(this, wxID_ANY, "输出 1");
            outs[i]->SetValue((defTruth >> i) & 1);
            grid->Add(outs[i], 0);
        }
        top->Add(grid, 0, wxALL, 12);

        // 按钮行:不用 CreateStdDialogButtonSizer(它的文字跟随系统语言,
        // 这里固定成中文,演示时更统一)
        wxBoxSizer* btnRow = new wxBoxSizer(wxHORIZONTAL);
        wxButton* okBtn = new wxButton(this, wxID_OK, "确定");
        okBtn->SetDefault();                       // 回车 = 确定
        btnRow->Add(okBtn, 0, wxRIGHT, 8);
        btnRow->Add(new wxButton(this, wxID_CANCEL, "取消"), 0);
        top->Add(btnRow, 0, wxALL | wxALIGN_RIGHT, 12);

        SetSizer(top);
        Center();
    }

    wxString GetGateName() const { return nameField->GetValue(); }

    // 把 4 个勾选框打包成一个整数(位 i = 输入组合 i 的输出)
    int GetTruth() const {
        int t = 0;
        for (int i = 0; i < 4; i++)
            if (outs[i]->IsChecked()) t |= (1 << i);
        return t;
    }

private:
    wxTextCtrl* nameField;
    wxCheckBox* outs[4];
};

// ================================================================
// 管理自定义元件:列出所有已定义的自定义元件,可以新建 / 编辑 / 删除。
// 删除和改名会影响画布上已放置的元件,规则写在对话框里的提示上。
// ================================================================
class CustomManagerDialog : public wxDialog {
public:
    CustomManagerDialog(wxWindow* parent, const wxVector<CustomDef>& defs)
        : wxDialog(parent, wxID_ANY, "管理自定义元件", wxDefaultPosition, wxSize(560, 400)),
          defs_(defs)
    {
        wxBoxSizer* top = new wxBoxSizer(wxVERTICAL);
        top->Add(new wxStaticText(this, wxID_ANY, "已定义的自定义元件(每个元件 = 名字 + 一张真值表):"),
                 0, wxALL, 10);

        wxBoxSizer* row = new wxBoxSizer(wxHORIZONTAL);
        list = new wxListBox(this, wxID_ANY, wxDefaultPosition, wxSize(340, 220));
        row->Add(list, 1, wxEXPAND | wxRIGHT, 10);

        wxBoxSizer* btns = new wxBoxSizer(wxVERTICAL);
        wxButton* bNew = new wxButton(this, wxID_ANY, "新建(&N)...");
        wxButton* bEdit = new wxButton(this, wxID_ANY, "编辑(&E)...");
        wxButton* bDel = new wxButton(this, wxID_ANY, "删除(&D)");
        btns->Add(bNew, 0, wxBOTTOM | wxEXPAND, 8);
        btns->Add(bEdit, 0, wxBOTTOM | wxEXPAND, 8);
        btns->Add(bDel, 0, wxEXPAND, 8);
        row->Add(btns, 0, wxEXPAND);
        top->Add(row, 1, wxEXPAND | wxLEFT | wxRIGHT, 10);

        top->Add(new wxStaticText(this, wxID_ANY,
                 "提示:改名或改真值表会同步更新画布上已放置的同名元件;\n"
                 "删除定义只影响以后放置,画布上已有的元件会保留。"),
                 0, wxALL, 10);

        wxButton* closeBtn = new wxButton(this, wxID_CANCEL, "关闭");
        top->Add(closeBtn, 0, wxALL | wxALIGN_RIGHT, 10);
        SetSizer(top);
        Center();

        bNew->Bind(wxEVT_BUTTON, &CustomManagerDialog::OnNew, this);
        bEdit->Bind(wxEVT_BUTTON, &CustomManagerDialog::OnEdit, this);
        bDel->Bind(wxEVT_BUTTON, &CustomManagerDialog::OnDelete, this);
        RefreshList();
    }

    const wxVector<CustomDefAction>& GetActions() const { return actions; }

private:
    void RefreshList()
    {
        int sel = list->GetSelection();
        list->Clear();
        for (size_t i = 0; i < defs_.size(); i++) {
            wxString t = wxString::Format("%s   [真值表 %d%d%d%d  (00,01,10,11)]",
                defs_[i].name,
                (defs_[i].truth >> 0) & 1, (defs_[i].truth >> 1) & 1,
                (defs_[i].truth >> 2) & 1, (defs_[i].truth >> 3) & 1);
            list->Append(t);
        }
        if (sel != wxNOT_FOUND && sel < (int)defs_.size()) list->SetSelection(sel);
    }

    void OnNew(wxCommandEvent&)
    {
        wxString defName = wxString::Format("MYGATE%d", (int)defs_.size() + 1);
        CustomGateDialog dlg(this, defName, 0);
        if (dlg.ShowModal() != wxID_OK) return;
        wxString nm = dlg.GetGateName();
        nm.Trim(true).Trim(false);
        if (nm.empty()) nm = defName;
        for (const auto& d : defs_) if (d.name==nm) {
            wxMessageBox("名称已存在，请使用不同的元件名称", "提示", wxOK | wxICON_INFORMATION, this); return;
        }
        CustomDef d; d.name = nm; d.truth = dlg.GetTruth();
        defs_.push_back(d);
        CustomDefAction a; a.kind = CustomDefAction::ADD; a.oldName = ""; a.newName = nm; a.truth = d.truth;
        actions.push_back(a);
        RefreshList();
    }

    void OnEdit(wxCommandEvent&)
    {
        int sel = list->GetSelection();
        if (sel == wxNOT_FOUND || sel >= (int)defs_.size()) { wxMessageBox("请先在列表里选一个元件", "提示", wxOK | wxICON_INFORMATION, this); return; }
        CustomDef& d = defs_[sel];
        CustomGateDialog dlg(this, d.name, d.truth);
        if (dlg.ShowModal() != wxID_OK) return;
        wxString nm = dlg.GetGateName();
        nm.Trim(true).Trim(false);
        if (nm.empty()) nm = d.name;
        for (size_t i=0;i<defs_.size();++i) if ((int)i!=sel && defs_[i].name==nm) {
            wxMessageBox("名称已存在，请使用不同的元件名称", "提示", wxOK | wxICON_INFORMATION, this); return;
        }
        CustomDefAction a;
        a.kind = CustomDefAction::EDIT; a.oldName = d.name; a.newName = nm; a.truth = dlg.GetTruth();
        actions.push_back(a);
        d.name = nm; d.truth = a.truth;
        RefreshList();
    }

    void OnDelete(wxCommandEvent&)
    {
        int sel = list->GetSelection();
        if (sel == wxNOT_FOUND || sel >= (int)defs_.size()) { wxMessageBox("请先在列表里选一个元件", "提示", wxOK | wxICON_INFORMATION, this); return; }
        if (wxMessageBox("确定删除自定义元件「" + defs_[sel].name + "」?\n(画布上已放置的元件会保留)",
                         "删除确认", wxYES_NO | wxICON_QUESTION, this) != wxYES) return;
        CustomDefAction a;
        a.kind = CustomDefAction::DEL; a.oldName = defs_[sel].name; a.newName = ""; a.truth = 0;
        actions.push_back(a);
        defs_.erase(defs_.begin() + sel);
        RefreshList();
    }

    wxVector<CustomDef> defs_;             // 对话框内的工作副本
    wxVector<CustomDefAction> actions;     // 用户做过的操作(关掉后由主窗口执行)
    wxListBox* list;
};

// ================================================================
// 管理子电路元件:列出所有已导入或封装的子电路,支持查看与删除定义。
// 同样遵循:对话框内记账,关闭后统一由主窗口执行生效。
// ================================================================
class SubcircuitManagerDialog : public wxDialog {
public:
    SubcircuitManagerDialog(wxWindow* parent, const ModuleLibrary& defs)
        : wxDialog(parent, wxID_ANY, "管理子电路元件", wxDefaultPosition, wxSize(560, 400)),
          defs_(defs)
    {
        wxBoxSizer* top = new wxBoxSizer(wxVERTICAL);
        top->Add(new wxStaticText(this, wxID_ANY, "工程中已定义的子电路元件列表:"),
                 0, wxALL, 10);

        wxBoxSizer* row = new wxBoxSizer(wxHORIZONTAL);
        list = new wxListBox(this, wxID_ANY, wxDefaultPosition, wxSize(360, 220));
        row->Add(list, 1, wxEXPAND | wxRIGHT, 10);

        wxBoxSizer* btns = new wxBoxSizer(wxVERTICAL);
        wxButton* bDel = new wxButton(this, wxID_ANY, "删除(&D)");
        btns->Add(bDel, 0, wxEXPAND | wxBOTTOM, 8);
        row->Add(btns, 0, wxEXPAND);
        top->Add(row, 1, wxEXPAND | wxLEFT | wxRIGHT, 10);

        top->Add(new wxStaticText(this, wxID_ANY,
                 "提示: 删除定义只从元件库列表中移除, 不再提供放置;\n"
                 "画布上已放置的子电路元件会保留其内部电路副本不受影响。"),
                 0, wxALL, 10);

        wxButton* closeBtn = new wxButton(this, wxID_CANCEL, "关闭");
        top->Add(closeBtn, 0, wxALL | wxALIGN_RIGHT, 10);
        SetSizer(top);
        Center();

        bDel->Bind(wxEVT_BUTTON, &SubcircuitManagerDialog::OnDelete, this);
        RefreshList();
    }

    const wxVector<wxString>& GetDeletedModules() const { return deleted_; }

private:
    void RefreshList()
    {
        int sel = list->GetSelection();
        list->Clear();
        for (size_t i = 0; i < defs_.size(); i++) {
            const auto& m = defs_[i];
            int gates = 0;
            for (const auto& in : m->components)
                if (in.type != SW_INPUT && in.type != SW_LED) ++gates;
            wxString t = wxString::Format("%s   [%d 输入, %d 输出, %d 逻辑元件]",
                m->name,
                (int)m->inputIds.size(),
                (int)m->outputIds.size(),
                gates);
            list->Append(t);
        }
        if (sel != wxNOT_FOUND && sel < (int)defs_.size()) list->SetSelection(sel);
    }

    void OnDelete(wxCommandEvent&)
    {
        int sel = list->GetSelection();
        if (sel == wxNOT_FOUND || sel >= (int)defs_.size()) {
            wxMessageBox("请先在列表里选择一个子电路元件", "提示", wxOK | wxICON_INFORMATION, this);
            return;
        }
        wxString name = defs_[sel]->name;
        if (wxMessageBox("确定删除子电路元件「" + name + "」?\n(画布上已放置的元件会保留)",
                         "删除确认", wxYES_NO | wxICON_QUESTION, this) != wxYES)
            return;
        deleted_.push_back(name);
        defs_.erase(defs_.begin() + sel);
        RefreshList();
    }

    ModuleLibrary defs_;
    wxVector<wxString> deleted_;
    wxListBox* list;
};
