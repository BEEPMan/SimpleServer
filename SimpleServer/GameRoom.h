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
#include "Map.h"

class GameRoom
{
public:
    ~GameRoom() { StopGameLoop(); }

    // IOCP/Accept 스레드에서 호출 — 작업을 게임 루프 큐에 등록
    void PostTask(std::function<void()> task);

    // TilemapExporter JSON을 로드 — StartGameLoop() 전에 호출
    bool LoadMap(const std::string& jsonPath);

    // 게임 루프 스레드에서만 호출
    void Enter(std::shared_ptr<Player> player);
    void Leave(uint64_t playerId);
    void EnterGame(std::shared_ptr<Player> player, const std::string& name);
    void Chat(std::shared_ptr<Player> player, const std::string& message, int32_t type);
    void HandleLadderState(std::shared_ptr<Player> player, bool onLadder,
                           float px, float py, float vx, float vy);
    void BroadcastAttack(std::shared_ptr<Player> attacker, uint32_t skillId,
                         int attackType, float aimX, float aimY);

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

    // ── 물리 상수 ────────────────────────────────────────────
    static constexpr float TICK_RATE        = 20.f;    // Hz
    static constexpr float CLIENT_TICK_RATE = 50.f;    // Hz (Unity FixedUpdate 기본값)
    static constexpr float MOVE_SPEED       = 5.0f;    // m/s  ← RPGPlayerController.moveSpeed
    static constexpr float JUMP_FORCE       = 10.f;    // m/s  ← AddForce Impulse / mass(1)
    static constexpr float GRAVITY          = 29.43f;  // m/s² ← Physics2D(9.81) × gravityScale(3)
    static constexpr float GROUND_THRESHOLD = 0.1f;    // 접지 판정 여유값 ← 클라이언트 groundCheckRadius(0.1f)와 일치
    static constexpr float MAX_FALL_SPEED  = -15.f;   // 터미널 속도 — |v|×useDt_max(0.1)=1.5 < cellSizeY+HALF_H
    static constexpr float CLIMB_SPEED     = 3.f;     // m/s ← RPGPlayerController.climbSpeed

    // ── 플레이어 AABB (BoxCollider2D size=(1,1), offset=(0,0)) ─
    // pos = transform.position = 콜라이더 중심(Center)
    static constexpr float PLAYER_HALF_W = 0.3f;   // size.x / 2
    static constexpr float PLAYER_HALF_H = 0.5f;   // size.y / 2
    static constexpr float PLAYER_HEIGHT = 1.0f;   // size.y (편의용)

    // ── 충돌 결과 ────────────────────────────────────────────
    struct GroundResult { bool  hit; float groundY; };
    struct CeilResult   { bool  hit; float ceilY;   };
    struct WallResult   { bool  hit; float wallX;   };

    // 타일맵 기반 충돌 판정 (게임 루프 전용)
    GroundResult CheckGround(const Vec2& pos, bool ignoreFloating = false) const;
    CeilResult   CheckCeiling(const Vec2& pos) const;
    WallResult   CheckWall(const Vec2& pos, float moveX) const;
    void         GetLadderExtent(int cx, int startCy, float& centerX, float& minY, float& maxY) const;

    // 게임 루프 스레드 전용 — 락 없이 접근
    std::unordered_map<uint64_t, std::shared_ptr<Player>> _players;
    Map      _map;
    uint32_t _serverTick = 0;

    // 크로스 스레드 작업 큐 — _taskMutex로 보호
    std::queue<std::function<void()>> _taskQueue;
    std::mutex                        _taskMutex;

    std::thread       _loopThread;
    std::atomic<bool> _running{ false };
};
