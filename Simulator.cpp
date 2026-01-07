// Simulator.cpp
#include "Simulator.h"
#include "DrawBoard.h"
#include "Component.h"

#include <algorithm>
#include <cmath>
#include <map>
#include <numeric>
#include <unordered_map>

// ==========================================================
//  仿真关键点：
//  1) BuildNetlist 必须正确处理：
//     - 线端点连接
//     - T 形连接（线的端点落在另一条线的中间）
//     - pin/节点点落在导线中间（无需手工把线拆段）
//  2) Step 必须迭代直到网络电平稳定（组合逻辑多级传播）
//  3) 译码器属于多输出元件，需要按 pinIdx 输出不同值
// ==========================================================


// ========= 工具：误差判断 =========
static inline bool NearlyEqualPt(const wxPoint& a, const wxPoint& b, int tol = 4) {
    return (std::abs(a.x - b.x) <= tol) && (std::abs(a.y - b.y) <= tol);
}

static inline long long PinKey(int compIdx, int pinIdx) {
    return (static_cast<long long>(compIdx) << 32) ^ static_cast<unsigned int>(pinIdx);
}

static inline int DivFloorInt(int v, int d) {
    if (d <= 0) return 0;
    if (v >= 0) return v / d;
    return -(((-v) + d - 1) / d);
}

static inline bool PointOnSegmentTol(const wxPoint& p, const wxPoint& a, const wxPoint& b, int tol) {
    // 轴对齐线段（Manhattan） + 少量容差
    if (a.x == b.x) {
        const int x = a.x;
        const int y0 = std::min(a.y, b.y);
        const int y1 = std::max(a.y, b.y);
        return (std::abs(p.x - x) <= tol) && (p.y >= y0 - tol) && (p.y <= y1 + tol);
    }
    if (a.y == b.y) {
        const int y = a.y;
        const int x0 = std::min(a.x, b.x);
        const int x1 = std::max(a.x, b.x);
        return (std::abs(p.y - y) <= tol) && (p.x >= x0 - tol) && (p.x <= x1 + tol);
    }

    // 兜底：一般不会出现（你的布线工具是 Manhattan）
    const double ax = a.x, ay = a.y;
    const double bx = b.x, by = b.y;
    const double px = p.x, py = p.y;
    const double vx = bx - ax;
    const double vy = by - ay;
    const double wx_ = px - ax;
    const double wy_ = py - ay;
    const double vv = vx * vx + vy * vy;
    if (vv <= 1e-9) return NearlyEqualPt(p, a, tol);

    double t = (wx_ * vx + wy_ * vy) / vv;
    if (t < 0.0) t = 0.0;
    if (t > 1.0) t = 1.0;
    const double cx = ax + t * vx;
    const double cy = ay + t * vy;
    const double dx = px - cx;
    const double dy = py - cy;
    return (dx * dx + dy * dy) <= double(tol * tol);
}

// ========= 并查集 =========
struct UF {
    std::vector<int> parent;
    explicit UF(int n) : parent(n) { std::iota(parent.begin(), parent.end(), 0); }
    int find(int i) {
        if (parent[i] == i) return i;
        return parent[i] = find(parent[i]);
    }
    void unite(int i, int j) {
        const int ri = find(i);
        const int rj = find(j);
        if (ri != rj) parent[ri] = rj;
    }
};

// ========= 扁平引脚 =========
struct FlatPin {
    wxPoint pt;
    PinRef ref;
    bool isOutput = false;
};

Simulator::Simulator(DrawBoard* board) : m_board(board) {}


