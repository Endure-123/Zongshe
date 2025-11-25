// Simulator.cpp
#include "Simulator.h"
#include "DrawBoard.h"
#include "Component.h"
#include <cmath>
#include <algorithm>
#include <numeric> // For std::iota
#include <map>

// ========= 工具：误差判断 =========
static inline bool NearlyEqualPt(const wxPoint& a, const wxPoint& b, int tol = 10) {
    return (std::abs(a.x - b.x) <= tol) && (std::abs(a.y - b.y) <= tol);
}

// ========= 辅助结构：并查集 =========
struct UF {
    std::vector<int> parent;
    UF(int n) : parent(n) { std::iota(parent.begin(), parent.end(), 0); } // 初始化，每个元素的父节点是自己
    int find(int i) {
        if (parent[i] == i) return i;
        return parent[i] = find(parent[i]); // 路径压缩
    }
    void unite(int i, int j) {
        int root_i = find(i);
        int root_j = find(j);
        if (root_i != root_j) parent[root_i] = root_j; // 合并
    }
};

// ========= 辅助结构：引脚定义 =========
struct FlatPin {
    wxPoint pt;     // 坐标
    PinRef ref;     // 引用 (compIdx, pinIdx)
    bool isOutput;  // 是否为输出引脚
};

// ========= 辅助函数：查找坐标点上的所有引脚 =========
static std::vector<int> FindPinsAt(const wxPoint& pt, const std::vector<FlatPin>& flat, int tol) {
    std::vector<int> indices;
    for (int i = 0; i < (int)flat.size(); ++i) {
        if (NearlyEqualPt(pt, flat[i].pt, tol)) {
            indices.push_back(i);
        }
    }
    return indices;
}


Simulator::Simulator(DrawBoard* board) : m_board(board) {}

