#include "GameRoom.h"
#include "Session.h"
#include "ProtoUtil.h"
#include "Packet.h"
#include "Opcodes.h"
#include "game.pb.h"

#include <iostream>
#include <vector>

void GameRoom::Enter(std::shared_ptr<Player> player)
{
    std::lock_guard<std::mutex> lock(_lock);
    _players[player->GetPlayerId()] = std::move(player);
}

void GameRoom::Leave(uint64_t playerId)
{
    std::lock_guard<std::mutex> lock(_lock);
    _players.erase(playerId);
}

void GameRoom::EnterGame(std::shared_ptr<Player> player, const std::string& name)
{
    if (name.empty() || name.size() > 20)
        return;
    if (!player->GetName().empty())
        return;

    std::vector<std::shared_ptr<Player>> others;
    {
        std::lock_guard<std::mutex> lock(_lock);
        player->SetName(name);
        for (auto& [id, p] : _players)
        {
            if (p.get() != player.get())
                others.push_back(p);
        }
    }

    // S_EnterGame — 입장 결과를 나에게 전송
    {
        Protocol::S_EnterGame res;
        res.set_success(true);
        auto* info = res.mutable_my_info();
        info->set_player_id(player->GetPlayerId());
        info->set_name(player->GetName());
        Send(player.get(), MakeSendBuffer(MakePacketFromProto(OP_S_ENTER_GAME, res)));
    }

    // S_PlayerList — 현재 접속 중인 플레이어 목록을 나에게 전송
    {
        Protocol::S_PlayerList playerList;
        for (const auto& p : others)
        {
            auto* info = playerList.add_players();
            info->set_player_id(p->GetPlayerId());
            info->set_name(p->GetName());
            info->mutable_position()->set_x(p->GetPosition().x);
            info->mutable_position()->set_y(p->GetPosition().y);
            info->mutable_position()->set_z(p->GetPosition().z);
            info->mutable_direction()->set_x(p->GetDirection().x);
            info->mutable_direction()->set_y(p->GetDirection().y);
            info->mutable_direction()->set_z(p->GetDirection().z);
        }
        Send(player.get(), MakeSendBuffer(MakePacketFromProto(OP_S_PLAYER_LIST, playerList)));
    }

    // S_Spawn — 나의 등장을 기존 플레이어들에게 전파
    {
        Protocol::S_Spawn spawn;
        auto* info = spawn.add_players();
        info->set_player_id(player->GetPlayerId());
        info->set_name(player->GetName());
        auto buf = MakeSendBuffer(MakePacketFromProto(OP_S_SPAWN, spawn));
        for (const auto& p : others)
            Send(p.get(), buf);
    }

    std::cout << "[Server] Player entered: " << name << "\n";
}

void GameRoom::Move(std::shared_ptr<Player> player, const Vec3& pos, const Vec3& dir)
{
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

    Broadcast(MakeSendBuffer(MakePacketFromProto(OP_S_MOVE, res)));
}

void GameRoom::Chat(std::shared_ptr<Player> player, const std::string& message)
{
    Protocol::S_Chat res;
    res.set_player_id(player->GetPlayerId());
    res.set_name(player->GetName());
    res.set_message(message);

    Broadcast(MakeSendBuffer(MakePacketFromProto(OP_S_CHAT, res)));
}

void GameRoom::Send(Player* player, const SendBufferRef& buf)
{
    if (Session* s = player->GetSession())
        s->EnqueueSend(buf);
}

void GameRoom::Broadcast(const SendBufferRef& buf, Player* except)
{
    std::vector<Session*> sessions;
    {
        std::lock_guard<std::mutex> lock(_lock);
        sessions.reserve(_players.size());
        for (auto& [id, player] : _players)
        {
            if (player.get() == except) continue;
            if (Session* s = player->GetSession())
                sessions.push_back(s);
        }
    }
    for (Session* s : sessions)
        s->EnqueueSend(buf);
}
