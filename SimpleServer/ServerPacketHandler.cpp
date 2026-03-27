#include "ServerPacketHandler.h"
#include "ClientSession.h"
#include "Player.h"
#include "Session.h"
#include "Packet.h"
#include "ProtoUtil.h"
#include "NetService.h"
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
    if (!_service) return false;

    Protocol::C_EnterGame req;
    if (!req.ParseFromArray(pkt.payload.data(), static_cast<int>(pkt.payload.size())))
        return false;

    auto& cs = static_cast<ClientSession&>(session);
    auto player = cs.GetPlayer();
    if (!player)
        return false;

    const std::string& name = req.name();
    if (name.empty() || name.size() > 20)
        return false;
    player->SetName(name);

    // S_EnterGame — 입장 결과를 나에게 전송
    {
        Protocol::S_EnterGame res;
        res.set_success(true);
        auto* info = res.mutable_my_info();
        info->set_player_id(player->GetPlayerId());
        info->set_name(player->GetName());
        session.EnqueueSend(MakePacketFromProto(OP_S_ENTER_GAME, res));
    }

    // S_PlayerList — 현재 접속 중인 플레이어 목록을 나에게 전송
    {
        Protocol::S_PlayerList playerList;
        _service->ForEachSession([&](Session& other)
        {
            if (&other == &session) return;
            auto& otherCs = static_cast<ClientSession&>(other);
            auto otherPlayer = otherCs.GetPlayer();
            if (!otherPlayer) return;

            auto* info = playerList.add_players();
            info->set_player_id(otherPlayer->GetPlayerId());
            info->set_name(otherPlayer->GetName());
            info->mutable_position()->set_x(otherPlayer->GetPosition().x);
            info->mutable_position()->set_y(otherPlayer->GetPosition().y);
            info->mutable_position()->set_z(otherPlayer->GetPosition().z);
            info->mutable_direction()->set_x(otherPlayer->GetDirection().x);
            info->mutable_direction()->set_y(otherPlayer->GetDirection().y);
            info->mutable_direction()->set_z(otherPlayer->GetDirection().z);
        });
        session.EnqueueSend(MakePacketFromProto(OP_S_PLAYER_LIST, playerList));
    }

    // S_Spawn — 나의 등장을 기존 플레이어들에게 전파
    {
        Protocol::S_Spawn spawn;
        auto* info = spawn.add_players();
        info->set_player_id(player->GetPlayerId());
        info->set_name(player->GetName());

        auto sendBuf = MakeSendBuffer(MakePacketFromProto(OP_S_SPAWN, spawn));
        _service->ForEachSession([&](Session& other)
        {
            if (&other != &session)
                other.EnqueueSend(sendBuf);
        });
    }

    std::cout << "[Server] Player entered: " << req.name() << "\n";
    return true;
}

bool ServerPacketHandler::HandleMove(Session& session, const Packet& pkt)
{
    if (!_service) return false;

    Protocol::C_Move req;
    if (!req.ParseFromArray(pkt.payload.data(), static_cast<int>(pkt.payload.size())))
        return false;

    auto& cs = static_cast<ClientSession&>(session);
    auto player = cs.GetPlayer();
    if (!player) return false;

    Vec3 pos{ req.position().x(), req.position().y(), req.position().z() };
    Vec3 dir{ req.direction().x(), req.direction().y(), req.direction().z() };
    player->SetPosition(pos);
    player->SetDirection(dir);

    Protocol::S_Move res;
    res.set_player_id(player->GetPlayerId());
    res.mutable_position()->set_x(pos.x);
    res.mutable_position()->set_y(pos.y);
    res.mutable_position()->set_z(pos.z);
    res.mutable_direction()->set_x(dir.x);
    res.mutable_direction()->set_y(dir.y);
    res.mutable_direction()->set_z(dir.z);

    _service->Broadcast(MakePacketFromProto(OP_S_MOVE, res));
    return true;
}

bool ServerPacketHandler::HandleChat(Session& session, const Packet& pkt)
{
    if (!_service) return false;

    Protocol::C_Chat req;
    if (!req.ParseFromArray(pkt.payload.data(), static_cast<int>(pkt.payload.size())))
        return false;

    auto& cs = static_cast<ClientSession&>(session);
    auto player = cs.GetPlayer();
    if (!player) return false;

    std::cout << "[Server] chat received: " << req.message() << "\n";

    Protocol::S_Chat res;
    res.set_player_id(player->GetPlayerId());
    res.set_name(player->GetName());
    res.set_message(req.message());

    _service->Broadcast(MakePacketFromProto(OP_S_CHAT, res));
    return true;
}

bool ServerPacketHandler::HandlePing(Session& session, const Packet&)
{
    Protocol::S_Pong res;
    session.EnqueueSend(MakePacketFromProto(OP_S_PONG, res));
    return true;
}