// ==========================================================
// ★ 重构: BuildNetlist (使用并查集)
// ==========================================================
void Simulator::BuildNetlist() {
    m_nets.clear();
    m_wire_to_net_map.clear();
    if (!m_board) return;

    std::vector<FlatPin> flat; // 存储所有组件的所有引脚

    // 1. 展开所有组件引脚, 并确定 I/O 属性
    for (int i = 0; i < (int)m_board->components.size(); ++i) {
        auto* c = m_board->components[i].get();
        auto pins = c->GetPins();
        for (int p = 0; p < (int)pins.size(); ++p) {
            FlatPin fp;
            fp.pt = pins[p];
            fp.ref = PinRef{ i, p };
            fp.isOutput = false; // 默认是输入

            // 根据组件类型判断引脚是否为输出
            switch (c->m_type) {
            case ComponentType::NODE_START:
                fp.isOutput = true; // 起始节点是驱动源
                break;
            case ComponentType::NODE_END:
            case ComponentType::NODE_BASIC:
                fp.isOutput = false; // 终止/普通节点 始终是负载
                break;
            case ComponentType::DECODER24: // 7 pins: EN, A0, A1, Y0, Y1, Y2, Y3
                fp.isOutput = (p >= 3); // Y0-Y3 是输出
                break;
            case ComponentType::DECODER38: // 12 pins: EN, A0, A1, A2, Y0...Y7
                fp.isOutput = (p >= 4); // Y0-Y7 是输出
                break;
            case ComponentType::NOTGATE:
            case ComponentType::ANDGATE:
            case ComponentType::ORGATE:
            case ComponentType::NANDGATE:
            case ComponentType::NORGATE:
            case ComponentType::XORGATE:
            case ComponentType::XNORGATE:
                fp.isOutput = (p == (int)pins.size() - 1); // 默认最后一个是输出
                break;
            default:
                fp.isOutput = false;
                break;
            }
            flat.push_back(fp);
        }
    }

    if (flat.empty()) return;

    // 2. 使用并查集 (UF) 合并连接的引脚
    UF uf(flat.size());

    // 2a. 通过 wires 合并
    for (int w = 0; w < (int)m_board->wires.size(); ++w) {
        const auto& poly = m_board->wires[w];
        if (poly.size() < 2) continue;

        // 查找导线两端连接的所有引脚
        auto pinsA = FindPinsAt(poly.front(), flat, 10);
        auto pinsB = FindPinsAt(poly.back(), flat, 10);

        // 2b. (重要) 合并所有在 *同一* 端点上的引脚 (例如 NODE_BASIC)
        for (size_t i = 1; i < pinsA.size(); ++i) uf.unite(pinsA[0], pinsA[i]);
        for (size_t i = 1; i < pinsB.size(); ++i) uf.unite(pinsB[0], pinsB[i]);

        // 2c. 连接 A 端和 B 端
        if (!pinsA.empty() && !pinsB.empty()) {
            uf.unite(pinsA[0], pinsB[0]);
        }
    }

    // 3. 根据 UF 的 root 建立 SimNets
    std::map<int, SimNet> root_to_net;
    for (int i = 0; i < (int)flat.size(); ++i) {
        int root = uf.find(i);
        SimNet& net = root_to_net[root];

        const FlatPin& fp = flat[i];
        if (fp.isOutput) {
            if (!net.driver.has_value()) { // 只取第一个驱动
                net.driver = fp.ref;
            }
            // else: 多个驱动源连接在一起 (短路), 忽略后续的
        }
        else {
            // 避免重复添加负载 (UF 可能导致同一引脚被多次访问)
            bool found = false;
            for (const auto& ld : net.loads) if (ld == fp.ref) found = true;
            if (!found) net.loads.push_back(fp.ref);
        }
    }

    // 4. 将 wire 索引关联到 SimNets
    for (int w = 0; w < (int)m_board->wires.size(); ++w) {
        const auto& poly = m_board->wires[w];
        if (poly.size() < 2) continue;

        // 找到线缆端点连接的第一个 pin
        auto pinsA = FindPinsAt(poly.front(), flat, 10);
        if (!pinsA.empty()) {
            int root = uf.find(pinsA[0]);
            root_to_net[root].wireIndices.push_back(w);
        }
        else {
            auto pinsB = FindPinsAt(poly.back(), flat, 10);
            if (!pinsB.empty()) {
                int root = uf.find(pinsB[0]);
                root_to_net[root].wireIndices.push_back(w);
            }
        }
    }

    // 5. 转移到 m_nets, 并建立 wire -> net 映射
    m_nets.reserve(root_to_net.size());
    int net_idx = 0;
    for (auto& pair : root_to_net) {
        auto& indices = pair.second.wireIndices;
        // 去重
        std::sort(indices.begin(), indices.end());
        indices.erase(std::unique(indices.begin(), indices.end()), indices.end());

        // 建立映射
        for (int wire_idx : indices) {
            m_wire_to_net_map[wire_idx] = net_idx;
        }

        m_nets.push_back(std::move(pair.second));
        net_idx++;
    }
}


