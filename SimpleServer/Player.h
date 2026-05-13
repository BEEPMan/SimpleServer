#pragma once
#include <cstdint>
#include <string>
#include <deque>
#include <mutex>
#include <atomic>

class Session;

struct Vec2
{
    float x = 0.f;
    float y = 0.f;
};

// game.proto MoveDir 값과 1:1 대응 (캐스트로 변환)
enum class MoveDir : uint8_t { None = 0, Left = 1, Right = 2 };

// 클라이언트에서 수신하는 입력 커맨드 (C_InputCmd 대응)
struct InputCmd
{
    uint32_t inputSeq    = 0;
    float    deltaTime   = 0.f;
    bool     moveLeft    = false;
    bool     moveRight   = false;
    bool     jump        = false;
    bool     dropThrough = false;   // proto attack 필드를 drop-through 신호로 재사용
    MoveDir  faceDir     = MoveDir::None;
    bool     moveUp      = false;   // 사다리 상승
    bool     moveDown    = false;   // 사다리 하강
};

// 서버 권위적 물리 상태 (게임 루프 스레드 전용)
struct PhysicsState
{
    Vec2     pos;
    Vec2     vel;                       // x: 수평 속도, y: 수직 속도
    MoveDir  dir        = MoveDir::Right;
    bool     isMoving   = false;
    bool     isJumping  = false;
    bool     isGrounded       = true;
    bool     isOnLadder       = false;  // 사다리 상태 (서버 시뮬레이션)
    float    ladderCenterX    = 0.f;    // 사다리 중심 X (월드 좌표)
    float    ladderMinY       = 0.f;    // 사다리 하단 Y (월드 좌표)
    float    ladderMaxY       = 0.f;    // 사다리 상단 Y (월드 좌표)
    float    dropThroughTimer = 0.f;    // >0이면 Floating Ground 충돌 무시
    uint32_t lastInputSeq     = 0;      // Reconciliation 기준점
};

class Player
{
public:
    Player(uint64_t id, std::string name);

    uint64_t           GetPlayerId() const { return _playerId; }
    const std::string& GetName()     const { return _name; }
    const Vec2&        GetPosition() const { return _physState.pos; }
    Session*           GetSession()  const { return _session; }

    // atomic read — IOCP 스레드에서 안전
    bool IsEntered() const { return _entered.load(std::memory_order_acquire); }

    // 아래 Set* 는 게임 루프 스레드에서만 호출할 것
    void SetName(const std::string& name)
    {
        _name = name;
        _entered.store(true, std::memory_order_release);
    }
    void SetSession(Session* s) { _session = s; }

    PhysicsState&       GetPhysicsState()       { return _physState; }
    const PhysicsState& GetPhysicsState() const { return _physState; }

    // EnqueueInput: IOCP 스레드 / Dequeue·HasInput: 게임 루프 스레드
    void     EnqueueInput(const InputCmd& cmd);
    InputCmd DequeueInput();
    bool     HasInput() const;

private:
    static constexpr int MAX_INPUT_BUFFER = 32;

    uint64_t    _playerId;
    std::string _name;
    Session*    _session = nullptr;

    std::atomic<bool> _entered{ false };

    PhysicsState         _physState;
    std::deque<InputCmd> _inputBuffer;
    mutable std::mutex   _inputMutex;
};
