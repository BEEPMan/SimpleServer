#pragma once

struct Packet;
class Session;

class IPacketHandler
{
public:
    virtual ~IPacketHandler() = default;

    virtual bool HandlePacket(Session& session, const Packet& pkt) = 0;
};