// ==========================================================
// BuildNetlist：从几何连通性构造仿真网络
// ==========================================================
void Simulator::BuildNetlist() {
    m_nets.clear();
    m_wire_to_net_map.clear();
    if (!m_board) return;

    // 连接容差：不要太大（避免误连），但要能覆盖缩放/取整造成的 1~2 像素误差
    // 若你有“缩放很多次后偶尔断连”，可把 4 调到 5；一般别再往上。
    constexpr int CONNECT_TOL = 4;
    constexpr int CELL = CONNECT_TOL + 1;

    // 1) 收集全部组件引脚
    std::vector<FlatPin> flat;
    flat.reserve(256);

    std::vector<wxPoint> nodePts;     // pin 点 + wire 顶点点
    nodePts.reserve(512);
    std::vector<int> flatNodeIdx;     // flat[i] -> nodePts idx
    flatNodeIdx.reserve(256);

    for (int i = 0; i < (int)m_board->components.size(); ++i) {
        auto* c = m_board->components[i].get();
        if (!c) continue;
        const auto pins = c->GetPins();
        for (int p = 0; p < (int)pins.size(); ++p) {
            FlatPin fp;
            fp.pt = pins[p];
            fp.ref = PinRef{ i, p };
            fp.isOutput = false;

            switch (c->m_type) {
            case ComponentType::NODE_START:
                fp.isOutput = true;
                break;
            case ComponentType::NODE_END:
            case ComponentType::NODE_BASIC:
                fp.isOutput = false;
                break;
            case ComponentType::DECODER24:
                fp.isOutput = (p >= 3); // EN,A0,A1,Y0..Y3
                break;
            case ComponentType::DECODER38:
                fp.isOutput = (p >= 4); // EN,A0,A1,A2,Y0..Y7
                break;
            case ComponentType::NOTGATE:
            case ComponentType::ANDGATE:
            case ComponentType::ORGATE:
            case ComponentType::NANDGATE:
            case ComponentType::NORGATE:
            case ComponentType::XORGATE:
            case ComponentType::XNORGATE:
                fp.isOutput = (p == (int)pins.size() - 1);
                break;
            default:
                fp.isOutput = false;
                break;
            }

            flatNodeIdx.push_back((int)nodePts.size());
            nodePts.push_back(fp.pt);
            flat.push_back(fp);
        }
    }
    if (flat.empty()) return;

    // 2) 收集所有 wire 的折线点
    std::vector<int> wireAnyNode(m_board->wires.size(), -1);
    for (int w = 0; w < (int)m_board->wires.size(); ++w) {
        const auto& poly = m_board->wires[w];
        if (poly.size() < 2) continue;
        wireAnyNode[w] = (int)nodePts.size();
        for (const auto& pt : poly) nodePts.push_back(pt);
    }
    if (nodePts.empty()) return;

    UF uf((int)nodePts.size());

    // 3) 合并“几乎重合”的点（用于抗缩放取整误差）
    {
        auto CellKey = [](int cx, int cy) -> long long {
            return (static_cast<long long>(cx) << 32) ^ static_cast<unsigned int>(cy);
            };
        std::unordered_map<long long, std::vector<int>> buckets;
        buckets.reserve(nodePts.size() * 2);

        for (int idx = 0; idx < (int)nodePts.size(); ++idx) {
            const auto& p = nodePts[idx];
            const int cx = DivFloorInt(p.x, CELL);
            const int cy = DivFloorInt(p.y, CELL);

            // 查邻域桶，避免漏掉边界情况
            for (int dx = -1; dx <= 1; ++dx) {
                for (int dy = -1; dy <= 1; ++dy) {
                    const long long k = CellKey(cx + dx, cy + dy);
                    auto it = buckets.find(k);
                    if (it == buckets.end()) continue;
                    for (int j : it->second) {
                        if (NearlyEqualPt(p, nodePts[j], CONNECT_TOL)) uf.unite(idx, j);
                    }
                }
            }
            buckets[CellKey(cx, cy)].push_back(idx);
        }
    }

    // 4) 通过每一段导线把段上的点合并
    //    支持：线端点落在另一条线的中间、pin 落在线中间、无需拆线
    for (int w = 0; w < (int)m_board->wires.size(); ++w) {
        const auto& poly = m_board->wires[w];
        if (poly.size() < 2) continue;

        for (size_t k = 1; k < poly.size(); ++k) {
            const wxPoint a = poly[k - 1];
            const wxPoint b = poly[k];
            if (a == b) continue;

            const int minx = std::min(a.x, b.x) - CONNECT_TOL;
            const int maxx = std::max(a.x, b.x) + CONNECT_TOL;
            const int miny = std::min(a.y, b.y) - CONNECT_TOL;
            const int maxy = std::max(a.y, b.y) + CONNECT_TOL;

            std::vector<int> onSeg;
            onSeg.reserve(8);
            for (int idx = 0; idx < (int)nodePts.size(); ++idx) {
                const auto& p = nodePts[idx];
                if (p.x < minx || p.x > maxx || p.y < miny || p.y > maxy) continue;
                if (PointOnSegmentTol(p, a, b, CONNECT_TOL)) onSeg.push_back(idx);
            }
            for (size_t t = 1; t < onSeg.size(); ++t) uf.unite(onSeg[0], onSeg[t]);
        }
    }

    // 5) 根据 UF root 建立 SimNet（只从“组件引脚”生成 driver/loads）
    std::map<int, SimNet> root_to_net;
    for (int i = 0; i < (int)flat.size(); ++i) {
        const int root = uf.find(flatNodeIdx[i]);
        SimNet& net = root_to_net[root];

        const FlatPin& fp = flat[i];
        if (fp.isOutput) {
            if (!net.driver.has_value()) {
                net.driver = fp.ref;
            }
        }
        else {
            net.loads.push_back(fp.ref);
        }
    }

    // 6) wire -> net 关联（按 wire 任意顶点的 root 归属）
    for (int w = 0; w < (int)m_board->wires.size(); ++w) {
        const int any = (w >= 0 && w < (int)wireAnyNode.size()) ? wireAnyNode[w] : -1;
        if (any < 0) continue;

        const int root = uf.find(any);
        auto it = root_to_net.find(root);
        if (it != root_to_net.end()) {
            it->second.wireIndices.push_back(w);
        }
    }

    // 7) 转移到 m_nets，并建立 wire -> net 映射
    m_nets.reserve(root_to_net.size());
    int net_idx = 0;
    for (auto& pair : root_to_net) {
        auto& indices = pair.second.wireIndices;
        std::sort(indices.begin(), indices.end());
        indices.erase(std::unique(indices.begin(), indices.end()), indices.end());

        for (int wire_idx : indices) m_wire_to_net_map[wire_idx] = net_idx;
        m_nets.push_back(std::move(pair.second));
        ++net_idx;
    }
}


