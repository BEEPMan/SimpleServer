#pragma once
#include <cstdint>

enum : uint16_t
{
    OP_C_ENTER_GAME  = 1,
    OP_S_ENTER_GAME  = 2,
    OP_S_SPAWN       = 3,
    OP_S_DESPAWN     = 4,
    OP_C_MOVE        = 5,
    OP_S_MOVE        = 6,
    OP_C_CHAT        = 7,
    OP_S_CHAT        = 8,
    OP_S_PLAYER_LIST = 9,
    OP_C_PING        = 10,
    OP_S_PONG        = 11,
    OP_C_INPUT_CMD   = 12,
    OP_S_PLAYER_STATE = 13,
};
