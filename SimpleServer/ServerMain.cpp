#include <WinSock2.h>
#include <iostream>

#include "NetService.h"
#include "ServerPacketHandler.h"
#include "Session.h"

#pragma comment(lib, "ws2_32.lib")

int main()
{
    WSADATA wsa{};
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0)
    {
        std::cout << "WSAStartup 초기화 실패\n";
        return 1;
    }

    ServerPacketHandler handler;
    NetService service(handler);
    handler.SetService(&service);

    service.SetOnConnected([](Session&)
        {
            std::cout << "[서버] 클라이언트 접속\n";
        });

    service.SetOnDisconnected([](Session&)
        {
            std::cout << "[서버] 클라이언트 접속 종료\n";
        });

    if (!service.StartServer("0.0.0.0", 7777, 4))
    {
        std::cout << "서버 시작 실패\n";
        WSACleanup();
        return 1;
    }

    service.RunAcceptLoop();

    service.Stop();
    WSACleanup();
    return 0;
}