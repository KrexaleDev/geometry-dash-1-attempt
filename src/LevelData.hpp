#pragma once

#include <string>
#include <vector>

struct LevelEntry
{
    int id = 0;
    int stars = 0;
    int length = 0; // 0 = unknown, 1 = Short, 2 = Medium, 3 = Long, 4 = XL
    int coins = 0;
};

namespace LevelData
{

    std::vector<LevelEntry> &all();

    void replaceAll(std::vector<LevelEntry> levels);

    bool contains(int levelId);

    std::vector<LevelEntry> parseCsv(std::string const &text);
    std::string toCsv(std::vector<LevelEntry> const &levels);

}