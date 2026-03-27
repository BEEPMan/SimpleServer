#pragma once
#include <cstdint>
#include <string>

struct Vec3
{
    float x = 0.f;
    float y = 0.f;
    float z = 0.f;
};

class Player
{
public:
    Player(uint64_t id, std::string name);

    uint64_t           GetPlayerId()  const { return _playerId; }
    const std::string& GetName()      const { return _name; }
    const Vec3&        GetPosition()  const { return _position; }
    const Vec3&        GetDirection() const { return _direction; }

    void SetName(const std::string& name) { _name = name; }
    void SetPosition(const Vec3& pos)     { _position = pos; }
    void SetDirection(const Vec3& dir)    { _direction = dir; }

private:
    uint64_t    _playerId;
    std::string _name;
    Vec3        _position;
    Vec3        _direction;
};
