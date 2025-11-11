// Simulator.h
#pragma once
#include <wx/wx.h>
#include <vector>
#include <unordered_map>
#include <optional>

class DrawBoard;   // 前向声明
class Component;

// ========== 引脚引用结构 ==========
struct PinRef {
    int compIdx = -1;  // 组件下标
    int pinIdx = -1;  // 引脚下标（0..n-1；最后一个视为输出）
    bool operator==(const PinRef& o) const { return compIdx == o.compIdx && pinIdx == o.pinIdx; }
};

// ========== 仿真用网络结构 (避免与 bookshelf::Net 冲突) ==========
struct SimNet {
    std::optional<PinRef> driver;   // 驱动引脚
    std::vector<PinRef>   loads;    // 被驱动引脚
    bool value = false;             // 当前电平
    std::vector<int> wireIndices;   // 关联的 wire 索引，用于渲染
};

// ========== 仿真控制类 ==========
class Simulator {
public:
    explicit Simulator(DrawBoard* board);

    void BuildNetlist();   // 从 DrawBoard 生成仿真网络
    void Step();           // 单步仿真
    void Run();            // 启动连续仿真
    void Stop();           // 停止仿真
    bool IsRunning() const { return m_running; }

    bool IsWireHigh(int wireIndex) const; // 查询线的高低电平
    void SetStartNodeValue(int compIdx, bool v); // 设置起始节点输出值
    bool GetStartNodeValue(int compIdx) const;   // 获取起始节点输出值

    std::vector<SimNet> m_nets;                     // 仿真网络
    std::unordered_map<int, bool> m_startNodeValue; // 起始节点电平表

private:
    DrawBoard* m_board = nullptr;
    bool m_running = false;

    void ComputeAllGateOutputs(std::vector<bool>& gateOut);
    void PropagateToInputs(const std::vector<bool>& gateOut);
    void UpdateNetValues();
};
