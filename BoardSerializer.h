#pragma once

#include "BoardModel.h"

#include <string>

class BoardSerializer {
public:
    static void SaveToFile(const BoardModel &board, const std::string &path);
    static void LoadFromFile(const std::string &path, BoardModel &board);
};