// ==========================================================
// ComputeAllGateOutputs：基于当前 net.value 计算每个元件输出
// ==========================================================
void Simulator::ComputeAllGateOutputs(std::vector<bool>& gateOut,
    std::unordered_map<long long, bool>& multiOut) {

    if (!m_board) return;

    gateOut.assign(m_board->components.size(), false);
    multiOut.clear();

    for (int i = 0; i < (int)m_board->components.size(); ++i) {
        auto* c = m_board->components[i].get();
        if (!c) continue;
        const auto pins = c->GetPins();

        // 确定输入引脚数量
        int inCount = 0;
        switch (c->m_type) {
        case ComponentType::NODE_START:  inCount = 0; break;
        case ComponentType::NODE_END:    inCount = 1; break;
        case ComponentType::NODE_BASIC:  inCount = (int)pins.size(); break;
        case ComponentType::DECODER24:   inCount = 3; break; // EN,A0,A1
        case ComponentType::DECODER38:   inCount = 4; break; // EN,A0,A1,A2
        case ComponentType::NOTGATE:     inCount = 1; break;
        default:
            inCount = std::max(0, (int)pins.size() - 1);
            break;
        }

        // 收集输入信号
        std::vector<bool> ins(inCount, false);
        for (const auto& net : m_nets) {
            const bool netVal = net.value;
            for (const auto& ld : net.loads) {
                if (ld.compIdx == i && ld.pinIdx >= 0 && ld.pinIdx < inCount) {
                    ins[ld.pinIdx] = netVal;
                }
            }
        }

        // 计算输出
        switch (c->m_type) {
        case ComponentType::NODE_START: {
            auto it = m_startNodeValue.find(i);
            gateOut[i] = (it != m_startNodeValue.end()) ? it->second : false;
            break;
        }
        case ComponentType::NODE_END: {
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
        case ComponentType::XNORGATE: {
            bool v = false;
            for (bool b : ins) v ^= b;
            gateOut[i] = (inCount > 0) ? !v : true;
            break;
        }
        case ComponentType::DECODER24: {
            // 7 pins: EN(0),A0(1),A1(2),Y0(3)..Y3(6)
            const bool en = (inCount >= 1) ? ins[0] : false;
            const int a0 = (inCount >= 2 && ins[1]) ? 1 : 0;
            const int a1 = (inCount >= 3 && ins[2]) ? 1 : 0;
            const int idx = (a1 << 1) | a0;
            for (int outPin = 3; outPin <= 6; ++outPin) {
                const bool v = en && ((outPin - 3) == idx);
                multiOut[PinKey(i, outPin)] = v;
            }
            gateOut[i] = false;
            break;
        }
        case ComponentType::DECODER38: {
            // 12 pins: EN(0),A0(1),A1(2),A2(3),Y0(4)..Y7(11)
            const bool en = (inCount >= 1) ? ins[0] : false;
            const int a0 = (inCount >= 2 && ins[1]) ? 1 : 0;
            const int a1 = (inCount >= 3 && ins[2]) ? 1 : 0;
            const int a2 = (inCount >= 4 && ins[3]) ? 1 : 0;
            const int idx = (a2 << 2) | (a1 << 1) | a0;
            for (int outPin = 4; outPin <= 11; ++outPin) {
                const bool v = en && ((outPin - 4) == idx);
                multiOut[PinKey(i, outPin)] = v;
            }
            gateOut[i] = false;
            break;
        }
        case ComponentType::NODE_BASIC:
        default:
            gateOut[i] = false;
            break;
        }
    }
}


// ==========================================================
// Step：迭代传播直到稳定（Settle）
// ==========================================================
void Simulator::Step() {
    if (!m_board) return;

    const int MAX_ITERATIONS = 64;
    bool changed = true;

    std::vector<bool> gateOut(m_board->components.size(), false);
    std::unordered_map<long long, bool> multiOut;
    multiOut.reserve(m_board->components.size() * 4);

    for (int iter = 0; iter < MAX_ITERATIONS && changed; ++iter) {
        changed = false;

        // 1) 计算输出
        ComputeAllGateOutputs(gateOut, multiOut);

        // 2) 写回网络
        for (auto& net : m_nets) {
            bool new_val = false;

            if (net.driver.has_value()) {
                const auto& d = *net.driver;
                if (d.compIdx >= 0 && d.compIdx < (int)m_board->components.size()) {
                    auto* dc = m_board->components[d.compIdx].get();
                    if (dc && (dc->m_type == ComponentType::DECODER24 || dc->m_type == ComponentType::DECODER38)) {
                        auto it = multiOut.find(PinKey(d.compIdx, d.pinIdx));
                        new_val = (it != multiOut.end()) ? it->second : false;
                    }
                    else {
                        if (d.compIdx >= 0 && d.compIdx < (int)gateOut.size()) {
                            new_val = gateOut[d.compIdx];
                        }
                    }
                }
            }

            if (net.value != new_val) {
                net.value = new_val;
                changed = true;
            }
        }
    }
}

void Simulator::Run() { m_running = true; }
void Simulator::Stop() { m_running = false; }


// ==========================================================
// IsWireHigh：wire 着色查询
// ==========================================================
bool Simulator::IsWireHigh(int wireIndex) const {
    auto it = m_wire_to_net_map.find(wireIndex);
    if (it == m_wire_to_net_map.end()) return false;

    const int net_idx = it->second;
    if (net_idx < 0 || net_idx >= (int)m_nets.size()) return false;

    return m_nets[net_idx].value;
}

void Simulator::SetStartNodeValue(int compIdx, bool v) {
    m_startNodeValue[compIdx] = v;
}

bool Simulator::GetStartNodeValue(int compIdx) const {
    auto it = m_startNodeValue.find(compIdx);
    if (it == m_startNodeValue.end()) return false;
    return it->second;
}
