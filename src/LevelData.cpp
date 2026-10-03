#include "LevelData.hpp"

#include <Geode/Geode.hpp>

#include <filesystem>
#include <fstream>
#include <sstream>

using namespace geode::prelude;

namespace
{
    std::vector<LevelEntry> g_levels;
    bool g_loaded = false;

    bool readTextFile(std::filesystem::path const &path, std::string &out)
    {
        std::ifstream file(path, std::ios::binary);
        if (!file.is_open())
            return false;

        std::ostringstream buffer;
        buffer << file.rdbuf();
        out = buffer.str();

        return true;
    }

    bool tryLoad(std::filesystem::path const &path)
    {
        std::string text;
        if (!readTextFile(path, text))
            return false;

        auto levels = LevelData::parseCsv(text);
        if (levels.empty())
            return false;

        g_levels = std::move(levels);
        g_loaded = true;

        return true;
    }

    void loadFromDisk()
    {
        if (tryLoad(Mod::get()->getSaveDir() / "levels.csv"))
            return;

        auto resourcesDir = Mod::get()->getResourcesDir();

        if (tryLoad(resourcesDir / "levels.csv"))
            return;

        log::error(
            "Could not load levels.csv (looked in the save folder and in {})",
            resourcesDir.string());
    }

    void saveToDisk()
    {
        auto path = Mod::get()->getSaveDir() / "levels.csv";
        std::ofstream file(path, std::ios::binary | std::ios::trunc);

        if (file.is_open())
            file << LevelData::toCsv(g_levels);
    }
}

namespace LevelData
{

    std::vector<LevelEntry> &all()
    {
        if (!g_loaded)
            loadFromDisk();

        return g_levels;
    }

    void replaceAll(std::vector<LevelEntry> levels)
    {
        g_levels = std::move(levels);
        g_loaded = true;

        saveToDisk();
    }

    bool contains(int levelId)
    {
        for (auto const &level : all())
        {
            if (level.id == levelId)
                return true;
        }

        return false;
    }

    std::vector<LevelEntry> parseCsv(std::string const &text)
    {
        std::vector<LevelEntry> levels;

        std::istringstream lines(text);
        std::string line;

        while (std::getline(lines, line))
        {
            if (line.find("id,stars") != std::string::npos)
                continue;

            for (auto &ch : line)
            {
                if (ch == ',')
                    ch = ' ';
            }

            std::istringstream fields(line);
            LevelEntry level;

            if (fields >> level.id >> level.stars >> level.length >> level.coins)
                levels.push_back(level);
        }

        return levels;
    }

    std::string toCsv(std::vector<LevelEntry> const &levels)
    {
        std::string csv = "id,stars,length,coins\n";
        csv.reserve(levels.size() * 16 + 32);

        for (auto const &level : levels)
        {
            csv += std::to_string(level.id) + ",";
            csv += std::to_string(level.stars) + ",";
            csv += std::to_string(level.length) + ",";
            csv += std::to_string(level.coins) + "\n";
        }

        return csv;
    }

}