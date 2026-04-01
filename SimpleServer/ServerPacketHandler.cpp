#include "ServerPacketHandler.h"
#include "GameRoom.h"
#include "ClientSession.h"
#include "Player.h"
#include "Session.h"
#include "Packet.h"
#include "ProtoUtil.h"
#include "Opcodes.h"

#include <iostream>
#include <algorithm>

bool ServerPacketHandler::HandlePacket(Session& session, const Packet& pkt)
{
    switch (pkt.header.opcode)
    {
    case OP_C_ENTER_GAME:  return HandleEnterGame(session, pkt);
    case OP_C_MOVE:        return HandleMove(session, pkt);
    case OP_C_INPUT_CMD:   return HandleInputCmd(session, pkt);
    case OP_C_CHAT:        return HandleChat(session, pkt);
    case OP_C_PING:        return HandlePing(session, pkt);
    default:               return false;
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

    std::string name = req.name();
    _room->PostTask([room = _room, player, name = std::move(name)]()
    {
        room->EnterGame(player, name);
    });
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

    Vec3  pos{ req.position().x(), req.position().y(), req.position().z() };
    float yaw   = req.yaw();
    float pitch = req.pitch();

    _room->PostTask([room = _room, player, pos, yaw, pitch]()
    {
        room->Move(player, pos, yaw, pitch);
    });
    return true;
}

bool ServerPacketHandler::HandleInputCmd(Session& session, const Packet& pkt)
{
    if (!_room) return false;

    Protocol::C_InputCmd req;
    if (!req.ParseFromArray(pkt.payload.data(), static_cast<int>(pkt.payload.size())))
        return false;

    auto& cs = static_cast<ClientSession&>(session);
    auto player = cs.GetPlayer();
    if (!player) return false;

    // IsEntered()는 atomic read — IOCP 스레드에서 안전
    // 미입장 상태(EnterGame 처리 전)의 InputCmd는 무시만 하고 연결은 유지
    if (!player->IsEntered()) return true;

    InputCmd cmd;
    cmd.tick       = req.tick();
    cmd.moveX      = std::clamp(req.move_x() / 127.f, -1.f, 1.f);
    cmd.moveZ      = std::clamp(req.move_z() / 127.f, -1.f, 1.f);
    cmd.yawDelta   = static_cast<float>(req.yaw_delta())   * 0.01f;
    cmd.pitchDelta = static_cast<float>(req.pitch_delta()) * 0.01f;
    cmd.jump       = req.jump();

    player->EnqueueInput(cmd);
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

    std::string message = req.message();
    std::cout << "[Server] chat received: " << message << "\n";

    _room->PostTask([room = _room, player, message = std::move(message)]()
    {
        room->Chat(player, message);
    });
    return true;
}

bool ServerPacketHandler::HandlePing(Session& session, const Packet&)
{
    Protocol::S_Pong res;
    session.EnqueueSend(MakePacketFromProto(OP_S_PONG, res));
    return true;
}
