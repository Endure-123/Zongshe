// Simulator.cpp
#include "Simulator.h"
#include "DrawBoard.h"
#include "Component.h"
#include <cmath>
#include <algorithm>

// ========= 工具：误差判断 =========
static inline bool NearlyEqualPt(const wxPoint& a, const wxPoint& b, int tol = 10) {
    return (std::abs(a.x - b.x) <= tol) && (std::abs(a.y - b.y) <= tol);
}

Simulator::Simulator(DrawBoard* board) : m_board(board) {}

void Simulator::BuildNetlist() {
    m_nets.clear();
    if (!m_board) return;

    struct FlatPin { wxPoint pt; PinRef ref; bool isOutput; };
    std::vector<FlatPin> flat;

    // 展开所有组件引脚
    for (int i = 0; i < (int)m_board->components.size(); ++i) {
        auto* c = m_board->components[i].get();
        auto pins = c->GetPins(); // 最后一个为输出
        for (int p = 0; p < (int)pins.size(); ++p) {
            FlatPin fp;
            fp.pt = pins[p];
            fp.ref = PinRef{ i, p };
            fp.isOutput = (p == (int)pins.size() - 1);
            flat.push_back(fp);
        }
    }

    // 每条 wire → 生成 SimNet
    for (int w = 0; w < (int)m_board->wires.size(); ++w) {
        const auto& poly = m_board->wires[w];
        if (poly.size() < 2) continue;
        const wxPoint& p0 = poly.front();
        const wxPoint& p1 = poly.back();

        SimNet net;
        net.wireIndices.push_back(w);

        auto bindEnd = [&](const wxPoint& endpoint) {
            for (const auto& fp : flat) {
                // 容差放宽为 10 像素
                if (NearlyEqualPt(fp.pt, endpoint, 10)) {
                    if (fp.isOutput) {
                        if (!net.driver.has_value())
                            net.driver = fp.ref;
                    }
                    else {
                        net.loads.push_back(fp.ref);
                    }
                }
            }
            };

        bindEnd(p0);
        bindEnd(p1);
        m_nets.push_back(std::move(net));
    }
}

void Simulator::ComputeAllGateOutputs(std::vector<bool>& gateOut) {
    gateOut.assign(m_board->components.size(), false);

    for (int i = 0; i < (int)m_board->components.size(); ++i) {
        auto* c = m_board->components[i].get();
        auto pins = c->GetPins();
        int inCount = std::max(0, (int)pins.size() - 1);
        std::vector<bool> ins(inCount, false);

        // 从 net 中收集输入信号
        for (const auto& net : m_nets) {
            bool netVal = net.value;
            for (const auto& ld : net.loads) {
                if (ld.compIdx == i && ld.pinIdx >= 0 && ld.pinIdx < inCount) {
                    ins[ld.pinIdx] = netVal;
                }
            }
        }

        switch (c->m_type) {
        case ComponentType::NODE_START: {
            auto it = m_startNodeValue.find(i);
            gateOut[i] = (it != m_startNodeValue.end()) ? it->second : false;
            break;
        }
        case ComponentType::NODE_END: {
            //  修改：终止节点输出等于输入信号
            bool v = false;
            for (bool b : ins) v = v || b;
            gateOut[i] = v;
            break;
        }
        case ComponentType::ANDGATE: {
            bool v = true;
            for (bool b : ins) v = v && b;
            gateOut[i] = (inCount > 0) ? v : false;
            break;
        }
        case ComponentType::ORGATE: {
            bool v = false;
            for (bool b : ins) v = v || b;
            gateOut[i] = (inCount > 0) ? v : false;
            break;
        }
        case ComponentType::NOTGATE: {
            gateOut[i] = (inCount >= 1) ? !ins[0] : true;
            break;
        }
        case ComponentType::NANDGATE: {
            bool v = true;
            for (bool b : ins) v = v && b;
            gateOut[i] = (inCount > 0) ? !v : true;
            break;
        }
        case ComponentType::NORGATE: {
            bool v = false;
            for (bool b : ins) v = v || b;
            gateOut[i] = (inCount > 0) ? !v : true;
            break;
        }
        case ComponentType::XORGATE: {
            bool v = false;
            for (bool b : ins) v ^= b;
            gateOut[i] = (inCount > 0) ? v : false;
            break;
        }
        default:
            gateOut[i] = false;
            break;
        }
    }
}

void Simulator::PropagateToInputs(const std::vector<bool>& gateOut) {
    for (auto& net : m_nets) {
        if (net.driver.has_value()) {
            const auto& d = *net.driver;
            bool outv = false;
            if (d.compIdx >= 0 && d.compIdx < (int)gateOut.size())
                outv = gateOut[d.compIdx];
            net.value = outv;
        }
        else {
            net.value = false;
        }
    }
}

void Simulator::UpdateNetValues() {
    // 当前逻辑简单，net.value 已更新
}

// ===== 修正版 Step：保证输入节点先驱动网络 =====
void Simulator::Step() {
    if (!m_board) return;

    // 1) 重新构网
    BuildNetlist();

    // 2) 准备 gateOut，先只写入“起始节点”的电平，其他默认 false
    std::vector<bool> gateOut(m_board->components.size(), false);
    for (int i = 0; i < (int)m_board->components.size(); ++i) {
        auto* c = m_board->components[i].get();
        if (c->m_type == ComponentType::NODE_START) {
            auto it = m_startNodeValue.find(i);
            gateOut[i] = (it != m_startNodeValue.end()) ? it->second : false;
        }
    }

    // 3) 先把“起始节点”的电平推进到各条线(仅播种一次，给门的输入提供有效电平)
    PropagateToInputs(gateOut);

    // 4) 现在各门的输入线上已有电平 -> 计算所有门的输出（含 OR/NOR… 及终止节点的显示值）
    ComputeAllGateOutputs(gateOut);

    // 5) 再将“所有门”的输出统一推进到各条线，用于着色/显示
    PropagateToInputs(gateOut);

    // 6) 目前 net.value 已经是最新的了
    UpdateNetValues();
}


void Simulator::Run() { m_running = true; }
void Simulator::Stop() { m_running = false; }

bool Simulator::IsWireHigh(int wireIndex) const {
    if (wireIndex < 0 || wireIndex >= (int)m_nets.size()) return false;
    return m_nets[wireIndex].value;
}

void Simulator::SetStartNodeValue(int compIdx, bool v) {
    m_startNodeValue[compIdx] = v;
}

bool Simulator::GetStartNodeValue(int compIdx) const {
    auto it = m_startNodeValue.find(compIdx);
    if (it == m_startNodeValue.end()) return false;
    return it->second;
}