// ==========================================================
// ★ 修正: ComputeAllGateOutputs
// ==========================================================
void Simulator::ComputeAllGateOutputs(std::vector<bool>& gateOut) {
    gateOut.assign(m_board->components.size(), false);

    for (int i = 0; i < (int)m_board->components.size(); ++i) {
        auto* c = m_board->components[i].get();
        auto pins = c->GetPins();

        // 确定输入引脚数量
        int inCount = 0;
        switch (c->m_type) {
        case ComponentType::NODE_START:  inCount = 0; break;
        case ComponentType::NODE_END:    inCount = 1; break; // 假定1个输入
        case ComponentType::NODE_BASIC:  inCount = (int)pins.size(); break; // 全是输入
        case ComponentType::DECODER24:   inCount = 3; break; // EN, A0, A1
        case ComponentType::DECODER38:   inCount = 4; break; // EN, A0, A1, A2
        case ComponentType::NOTGATE:     inCount = 1; break;
        default: // AND, OR, NAND, NOR, XOR, XNOR
            inCount = std::max(0, (int)pins.size() - 1);
            break;
        }

        // 从 net 中收集输入信号
        std::vector<bool> ins(inCount, false);
        for (const auto& net : m_nets) {
            bool netVal = net.value;
            for (const auto& ld : net.loads) {
                // 如果这个负载是当前组件 i, 并且引脚索引是输入引脚
                if (ld.compIdx == i && ld.pinIdx >= 0 && ld.pinIdx < inCount) {
                    ins[ld.pinIdx] = netVal;
                }
            }
        }

        // 根据输入计算输出 (gateOut[i] 存储组件 i 的 *主* 输出)
        // (注意：多输出组件(译码器)的逻辑未在此处实现)
        switch (c->m_type) {
        case ComponentType::NODE_START: {
            auto it = m_startNodeValue.find(i);
            gateOut[i] = (it != m_startNodeValue.end()) ? it->second : false;
            break;
        }
        case ComponentType::NODE_END: {
            // ★ 修正: 终止节点"输出"它唯一的输入值 (用于DrawNodeStates着色)
            gateOut[i] = (inCount >= 1) ? ins[0] : false;
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
                                   // ★ 新增
        case ComponentType::XNORGATE: {
            bool v = false;
            for (bool b : ins) v ^= b;
            gateOut[i] = (inCount > 0) ? !v : true; // XNOR
            break;
        }
                                    // ★ 译码器和普通节点没有单一输出
        case ComponentType::DECODER24:
        case ComponentType::DECODER38:
        case ComponentType::NODE_BASIC:
        default:
            gateOut[i] = false;
            break;
        }
    }
}

void Simulator::UpdateNetValues() {
    // 当前逻辑简单，net.value 已更新
}

// ==========================================================
// ★ 重构: Step (改为“Settle”稳定逻辑)
// ==========================================================
void Simulator::Step() {
    if (!m_board) return;

    // (BuildNetlist 已移到 SimStart 和 SimStep(DrawBoard) 中)

    const int MAX_ITERATIONS = 50; // Settle 迭代上限, 防止振荡器死循环
    bool changed = true;
    // 存储所有(单输出)门的值
    std::vector<bool> gateOut(m_board->components.size(), false);

    for (int iter = 0; iter < MAX_ITERATIONS && changed; ++iter) {
        changed = false;

        // 1. 计算所有门输出 (基于 m_nets 当前值)
        ComputeAllGateOutputs(gateOut);

        // 2. 将 gateOut 传播回 m_nets
        // (注意：这个传播逻辑对多输出组件(译码器)无效)
        for (auto& net : m_nets) {
            bool new_val = false;
            if (net.driver.has_value()) {
                const auto& d = *net.driver;
                // 检查是否为多输出组件
                auto* c = m_board->components[d.compIdx].get();
                if (c->m_type == ComponentType::DECODER24 || c->m_type == ComponentType::DECODER38) {
                    // TODO: 此处需要译码器的完整仿真逻辑
                    // 暂时保持 false
                }
                else {
                    // 单输出组件
                    if (d.compIdx >= 0 && d.compIdx < (int)gateOut.size()) {
                        new_val = gateOut[d.compIdx];
                    }
                }
            }
            // else: no driver, new_val stays false

            if (net.value != new_val) {
                net.value = new_val;
                changed = true; // 状态改变，需要再迭代
            }
        }
    }
    // 循环结束，电路已 "settle"
}


void Simulator::Run() { m_running = true; }
void Simulator::Stop() { m_running = false; }

// ==========================================================
// ★ 修正: IsWireHigh (使用映射表)
// ==========================================================
bool Simulator::IsWireHigh(int wireIndex) const {
    auto it = m_wire_to_net_map.find(wireIndex);
    if (it == m_wire_to_net_map.end()) return false; // 导线未连接到网络

    int net_idx = it->second;
    if (net_idx < 0 || net_idx >= (int)m_nets.size()) return false;

    return m_nets[net_idx].value; // 返回所属网络的电平
}

void Simulator::SetStartNodeValue(int compIdx, bool v) {
    m_startNodeValue[compIdx] = v;
}

bool Simulator::GetStartNodeValue(int compIdx) const {
    auto it = m_startNodeValue.find(compIdx);
    if (it == m_startNodeValue.end()) return false;
    return it->second;
}