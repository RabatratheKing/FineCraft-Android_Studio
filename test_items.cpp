#include <iostream>
#include <fstream>
#include <string>
#include "app/src/main/cpp/nlohmann/json.hpp"

using json = nlohmann::json;

int main() {
    std::ifstream f("app/src/main/assets/items.json");
    if (!f.is_open()) return 1;
    std::string itemsStr((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
    
    auto j = json::parse(itemsStr);
    int valid = 0;
    for (auto& el : j.items()) {
        std::string mc_id = el.key();
        if (mc_id.find("minecraft:") == 0) valid++;
    }
    std::cout << "Parsed items array size: " << j.size() << std::endl;
    std::cout << "Valid keys starting with minecraft: " << valid << std::endl;
    return 0;
}
