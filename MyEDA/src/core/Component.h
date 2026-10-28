#pragma once
#include <wx/wx.h>
#include <memory>
#include "core/WireRouting.h"

// 元件类型(元件库注册表:新增一种元件,在 4 个"注册点"各加一行——
// ①这些枚举 ②GateTypeName/FromName 名字表 ③DrawGate 绘制 ④SimulateCircuit 仿真逻辑)
enum GateType {
    GATE_AND,
    GATE_OR,
    GATE_NOT,
    GATE_XOR,
    GATE_NAND,     // 与非 = 与门+非门
    GATE_NOR,      // 或非 = 或门+非门
    GATE_CUSTOM,   // 用户自定义元件:2 输入 1 输出,行为由真值表 truth 决定
    SW_INPUT,      // 开关:点击切换 0/1,输出引脚
    GATE_MODULE,   // Reusable hierarchical circuit with named input/output ports.
    SW_LED,        // 指示灯:亮=1 灭=0,输入引脚
};

struct SubcircuitDef;
using ModulePtr = std::shared_ptr<const SubcircuitDef>;

// 画布上的一个元件:类型 + 位置 + 编号 + 状态。
// 原则:数据是核心,界面只是把数据"画"出来
struct Component {
    GateType type = GATE_AND;
    int x = 0, y = 0;      // 元件中心在画布上的坐标
    int id = 0;            // 全局编号 1,2,3...(显示为 U1、U2...)
    bool state = false;    // 开关的开/关(其他元件不用)
    wxString customName;   // 自定义元件的名字(其他类型为空)
    ModulePtr module;
    int truth = 0;         // 自定义元件真值表:bit(a*2+b) = 该输入组合下的输出
                           //   例:NAND 的 truth = 0b0111 = 7 → (0,1,1,1)
};

// 用户自定义元件的"定义"(元件库条目):名字 + 真值表。
// 放在数据层,因为它是要跟着电路一起存盘的数据,不只是界面的事
struct CustomDef {
    wxString name;
    int truth = 0;
};

// 一根连线:起点(元件编号+引脚号) → 终点(元件编号+引脚号)
struct Wire {
    int comp1, pin1;
    int comp2, pin2;
    wxVector<wxPoint> bends; // User-defined grid waypoints, independent of electrical connectivity.
};

struct SubcircuitDef {
    wxString name;
    wxVector<Component> components;
    wxVector<Wire> wires;
    wxVector<int> inputIds, outputIds;
    wxVector<wxString> inputNames, outputNames;
};
using ModuleLibrary = wxVector<ModulePtr>;
inline int ModuleHeight(const Component& c) {
    return c.module ? std::max(80, ((int)std::max(c.module->inputIds.size(),c.module->outputIds.size())+3)*20) : 60;
}

// 引脚编号约定:
//   与门/或门/非门/异或/与非/或非/自定义: 多输入门 0=上输入 1=下输入 2=输出;非门 0=输入 1=输出
//   开关/指示灯: 0(开关=输出,指示灯=输入)
inline int GatePinCount(const Component& c) {
    if(c.type==GATE_MODULE)return c.module?(int)(c.module->inputIds.size()+c.module->outputIds.size()):0;
    if (c.type == SW_INPUT || c.type == SW_LED) return 1;
    if (c.type == GATE_NOT) return 2;
    return 3;
}

inline wxPoint GetRawPinPos(const Component& c, int pin) {
    if(c.type==GATE_MODULE && c.module) {
        const int inputs=(int)c.module->inputIds.size();
        if(pin<inputs)return wxPoint(c.x-70,c.y+(2*pin-inputs+1)*10);
        const int outputs=(int)c.module->outputIds.size();
        return wxPoint(c.x+70,c.y+(2*(pin-inputs)-outputs+1)*10);
    }
    if (c.type == GATE_AND) {
        if (pin == 0) return wxPoint(c.x - 20, c.y - 10);
        if (pin == 1) return wxPoint(c.x - 20, c.y + 10);
        return wxPoint(c.x + 20, c.y);
    }
    if (c.type == GATE_NAND) {   // 与非门:输入同与门,输出在小圆圈右侧
        if (pin == 0) return wxPoint(c.x - 20, c.y - 10);
        if (pin == 1) return wxPoint(c.x - 20, c.y + 10);
        return wxPoint(c.x + 29, c.y);
    }
    if (c.type == GATE_NOR) {    // 或非门:输入同或门,输出在小圆圈右侧
        if (pin == 0) return wxPoint(c.x - 22, c.y - 9);
        if (pin == 1) return wxPoint(c.x - 22, c.y + 9);
        return wxPoint(c.x + 29, c.y);
    }
    if (c.type == GATE_OR || c.type == GATE_XOR) {
        if (pin == 0) return wxPoint(c.x - 22, c.y - 9);
        if (pin == 1) return wxPoint(c.x - 22, c.y + 9);
        return wxPoint(c.x + 20, c.y);
    }
    if (c.type == GATE_NOT) {
        if (pin == 0) return wxPoint(c.x - 16, c.y);
        return wxPoint(c.x + 19, c.y);
    }
    if (c.type == GATE_CUSTOM) {   // 自定义元件:方框,左两入右一出
        if (pin == 0) return wxPoint(c.x - 24, c.y - 10);
        if (pin == 1) return wxPoint(c.x - 24, c.y + 10);
        return wxPoint(c.x + 24, c.y);
    }
    if (c.type == SW_INPUT)  return wxPoint(c.x + 22, c.y);   // 开关:输出在右
    return wxPoint(c.x - 20, c.y);                            // 指示灯:输入在左
}

inline wxPoint GetPinPos(const Component& c, int pin) {
    return SnapGrid(GetRawPinPos(c, pin));
}

// 给树控件挂的数据:点哪个树节点,就能查出对应哪种元件
#include <wx/treectrl.h>
class GateItemData : public wxTreeItemData {
public:
    GateItemData(GateType t, int customIdx = -1, bool defineNew = false)
        : type(t), customIdx(customIdx), defineNew(defineNew) {}
    GateType type;
    int  customIdx;    // 自定义元件在"用户定义列表"里的下标
    bool defineNew;    // true = 这个节点是"新建自定义元件…"入口
};
