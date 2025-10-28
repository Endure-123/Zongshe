#pragma once
#include <wx/wx.h>
#include <vector> 

// 新增更完整的门类型
enum ComponentType {
    ANDGATE,
    ORGATE,
    NOTGATE,
    NANDGATE,
    NORGATE,   // 注意：枚举名用 NORGATE（读作 NOR Gate）
    XORGATE,
    POWER,
    NODE_BASIC,     // 普通结点（小圆点，多线汇聚）
    NODE_START,     // 起始节点（信号源，箭头）
    NODE_END        // 终止节点（空心圆，信号终点）
};

class Component {
public:
    ComponentType m_type;
    double scale;
    bool scaling = false;
    wxPoint m_BoundaryPoints[4];

    Component(wxPoint center, ComponentType type) {
        this->scale = 1.0;
        this->m_type = type;
        this->m_center = center;
        this->m_isSelected = false;
    }
    virtual ~Component() {}

    wxPoint GetCenter() { return m_center; }
    void SetCenter(wxPoint center) { m_center = center; UpdateGeometry(); }
    void SetSelected(bool selected) { m_isSelected = selected; }

    virtual void UpdateGeometry() = 0;
    virtual void drawSelf(wxMemoryDC& memDC) = 0;
    virtual bool Isinside(const wxPoint& point) const = 0;
    virtual std::vector<wxPoint> GetPins() const { return {}; }

protected:
    bool m_isSelected;
    wxPoint m_center;
};

class Gate : public Component {
public:
    wxPoint pout; // 输出引脚位置（可选使用）
    Gate(wxPoint center, ComponentType type) : Component(center, type) {}
    virtual ~Gate() {}
    virtual void drawSelf(wxMemoryDC& memDC) = 0;
    virtual void UpdateGeometry() = 0;
    virtual bool Isinside(const wxPoint& point) const = 0;
};

// ---------- AND ----------
class ANDGate : public Gate {
public:
    ANDGate(wxPoint center, ComponentType type = ANDGATE) : Gate(center, type) {
        this->scale = 1.0;
        UpdateGeometry();
    }
    void drawSelf(wxMemoryDC& memDC) override;
    bool Isinside(const wxPoint& point) const override;
    void UpdateGeometry() override;
    std::vector<wxPoint> GetPins() const override;
};

// ---------- OR ----------
class ORGate : public Gate {
public:
    ORGate(wxPoint center, ComponentType type = ORGATE) : Gate(center, type) {
        this->scale = 1.0;
        UpdateGeometry();
    }
    void drawSelf(wxMemoryDC& memDC) override;
    bool Isinside(const wxPoint& point) const override;
    void UpdateGeometry() override;
    std::vector<wxPoint> GetPins() const override;
};

// ---------- NOT ----------
class NOTGate : public Gate {
public:
    NOTGate(wxPoint center, ComponentType type = NOTGATE) : Gate(center, type) {
        this->scale = 1.0;
        UpdateGeometry();
    }
    void drawSelf(wxMemoryDC& memDC) override;
    bool Isinside(const wxPoint& point) const override;
    void UpdateGeometry() override;
    std::vector<wxPoint> GetPins() const override;
};

// ---------- NAND（= AND + 气泡） ----------
class NANDGate : public Gate {
public:
    NANDGate(wxPoint center, ComponentType type = NANDGATE) : Gate(center, type) {
        this->scale = 1.0;
        UpdateGeometry();
    }
    void drawSelf(wxMemoryDC& memDC) override;
    bool Isinside(const wxPoint& point) const override;
    void UpdateGeometry() override;
    std::vector<wxPoint> GetPins() const override;
};

// ---------- NOR（= OR + 气泡） ----------
class NORGate : public Gate {
public:
    NORGate(wxPoint center, ComponentType type = NORGATE) : Gate(center, type) {
        this->scale = 1.0;
        UpdateGeometry();
    }
    void drawSelf(wxMemoryDC& memDC) override;
    bool Isinside(const wxPoint& point) const override;
    void UpdateGeometry() override;
    std::vector<wxPoint> GetPins() const override;
};

// ---------- XOR（= OR 的前缘双曲线） ----------
class XORGate : public Gate {
public:
    XORGate(wxPoint center, ComponentType type = XORGATE) : Gate(center, type) {
        this->scale = 1.0;
        UpdateGeometry();
    }
    void drawSelf(wxMemoryDC& memDC) override;
    bool Isinside(const wxPoint& point) const override;
    void UpdateGeometry() override;
    std::vector<wxPoint> GetPins() const override;
};
// ---------- 普通结点 ----------
class NodeDot : public Component {
public:
    explicit NodeDot(wxPoint center, ComponentType type = NODE_BASIC)
        : Component(center, type) {
        this->scale = 1.0;
        UpdateGeometry();
    }
    void drawSelf(wxMemoryDC& memDC) override;
    bool Isinside(const wxPoint& p) const override;
    void UpdateGeometry() override;
    std::vector<wxPoint> GetPins() const override;
};

// ---------- 起始节点 ----------
class StartNode : public Component {
public:
    explicit StartNode(wxPoint center, ComponentType type = NODE_START)
        : Component(center, type) {
        this->scale = 1.0;
        UpdateGeometry();
    }
    void drawSelf(wxMemoryDC& memDC) override;
    bool Isinside(const wxPoint& p) const override;
    void UpdateGeometry() override;
    std::vector<wxPoint> GetPins() const override;
};

// ---------- 终止节点 ----------
class EndNode : public Component {
public:
    explicit EndNode(wxPoint center, ComponentType type = NODE_END)
        : Component(center, type) {
        this->scale = 1.0;
        UpdateGeometry();
    }
    void drawSelf(wxMemoryDC& memDC) override;
    bool Isinside(const wxPoint& p) const override;
    void UpdateGeometry() override;
    std::vector<wxPoint> GetPins() const override;
};
