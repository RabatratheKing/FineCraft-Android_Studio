#pragma once
#include <string>

namespace SaveManager {
    void Init(const std::string& path);
    bool HasSave();
    void SaveGame();
    bool LoadGame();
    void NewGame();
}
