#include "Player.h"

Player::Player(uint64_t id, std::string name)
    : _playerId(id), _name(std::move(name))
{
}
