#pragma once
#include <cstdint>
#include <functional>
#include <unordered_map>

struct Packet;
class Session;

class PacketDispatcher
{
public:
    using Handler = std::function<void(Session&, const Packet&)>;

    void Register(uint16_t opcode, Handler handler);
    bool Dispatch(Session& session, const Packet& pkt) const;

private:
    std::unordered_map<uint16_t, Handler> _handlers;
};