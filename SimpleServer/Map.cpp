#define NOMINMAX
#include "Map.h"
#include <fstream>
#include <iostream>
#include <nlohmann/json.hpp>

bool Map::Load(const std::string& jsonPath)
{
    std::ifstream file(jsonPath);
    if (!file.is_open())
    {
        std::cout << "[Map] Failed to open: " << jsonPath << "\n";
        return false;
    }

    nlohmann::json j;
    try { file >> j; }
    catch (const nlohmann::json::exception& e)
    {
        std::cout << "[Map] JSON parse error: " << e.what() << "\n";
        return false;
    }

    if (!j.contains("layers") || !j["layers"].is_array())
    {
        std::cout << "[Map] JSON has no 'layers' array\n";
        return false;
    }

    _solid.clear();
    _ladder.clear();
    _floatingGround.clear();

    for (const auto& layer : j["layers"])
    {
        std::string name = layer.value("name", "");

        // cellSize는 레이어마다 동일하다고 가정, 첫 레이어에서 읽음
        if (_cellSizeX == 1.f && _cellSizeY == 1.f)
        {
            _cellSizeX = layer.value("cellSizeX", 1.f);
            _cellSizeY = layer.value("cellSizeY", 1.f);
        }

        bool isGround   = (name == "Ground");
        bool isLadder   = (name == "Ladder");
        bool isFloating = (name == "Floating Ground");
        if (!isGround && !isLadder && !isFloating)
            continue;

        auto& target = isGround ? _solid : (isLadder ? _ladder : _floatingGround);

        if (!layer.contains("tiles") || !layer["tiles"].is_array())
            continue;

        for (const auto& tile : layer["tiles"])
        {
            int cx = tile.value("x", 0);
            int cy = tile.value("y", 0);
            target.insert(PackCell(cx, cy));
        }
    }

    _loaded = true;
    std::cout << "[Map] Loaded: " << _solid.size() << " solid tiles, "
              << _ladder.size() << " ladder tiles, "
              << _floatingGround.size() << " floating ground tiles"
              << " (cellSize=" << _cellSizeX << "x" << _cellSizeY << ")\n";
    return true;
}
