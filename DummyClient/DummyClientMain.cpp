#include <WinSock2.h>
#include <WS2tcpip.h>
#include <windows.h>

#include <iostream>
#include <thread>
#include <vector>
#include <chrono>
#include <memory>
#include <string>

#include "NetService.h"
#include "ClientPacketHandler.h"
#include "Packet.h"
#include "Opcodes.h"
#include "Session.h"

#pragma comment(lib, "ws2_32.lib")

struct DummyBot
{
    int id = 0;
    ClientPacketHandler handler;
    NetService service;

    explicit DummyBot(int clientId)
        : id(clientId), handler(clientId), service(handler)
    {
    }
};

void RunDummyBot(int id, const char* ip, uint16_t port)
{
    DummyBot bot(id);

    bot.service.SetOnConnected([&bot](Session& session)
        {
            std::cout << "[Dummy " << bot.id << "] connected\n";
            session.EnqueueSend(MakePacket(OP_PING));
        });

    bot.service.SetOnDisconnected([&bot](Session&)
        {
            std::cout << "[Dummy " << bot.id << "] disconnected\n";
        });

    if (!bot.service.StartClient(1))
    {
        std::cout << "[Dummy " << bot.id << "] StartClient failed\n";
        return;
    }

    if (!bot.service.Connect(ip, port))
    {
        std::cout << "[Dummy " << bot.id << "] Connect failed\n";
        bot.service.Stop();
        return;
    }

    int tick = 0;
    while (true)
    {
        std::string msg = "Hello from dummy #" + std::to_string(bot.id) +
            " tick=" + std::to_string(tick++);

        bot.service.Broadcast(MakePacket(
            OP_CHAT,
            msg.data(),
            static_cast<uint16_t>(msg.size())
        ));

        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
}

int main()
{
    WSADATA wsa{};
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0)
    {
        std::cout << "WSAStartup failed\n";
        return 1;
    }

    constexpr int kDummyCount = 10;
    std::vector<std::thread> bots;
    bots.reserve(kDummyCount);

    for (int i = 0; i < kDummyCount; ++i)
    {
        bots.emplace_back([i]()
            {
                RunDummyBot(i, "127.0.0.1", 7777);
            });

        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }

    for (auto& t : bots)
        t.join();

    WSACleanup();
    return 0;
}