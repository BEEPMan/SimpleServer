#pragma once
#include <cstdint>
#include <string>
#include <deque>
#include <mutex>
#include <atomic>

class Session;

struct Vec3
{
    float x = 0.f;
    float y = 0.f;
    float z = 0.f;
};

// 클라이언트에서 수신하는 틱 단위 입력 커맨드
struct InputCmd
{
    uint32_t tick       = 0;
    float    moveX      = 0.f;  // -1.0 ~ 1.0
    float    moveZ      = 0.f;  // -1.0 ~ 1.0
    float    yawDelta   = 0.f;  // 도(degree) 단위
    float    pitchDelta = 0.f;  // 도(degree) 단위
    bool     jump       = false;
};

// 서버 권위적 물리 상태 (게임 루프 스레드 전용)
struct PhysicsState
{
    Vec3     pos;
    float    yaw        = 0.f;
    float    pitch      = 0.f;  // -80 ~ 80 클램프
    float    vertVel    = 0.f;
    bool     isGrounded = true;
    uint32_t lastTick   = 0;
};

class Player
{
public:
    Player(uint64_t id, std::string name);

    uint64_t           GetPlayerId()  const { return _playerId; }
    const std::string& GetName()      const { return _name; }
    const Vec3&        GetPosition()  const { return _position; }
    const Vec3&        GetDirection() const { return _direction; }
    Session*           GetSession()   const { return _session; }

    // 입장 완료 여부 (atomic — IOCP 스레드에서 안전하게 읽기 가능)
    bool IsEntered() const { return _entered.load(std::memory_order_acquire); }

    // 아래 Set* 는 게임 루프 스레드에서만 호출할 것
    void SetName(const std::string& name)
    {
        _name = name;
        _entered.store(true, std::memory_order_release);
    }
    void SetPosition(const Vec3& pos)     { _position = pos; }
    void SetDirection(const Vec3& dir)    { _direction = dir; }
    void SetSession(Session* s)           { _session = s; }

    // 물리 상태 (게임 루프 스레드 전용)
    PhysicsState&       GetPhysicsState()       { return _physState; }
    const PhysicsState& GetPhysicsState() const { return _physState; }

    // 입력 버퍼 — EnqueueInput: IOCP 스레드, DequeueInput/HasInput: 게임 루프 스레드
    void     EnqueueInput(const InputCmd& cmd);
    InputCmd DequeueInput();   // 버퍼가 비면 zero-input 반환
    bool     HasInput() const;  // 처리 대기 입력 존재 여부

private:
    static constexpr int MAX_INPUT_BUFFER = 32;

    uint64_t    _playerId;
    std::string _name;
    Vec3        _position;
    Vec3        _direction;
    Session*    _session = nullptr;

    std::atomic<bool> _entered{ false };

    PhysicsState         _physState;
    std::deque<InputCmd>   _inputBuffer;
    mutable std::mutex     _inputMutex;
};
