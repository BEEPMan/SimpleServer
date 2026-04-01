#pragma once
#include <mutex>
#include <queue>
#include <functional>
#include <unordered_map>
#include <memory>
#include <string>
#include <thread>
#include <atomic>
#include "Player.h"
#include "SendBuffer.h"

class GameRoom
{
public:
    ~GameRoom() { StopGameLoop(); }

    // IOCP/Accept 스레드에서 호출 — 작업을 게임 루프 큐에 등록
    void PostTask(std::function<void()> task);

    // 게임 루프 스레드에서만 호출
    void Enter(std::shared_ptr<Player> player);
    void Leave(uint64_t playerId);
    void EnterGame(std::shared_ptr<Player> player, const std::string& name);
    void Move(std::shared_ptr<Player> player, const Vec3& pos, float yaw, float pitch);
    void Chat(std::shared_ptr<Player> player, const std::string& message);

    // 20Hz 게임 루프 시작/종료
    void StartGameLoop();
    void StopGameLoop();

private:
    void Send(Player* player, const SendBufferRef& buf);
    void Broadcast(const SendBufferRef& buf, Player* except = nullptr);

    // 게임 루프
    void GameLoopThread();
    void DrainTasks();
    void TickAll(float dt);
    void SimulatePlayer(Player& player, const InputCmd& cmd, float dt);

    static constexpr float TICK_RATE        = 20.f;    // Hz (서버 게임 루프)
    static constexpr float CLIENT_TICK_RATE = 60.f;    // Hz (클라이언트 입력 송신 주파수)
    static constexpr float MOVE_SPEED       = 5.5f;    // m/s
    static constexpr float GRAVITY          = 20.f;    // m/s²
    static constexpr float JUMP_VEL         = 6.5f;    // m/s
    static constexpr float GROUND_THRESHOLD = 0.08f;   // Unity CC skinWidth와 동일 — 지면 판정 허용 오차

    // 지면 충돌 판정 결과
    // 향후 맵 데이터(static_colliders.json) 기반 레이캐스트로 교체 예정
    struct GroundResult
    {
        bool  isGrounded;
        float groundY;    // 플레이어가 서야 할 Y 좌표
    };

    // 현재: y=0 평면을 지면으로 판정
    // 향후: static_colliders.json 레이캐스트로 교체
    static GroundResult CheckGround(const Vec3& pos);

    // 게임 루프 스레드 전용 — 락 없이 접근
    std::unordered_map<uint64_t, std::shared_ptr<Player>> _players;

    // 크로스 스레드 작업 큐 — _taskMutex로 보호
    std::queue<std::function<void()>> _taskQueue;
    std::mutex                        _taskMutex;

    std::thread       _loopThread;
    std::atomic<bool> _running{ false };
};
