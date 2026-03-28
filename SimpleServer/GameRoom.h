#pragma once
#include <mutex>
#include <unordered_map>
#include <memory>
#include <string>
#include "Player.h"
#include "SendBuffer.h"

class GameRoom
{
public:
    void Enter(std::shared_ptr<Player> player);
    void Leave(uint64_t playerId);

    void EnterGame(std::shared_ptr<Player> player, const std::string& name);
    void Move(std::shared_ptr<Player> player, const Vec3& pos, const Vec3& dir);
    void Chat(std::shared_ptr<Player> player, const std::string& message);

private:
    void Send(Player* player, const SendBufferRef& buf);
    void Broadcast(const SendBufferRef& buf, Player* except = nullptr);

    std::mutex _lock;
    std::unordered_map<uint64_t, std::shared_ptr<Player>> _players;
};
