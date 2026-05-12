#include "ServerPacketHandler.h"
#include "GameRoom.h"
#include "ClientSession.h"
#include "Player.h"
#include "Session.h"
#include "Packet.h"
#include "ProtoUtil.h"
#include "Opcodes.h"

#include <iostream>
#include <chrono>

bool ServerPacketHandler::HandlePacket(Session& session, const Packet& pkt)
{
    switch (pkt.header.opcode)
    {
    case OP_C_ENTER_GAME:   return HandleEnterGame(session, pkt);
    case OP_C_INPUT_CMD:    return HandleInputCmd(session, pkt);
    case OP_C_CHAT:         return HandleChat(session, pkt);
    case OP_C_ATTACK:       return HandleAttack(session, pkt);
    case OP_C_RESURRECT:    return HandleResurrect(session, pkt);
    case OP_C_USE_ITEM:     return HandleUseItem(session, pkt);
    case OP_C_ENTER_ZONE:   return HandleEnterZone(session, pkt);
    case OP_C_LADDER_STATE: return HandleLadderState(session, pkt);
    case OP_C_PING:         return HandlePing(session, pkt);
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

bool ServerPacketHandler::HandleInputCmd(Session& session, const Packet& pkt)
{
    if (!_room) return false;

    Protocol::C_InputCmd req;
    if (!req.ParseFromArray(pkt.payload.data(), static_cast<int>(pkt.payload.size())))
        return false;

    auto& cs = static_cast<ClientSession&>(session);
    auto player = cs.GetPlayer();
    if (!player) return false;

    // 미입장 상태의 InputCmd는 무시 (연결은 유지)
    if (!player->IsEntered()) return true;

    InputCmd cmd;
    cmd.inputSeq    = req.input_seq();
    cmd.deltaTime   = req.delta_time();
    cmd.moveLeft    = req.move_left();
    cmd.moveRight   = req.move_right();
    cmd.jump        = req.jump();
    cmd.dropThrough = req.attack();   // proto attack 필드를 drop-through 신호로 재사용
    cmd.faceDir     = static_cast<MoveDir>(req.face_dir());

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
    int32_t type = static_cast<int32_t>(req.type());
    std::cout << "[Server] chat received: " << message << "\n";

    _room->PostTask([room = _room, player, message = std::move(message), type]()
    {
        room->Chat(player, message, type);
    });
    return true;
}

bool ServerPacketHandler::HandleAttack(Session& session, const Packet& pkt)
{
    if (!_room) return false;

    Protocol::C_Attack req;
    if (!req.ParseFromArray(pkt.payload.data(), static_cast<int>(pkt.payload.size())))
        return false;

    auto& cs = static_cast<ClientSession&>(session);
    auto player = cs.GetPlayer();
    if (!player || !player->IsEntered()) return true;

    // TODO: 전투 판정 구현
    return true;
}

bool ServerPacketHandler::HandleResurrect(Session& session, const Packet& pkt)
{
    if (!_room) return false;

    auto& cs = static_cast<ClientSession&>(session);
    auto player = cs.GetPlayer();
    if (!player || !player->IsEntered()) return true;

    // TODO: 부활 처리 구현
    return true;
}

bool ServerPacketHandler::HandleUseItem(Session& session, const Packet& pkt)
{
    if (!_room) return false;

    Protocol::C_UseItem req;
    if (!req.ParseFromArray(pkt.payload.data(), static_cast<int>(pkt.payload.size())))
        return false;

    auto& cs = static_cast<ClientSession&>(session);
    auto player = cs.GetPlayer();
    if (!player || !player->IsEntered()) return true;

    // TODO: 아이템 사용 처리 구현
    return true;
}

bool ServerPacketHandler::HandleEnterZone(Session& session, const Packet& pkt)
{
    if (!_room) return false;

    Protocol::C_EnterZone req;
    if (!req.ParseFromArray(pkt.payload.data(), static_cast<int>(pkt.payload.size())))
        return false;

    auto& cs = static_cast<ClientSession&>(session);
    auto player = cs.GetPlayer();
    if (!player || !player->IsEntered()) return true;

    // TODO: 존 이동 처리 구현
    return true;
}

bool ServerPacketHandler::HandleLadderState(Session& session, const Packet& pkt)
{
    if (!_room) return false;

    Protocol::C_LadderState req;
    if (!req.ParseFromArray(pkt.payload.data(), static_cast<int>(pkt.payload.size())))
        return false;

    auto& cs = static_cast<ClientSession&>(session);
    auto player = cs.GetPlayer();
    if (!player || !player->IsEntered()) return true;

    bool  onLadder = req.on_ladder();
    float px = req.pos_x(), py = req.pos_y();
    float vx = req.vel_x(), vy = req.vel_y();

    _room->PostTask([room = _room, player, onLadder, px, py, vx, vy]()
    {
        room->HandleLadderState(player, onLadder, px, py, vx, vy);
    });
    return true;
}

bool ServerPacketHandler::HandlePing(Session& session, const Packet& pkt)
{
    Protocol::C_Ping req;
    if (!req.ParseFromArray(pkt.payload.data(), static_cast<int>(pkt.payload.size())))
        return false;

    using namespace std::chrono;
    Protocol::S_Pong res;
    res.set_client_time(req.client_time());
    res.set_server_time(static_cast<uint64_t>(
        duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count()));

    session.EnqueueSend(MakePacketFromProto(OP_S_PONG, res));
    return true;
}
