#include <WinSock2.h>
#include <Windows.h>
#include <iostream>
#include <fstream>
#include <atomic>
#include <filesystem>
#include <chrono>
#include <ctime>

#include "NetService.h"
#include "ServerPacketHandler.h"
#include "ClientSession.h"
#include "Player.h"
#include "GameRoom.h"

#pragma comment(lib, "ws2_32.lib")

// cout을 콘솔 + 파일에 동시 출력하는 streambuf
class TeeBuf : public std::streambuf
{
public:
    TeeBuf(std::streambuf* console, std::streambuf* file)
        : _console(console), _file(file) {}

protected:
    int overflow(int c) override
    {
        if (c == EOF) return !EOF;
        if (_console->sputc(static_cast<char>(c)) == EOF) return EOF;
        if (_file   ->sputc(static_cast<char>(c)) == EOF) return EOF;
        return c;
    }

    std::streamsize xsputn(const char* s, std::streamsize n) override
    {
        _console->sputn(s, n);
        _file   ->sputn(s, n);
        return n;
    }

    int sync() override
    {
        _console->pubsync();
        _file   ->pubsync();
        return 0;
    }

private:
    std::streambuf* _console;
    std::streambuf* _file;
};

static std::atomic<uint64_t> s_nextPlayerId{ 1 };

int main()
{
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);

    // ── 로그 파일 설정 (exe → ../../../Logs) ─────────────────────────────────
    char exeBuf[MAX_PATH];
    GetModuleFileNameA(nullptr, exeBuf, MAX_PATH);
    auto logsDir = (std::filesystem::path(exeBuf).parent_path()
                    / "../../../Logs").lexically_normal();
    std::filesystem::create_directories(logsDir);

    auto cur  = logsDir / "server.log";
    auto prev = logsDir / "server.prev.log";
    if (std::filesystem::exists(prev)) std::filesystem::remove(prev);
    if (std::filesystem::exists(cur))  std::filesystem::rename(cur, prev);

    std::ofstream logFile(cur);
    auto origCoutBuf = std::cout.rdbuf();
    TeeBuf teeBuf(origCoutBuf, logFile.rdbuf());
    std::cout.rdbuf(&teeBuf);

    // 세션 시작 헤더
    auto now = std::chrono::system_clock::now();
    std::time_t t = std::chrono::system_clock::to_time_t(now);
    char timeBuf[32];
    struct tm tmInfo{};
    localtime_s(&tmInfo, &t);
    std::strftime(timeBuf, sizeof(timeBuf), "%Y-%m-%d %H:%M:%S", &tmInfo);
    std::cout << "=== Session started: " << timeBuf << " ===\n";

    WSADATA wsa{};
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0)
    {
        std::cout << "WSAStartup failed\n";
        return 1;
    }

    GameRoom room;
    ServerPacketHandler handler;
    NetService service(handler);
    handler.SetRoom(&room);

    service.SetSessionFactory([](SOCKET sock, IPacketHandler& h, NetService& svc) -> Session*
        {
            return new ClientSession(sock, h, svc);
        });

    service.SetOnConnected([&room](Session& session)
        {
            auto& cs = static_cast<ClientSession&>(session);
            uint64_t id = s_nextPlayerId.fetch_add(1, std::memory_order_relaxed);
            auto player = std::make_shared<Player>(id, "");
            player->SetSession(&session);
            cs.SetPlayer(player);

            // Enter는 게임 루프 스레드에서 처리
            room.PostTask([&room, player]()
            {
                room.Enter(player);
            });

            std::cout << "[Server] Client connected. Player id=" << id << "\n";
        });

    service.SetOnDisconnected([&room](Session& session)
        {
            auto& cs = static_cast<ClientSession&>(session);
            auto player = cs.GetPlayer();
            if (player)
            {
                uint64_t id = player->GetPlayerId();
                player->SetSession(nullptr);

                // Leave는 게임 루프 스레드에서 처리
                room.PostTask([&room, id]()
                {
                    room.Leave(id);
                });

                std::cout << "[Server] Client disconnected. Player id=" << id << "\n";
            }
        });

    if (!service.StartServer("0.0.0.0", 7777, 4))
    {
        std::cout << "Server start failed\n";
        WSACleanup();
        return 1;
    }

    char exePathBuf[MAX_PATH];
    GetModuleFileNameA(nullptr, exePathBuf, MAX_PATH);
    auto mapPath = std::filesystem::path(exePathBuf).parent_path()
                   / "../../../SimpleClient/Exports/tilemap.json";
    room.LoadMap(mapPath.lexically_normal().string());
    room.StartGameLoop();
    service.RunAcceptLoop();
    room.StopGameLoop();

    service.Stop();
    WSACleanup();

    std::cout.rdbuf(origCoutBuf);
    return 0;
}
