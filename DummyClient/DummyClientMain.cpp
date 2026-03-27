#include <WinSock2.h>

#include <iostream>
#include <thread>
#include <vector>
#include <chrono>
#include <string>

#include "NetService.h"
#include "ClientPacketHandler.h"
#include "Packet.h"
#include "ProtoUtil.h"
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

            Protocol::C_EnterGame req;
            req.set_name("더미봇_" + std::to_string(bot.id));
            session.EnqueueSend(MakePacketFromProto(OP_C_ENTER_GAME, req));
        });

    bot.service.SetOnDisconnected([&bot](Session&)
        {
            std::cout << "[Dummy " << bot.id << "] disconnected\n";
        });

    if (!bot.service.StartClient(1))
    {
        std::cout << "[Dummy " << bot.id << "] client start failed\n";
        return;
    }

    if (!bot.service.Connect(ip, port))
    {
        std::cout << "[Dummy " << bot.id << "] connect failed\n";
        bot.service.Stop();
        return;
    }

    int tick = 0;
    while (true)
    {
        Protocol::C_Chat chatReq;
        chatReq.set_message("안녕하세요 더미봇_" + std::to_string(bot.id) +
            " (" + std::to_string(tick++) + ")");
        bot.service.Broadcast(MakePacketFromProto(OP_C_CHAT, chatReq));

        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
}

int main()
{
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);

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
