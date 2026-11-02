#include <iostream>
#include <fstream>
#include <string>
#include <unordered_map>
#include "app/src/main/cpp/nlohmann/json.hpp"

using json = nlohmann::json;

std::string readAsset(const char* path) {
    std::ifstream f(path);
    if (!f.is_open()) return "";
    return std::string((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
}

int main() {
    std::unordered_map<std::string, int> atlasMap;
    std::string atlasStr = readAsset("app/src/main/assets/atlas_map.json");
    if (!atlasStr.empty()) {
        try {
            auto j = json::parse(atlasStr);
            for (auto& el : j.items()) {
                atlasMap[el.key()] = el.value().get<int>();
            }
        } catch (...) {}
    }
    
    auto getTex = [&](const std::string& name) {
        if (atlasMap.find(name) != atlasMap.end()) return atlasMap[name];
        return 0;
    };
    
    std::unordered_map<std::string, uint16_t> preset_ids = {
        {"grass_block", 1}, {"dirt", 2}, {"stone", 3}, {"oak_log", 4},
        {"oak_leaves", 5}, {"water", 6}, {"oak_planks", 7}, {"sand", 8},
        {"glass", 9}, {"bricks", 10}, {"gravel", 11}, {"coal_ore", 12},
        {"iron_ore", 13}, {"diamond_ore", 14}, {"cobblestone", 15}
    };
    
    std::string blocksStr = readAsset("app/src/main/assets/blocks.json");
    if (!blocksStr.empty()) {
        try {
            auto j = json::parse(blocksStr);
            uint16_t fc_id = 16;
            for (auto& el : j.items()) {
                std::string mc_id = el.key();
                if (mc_id.find("minecraft:") != 0) continue;
                std::string identifier = mc_id.substr(10);
                
                uint16_t block_id = fc_id;
                if (preset_ids.find(identifier) != preset_ids.end()) {
                    block_id = preset_ids[identifier];
                } else {
                    fc_id++;
                }
                
                int texTop = getTex(identifier + "_top");
                int texBottom = getTex(identifier + "_bottom");
                int texSide = getTex(identifier + "_side");
                int texAll = getTex(identifier);
                
                if (block_id <= 15) {
                    std::cout << "ID " << block_id << " (" << identifier << "): all=" << texAll 
                              << " top=" << texTop << " bot=" << texBottom << " side=" << texSide << std::endl;
                }
            }
        } catch (...) {}
    }
    
    return 0;
}
