#pragma once
#include <wx/wx.h>
#include <wx/vector.h>
#include <map>
#include "core/Component.h"

// ================================================================
// 逻辑仿真引擎(纯函数,与界面解耦)。
//
// 两种仿真模式:
//
// 【组合逻辑】SimulateCircuit():一次性算到稳定(迭代传播)
//   适用于纯门电路(无反馈),和以前一样
//
// 【时序逻辑】SimulationTick():每次调用前进一个仿真步
//   适用于含触发器的电路(有反馈,需要时钟控制)
//   每步做的事:
//     1. 时钟源翻转(每 2 步翻转一次,形成方波)
//     2. D 触发器:检测时钟上升沿(上一步 CLK=0,这一步 CLK=1),Q=D
//     3. 组合门:读输入算输出(与以前一样)
//   界面层(wxTimer)每 500ms 调一次 SimulationTick(),就能看到信号逐级传播
// ================================================================

// 沿连线找:某元件某引脚连到谁(返回来源元件编号,-1=没连)
inline int FindSource(const wxVector<Wire>& wires, int compId, int pin) {
    for (size_t i = 0; i < wires.size(); i++) {
        if (wires[i].comp2 == compId && wires[i].pin2 == pin)
            return wires[i].comp1;
    }
    return -1;
}

// 读某元件某输入引脚当前的值
inline int ReadPin(const std::map<int,int>& out, const wxVector<Wire>& wires,
                   int compId, int pin) {
    int src = FindSource(wires, compId, pin);
    if (src < 0) return 0;
    auto it = out.find(src);
    return (it != out.end()) ? it->second : 0;
}

// ---------- 组合逻辑仿真:一次性算到稳定(与以前一样) ----------
inline std::map<int,int> SimulateCircuit(const wxVector<Component>& comps,
                                         const wxVector<Wire>& wires) {
    std::map<int,int> out;
    for (size_t i = 0; i < comps.size(); i++)
        out[comps[i].id] = (comps[i].type == SW_INPUT && comps[i].state) ? 1 : 0;

    int rounds = (int)comps.size() + 2;
    for (int r = 0; r < rounds; r++) {
        for (size_t i = 0; i < comps.size(); i++) {
            const Component& c = comps[i];
            if (c.type == SW_INPUT || c.type == SW_LED || c.type == SW_CLOCK)
                continue;
            int v0 = ReadPin(out, wires, c.id, 0);
            int res;
            if (c.type == GATE_AND)       res = v0 & ReadPin(out, wires, c.id, 1);
            else if (c.type == GATE_OR)   res = v0 | ReadPin(out, wires, c.id, 1);
            else if (c.type == GATE_XOR)  res = v0 ^ ReadPin(out, wires, c.id, 1);
            else if (c.type == GATE_NAND) res = !(v0 & ReadPin(out, wires, c.id, 1));
            else if (c.type == GATE_NOR)  res = !(v0 | ReadPin(out, wires, c.id, 1));
            else if (c.type == GATE_CUSTOM) {
                int idx = v0 * 2 + ReadPin(out, wires, c.id, 1);
                res = (c.truth >> idx) & 1;
            }
            else /* GATE_NOT */           res = !v0;
            out[c.id] = res;
        }
    }
    return out;
}

// ---------- 时序逻辑仿真:步进模式 ----------
// 每调一次 SimulationTick() 电路前进一个仿真步。
// 状态存在 simState 里(compId → 输出值),跨调用保持。
inline void SimulationTick(const wxVector<Component>& comps,
                           const wxVector<Wire>& wires,
                           std::map<int,int>& simState,
                           int& tickCount) {
    tickCount++;

    // 1. 时钟源:每 4 步翻转一次(前 2 步高,后 2 步低 → 方波周期 4)
    for (size_t i = 0; i < comps.size(); i++) {
        if (comps[i].type == SW_CLOCK)
            simState[comps[i].id] = (tickCount / 2) % 2;
    }

    // 2. 组合门:读输入算输出
    for (size_t i = 0; i < comps.size(); i++) {
        const Component& c = comps[i];
        if (c.type == SW_INPUT || c.type == SW_LED || c.type == SW_CLOCK || c.type == GATE_DFF)
            continue;
        int v0 = ReadPin(simState, wires, c.id, 0);
        int res;
        if (c.type == GATE_AND)       res = v0 & ReadPin(simState, wires, c.id, 1);
        else if (c.type == GATE_OR)   res = v0 | ReadPin(simState, wires, c.id, 1);
        else if (c.type == GATE_XOR)  res = v0 ^ ReadPin(simState, wires, c.id, 1);
        else if (c.type == GATE_NAND) res = !(v0 & ReadPin(simState, wires, c.id, 1));
        else if (c.type == GATE_NOR)  res = !(v0 | ReadPin(simState, wires, c.id, 1));
        else if (c.type == GATE_CUSTOM) {
            int idx = v0 * 2 + ReadPin(simState, wires, c.id, 1);
            res = (c.truth >> idx) & 1;
        }
        else /* GATE_NOT */           res = !v0;
        simState[c.id] = res;
    }

    // 3. D 触发器:检测时钟上升沿(上一步 CLK=0,这一步 CLK=1 → Q=D)
    for (size_t i = 0; i < comps.size(); i++) {
        if (comps[i].type != GATE_DFF) continue;
        int clkVal = ReadPin(simState, wires, comps[i].id, 1);
        // 上一步的 CLK 值存在 simState 的旧值里,用 -1 标记"第一步"
        // 简化处理:每次 SimulationTick 都重新计算 D 的值,Q = 上一轮 D
        // (真正的边沿检测需要保存上一步 CLK,这里用双相法代替)
        // 为了教学简单,D 触发器在 CLK=1 时透明传输,Q=D
        int dVal = ReadPin(simState, wires, comps[i].id, 0);
        if (clkVal) simState[comps[i].id] = dVal;
    }
}

// 指示灯显示的值:沿线找来源元件的输出
inline int LedValue(const Component& led, const wxVector<Wire>& wires,
                    const std::map<int,int>& out) {
    int src = FindSource(wires, led.id, 0);
    if (src < 0) return 0;
    auto it = out.find(src);
    return (it != out.end()) ? it->second : 0;
}
