#include "ClientPacketHandler.h"
#include "Session.h"
#include "Packet.h"
#include "ProtoUtil.h"
#include "Opcodes.h"

#include <iostream>

ClientPacketHandler::ClientPacketHandler(int clientId)
    : _clientId(clientId)
{
}

bool ClientPacketHandler::HandlePacket(Session& session, const Packet& pkt)
{
    switch (pkt.header.opcode)
    {
    case OP_S_ENTER_GAME:  return HandleEnterGame(session, pkt);
    case OP_S_SPAWN:       return HandleSpawn(session, pkt);
    case OP_S_DESPAWN:     return HandleDespawn(session, pkt);
    case OP_S_CHAT:        return HandleChat(session, pkt);
    case OP_S_PLAYER_LIST: return HandlePlayerList(session, pkt);
    case OP_S_PONG:        return HandlePong(session, pkt);
    default:               return false;
    }
}

bool ClientPacketHandler::HandleEnterGame(Session&, const Packet& pkt)
{
    Protocol::S_EnterGame res;
    if (!res.ParseFromArray(pkt.payload.data(), static_cast<int>(pkt.payload.size())))
        return false;

    if (res.success())
        std::cout << "[Dummy " << _clientId << "] entered game. id=" << res.my_info().player_id() << "\n";
    else
        std::cout << "[Dummy " << _clientId << "] enter game failed\n";

    return true;
}

bool ClientPacketHandler::HandleSpawn(Session&, const Packet& pkt)
{
    Protocol::S_Spawn res;
    if (!res.ParseFromArray(pkt.payload.data(), static_cast<int>(pkt.payload.size())))
        return false;

    for (const auto& p : res.players())
        std::cout << "[Dummy " << _clientId << "] spawn: " << p.name() << " (id=" << p.player_id() << ")\n";

    return true;
}

bool ClientPacketHandler::HandleDespawn(Session&, const Packet& pkt)
{
    Protocol::S_Despawn res;
    if (!res.ParseFromArray(pkt.payload.data(), static_cast<int>(pkt.payload.size())))
        return false;

    for (uint64_t id : res.player_ids())
        std::cout << "[Dummy " << _clientId << "] despawn: id=" << id << "\n";

    return true;
}

bool ClientPacketHandler::HandleChat(Session&, const Packet& pkt)
{
    Protocol::S_Chat res;
    if (!res.ParseFromArray(pkt.payload.data(), static_cast<int>(pkt.payload.size())))
        return false;

    std::cout << "[Dummy " << _clientId << "] chat: " << res.name() << ": " << res.message() << "\n";
    return true;
}

bool ClientPacketHandler::HandlePlayerList(Session&, const Packet& pkt)
{
    Protocol::S_PlayerList res;
    if (!res.ParseFromArray(pkt.payload.data(), static_cast<int>(pkt.payload.size())))
        return false;

    std::cout << "[Dummy " << _clientId << "] player list (" << res.players_size() << " players)\n";
    return true;
}

bool ClientPacketHandler::HandlePong(Session&, const Packet&)
{
    std::cout << "[Dummy " << _clientId << "] PONG received\n";
    return true;
}
