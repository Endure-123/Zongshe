#pragma once
#include "UndoRedo.h"
#include "DrawBoard.h"

// —— 快照结构由 DrawBoard 提供 ——

// 添加元件
class AddGateCmd : public ICommand {
public:
    AddGateCmd(DrawBoard* b, const GateSnapshot& snap) : b(b), snap(snap), newId(-1) {}
    void Do() override { newId = b->AddGateFromSnapshot(snap); }
    void Undo() override { if (newId >= 0) b->DeleteGateByIndex(newId); }
private:
    DrawBoard* b; GateSnapshot snap; long newId;
};

// 删除元件
class DeleteGateCmd : public ICommand {
public:
    DeleteGateCmd(DrawBoard* b, long id) : b(b), id(id) {}
    void Do() override { backup = b->ExportGateByIndex(id); b->DeleteGateByIndex(id); }
    void Undo() override { if (backup.has_value()) b->AddGateFromSnapshot(*backup); }
private:
    DrawBoard* b; long id; std::optional<GateSnapshot> backup;
};

// 移动元件
class MoveGateCmd : public ICommand {
public:
    MoveGateCmd(DrawBoard* b, long id, wxPoint from, wxPoint to) : b(b), id(id), oldPos(from), newPos(to) {}
    void Do() override { b->MoveGateTo(id, newPos); }
    void Undo() override { b->MoveGateTo(id, oldPos); }
private:
    DrawBoard* b; long id; wxPoint oldPos, newPos;
};

// 添加/删除连线
class AddWireCmd : public ICommand {
public:
    AddWireCmd(DrawBoard* b, const WireSnapshot& w) : b(b), w(w), newId(-1) {}
    void Do() override { newId = b->AddWire(w); }
    void Undo() override { if (newId >= 0) b->DeleteWireByIndex(newId); }
private:
    DrawBoard* b; WireSnapshot w; long newId;
};

class DeleteWireCmd : public ICommand {
public:
    DeleteWireCmd(DrawBoard* b, long id) : b(b), id(id) {}
    void Do() override { backup = b->ExportWireByIndex(id); b->DeleteWireByIndex(id); }
    void Undo() override { if (backup.has_value()) b->AddWire(*backup); }
private:
    DrawBoard* b; long id; std::optional<WireSnapshot> backup;
};
