#pragma once
#include <wx/wx.h>

// 当画布（DrawBoard）中的“选中对象”变化时发出的事件。
// evt.GetInt()     -> SelKind（见 DrawBoard.h 内定义的枚举转 int）
// evt.GetExtraLong()-> 选中对象的索引（或 id）
wxDECLARE_EVENT(EVT_SELECTION_CHANGED, wxCommandEvent);