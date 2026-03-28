#pragma once
#include "IPacketHandler.h"

class GameRoom;

class ServerPacketHandler : public IPacketHandler
{
public:
    void SetRoom(GameRoom* room) { _room = room; }
    bool HandlePacket(Session& session, const Packet& pkt) override;

private:
    bool HandleEnterGame(Session& session, const Packet& pkt);
    bool HandleMove(Session& session, const Packet& pkt);
    bool HandleChat(Session& session, const Packet& pkt);
    bool HandlePing(Session& session, const Packet& pkt);

private:
    GameRoom* _room = nullptr;
};
