#pragma once
#include <string>
#include <unordered_set>
#include <cstdint>
#include <cmath>

// 타일 셀 좌표 (cx, cy) → int64 키
inline int64_t PackCell(int cx, int cy)
{
    return (static_cast<int64_t>(cy) << 32) | static_cast<uint32_t>(cx);
}

// TilemapExporter(.json) 로 내보낸 타일맵 데이터
// Ground 레이어: 고체 타일 (충돌 판정)
// Ladder 레이어: 사다리 타일 (서버 충돌 제외, 추후 사다리 로직 확장 가능)
class Map
{
public:
    bool Load(const std::string& jsonPath);

    bool IsLoaded()  const { return _loaded; }
    float CellSizeX() const { return _cellSizeX; }
    float CellSizeY() const { return _cellSizeY; }

    // 월드 좌표 → 셀 좌표 (floor 기반)
    static int WorldToCell(float world, float cellSize)
    {
        return static_cast<int>(std::floor(world / cellSize));
    }

    // 셀 좌표 → 타일 상단 월드 Y
    float CellTopY(int cy)    const { return (cy + 1) * _cellSizeY; }
    float CellBottomY(int cy) const { return cy       * _cellSizeY; }
    float CellRightX(int cx)  const { return (cx + 1) * _cellSizeX; }
    float CellLeftX(int cx)   const { return cx       * _cellSizeX; }

    bool IsSolid(int cx, int cy)          const { return _solid.count(PackCell(cx, cy))          != 0; }
    bool IsLadder(int cx, int cy)         const { return _ladder.count(PackCell(cx, cy))         != 0; }
    bool IsFloatingGround(int cx, int cy) const { return _floatingGround.count(PackCell(cx, cy)) != 0; }

    int SolidCount()          const { return static_cast<int>(_solid.size());          }
    int LadderCount()         const { return static_cast<int>(_ladder.size());         }
    int FloatingGroundCount() const { return static_cast<int>(_floatingGround.size()); }

private:
    std::unordered_set<int64_t> _solid;           // Ground 레이어 타일
    std::unordered_set<int64_t> _ladder;          // Ladder 레이어 타일
    std::unordered_set<int64_t> _floatingGround;  // Floating Ground 레이어 타일 (위에서만 충돌)

    float _cellSizeX = 1.f;
    float _cellSizeY = 1.f;
    bool  _loaded    = false;
};
