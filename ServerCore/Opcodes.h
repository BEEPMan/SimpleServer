#pragma once
#include <cstdint>

enum : uint16_t
{
    // Client → Server
    OP_C_ENTER_GAME   = 1000,
    OP_C_INPUT_CMD    = 1001,
    OP_C_ATTACK       = 1002,
    OP_C_CHAT         = 1003,
    OP_C_USE_ITEM     = 1004,
    OP_C_ENTER_ZONE   = 1005,
    OP_C_RESURRECT    = 1006,
    OP_C_PING         = 1007,
    OP_C_LADDER_STATE = 1008,

    // Server → Client
    OP_S_ENTER_GAME        = 2000,
    OP_S_SPAWN             = 2001,
    OP_S_DESPAWN           = 2002,
    OP_S_PLAYER_LIST       = 2003,
    OP_S_PLAYER_STATE      = 2004,
    OP_S_BROADCAST_MOVE    = 2005,
    OP_S_ATTACK_RESULT     = 2006,
    OP_S_TAKE_DAMAGE       = 2007,
    OP_S_DIE               = 2008,
    OP_S_RESURRECT         = 2009,
    OP_S_CHAT              = 2010,
    OP_S_INVENTORY_UPDATE  = 2011,
    OP_S_ENTER_ZONE        = 2012,
    OP_S_PONG              = 2013,
};
