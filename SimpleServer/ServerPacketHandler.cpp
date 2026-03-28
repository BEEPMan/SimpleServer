#include "ServerPacketHandler.h"
#include "GameRoom.h"
#include "ClientSession.h"
#include "Player.h"
#include "Session.h"
#include "Packet.h"
#include "ProtoUtil.h"
#include "Opcodes.h"

#include <iostream>

bool ServerPacketHandler::HandlePacket(Session& session, const Packet& pkt)
{
    switch (pkt.header.opcode)
    {
    case OP_C_ENTER_GAME: return HandleEnterGame(session, pkt);
    case OP_C_MOVE:       return HandleMove(session, pkt);
    case OP_C_CHAT:       return HandleChat(session, pkt);
    case OP_C_PING:       return HandlePing(session, pkt);
    default:              return false;
    }
}

bool ServerPacketHandler::HandleEnterGame(Session& session, const Packet& pkt)
{
    if (!_room) return false;

    Protocol::C_EnterGame req;
    if (!req.ParseFromArray(pkt.payload.data(), static_cast<int>(pkt.payload.size())))
        return false;

    auto& cs = static_cast<ClientSession&>(session);
    auto player = cs.GetPlayer();
    if (!player) return false;

    _room->EnterGame(player, req.name());
    return true;
}

bool ServerPacketHandler::HandleMove(Session& session, const Packet& pkt)
{
    if (!_room) return false;

    Protocol::C_Move req;
    if (!req.ParseFromArray(pkt.payload.data(), static_cast<int>(pkt.payload.size())))
        return false;

    auto& cs = static_cast<ClientSession&>(session);
    auto player = cs.GetPlayer();
    if (!player) return false;

    Vec3 pos{ req.position().x(), req.position().y(), req.position().z() };
    Vec3 dir{ req.direction().x(), req.direction().y(), req.direction().z() };
    _room->Move(player, pos, dir);
    return true;
}

bool ServerPacketHandler::HandleChat(Session& session, const Packet& pkt)
{
    if (!_room) return false;

    Protocol::C_Chat req;
    if (!req.ParseFromArray(pkt.payload.data(), static_cast<int>(pkt.payload.size())))
        return false;

    auto& cs = static_cast<ClientSession&>(session);
    auto player = cs.GetPlayer();
    if (!player) return false;

    std::cout << "[Server] chat received: " << req.message() << "\n";
    _room->Chat(player, req.message());
    return true;
}

bool ServerPacketHandler::HandlePing(Session& session, const Packet&)
{
    Protocol::S_Pong res;
    session.EnqueueSend(MakePacketFromProto(OP_S_PONG, res));
    return true;
}
