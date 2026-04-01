#define NOMINMAX
#include "GameRoom.h"
#include "Session.h"
#include "ProtoUtil.h"
#include "Packet.h"
#include "Opcodes.h"
#include "game.pb.h"

#include <iostream>
#include <vector>
#include <cmath>
#include <algorithm>
#include <chrono>

// ─── 작업 큐 (크로스 스레드) ──────────────────────────────────────────────────

void GameRoom::PostTask(std::function<void()> task)
{
    std::lock_guard<std::mutex> lock(_taskMutex);
    _taskQueue.push(std::move(task));
}

void GameRoom::DrainTasks()
{
    std::queue<std::function<void()>> local;
    {
        std::lock_guard<std::mutex> lock(_taskMutex);
        std::swap(local, _taskQueue);
    }
    while (!local.empty())
    {
        local.front()();
        local.pop();
    }
}

// ─── 게임 루프 스레드 전용 함수들 ────────────────────────────────────────────

void GameRoom::Enter(std::shared_ptr<Player> player)
{
    _players[player->GetPlayerId()] = std::move(player);
}

void GameRoom::Leave(uint64_t playerId)
{
    auto it = _players.find(playerId);
    if (it == _players.end()) return;

    // S_Despawn 브로드캐스트
    Protocol::S_Despawn despawn;
    despawn.add_player_ids(playerId);
    Broadcast(MakeSendBuffer(MakePacketFromProto(OP_S_DESPAWN, despawn)));

    _players.erase(it);
}

void GameRoom::EnterGame(std::shared_ptr<Player> player, const std::string& name)
{
    if (name.empty() || name.size() > 20)
        return;
    if (player->IsEntered())
        return;

    player->SetName(name);  // _entered = true

    // 기존 플레이어 목록 수집
    std::vector<Player*> others;
    for (auto& [id, p] : _players)
    {
        if (p.get() != player.get())
            others.push_back(p.get());
    }

    // S_EnterGame — 입장 결과를 나에게 전송
    {
        Protocol::S_EnterGame res;
        res.set_success(true);
        auto* info = res.mutable_my_info();
        info->set_player_id(player->GetPlayerId());
        info->set_name(player->GetName());
        const PhysicsState& ps = player->GetPhysicsState();
        info->mutable_position()->set_x(ps.pos.x);
        info->mutable_position()->set_y(ps.pos.y);
        info->mutable_position()->set_z(ps.pos.z);
        info->set_yaw(ps.yaw);
        info->set_pitch(ps.pitch);
        Send(player.get(), MakeSendBuffer(MakePacketFromProto(OP_S_ENTER_GAME, res)));
    }

    // S_PlayerList — 현재 접속 중인 플레이어 목록을 나에게 전송
    {
        Protocol::S_PlayerList playerList;
        for (const auto& p : others)
        {
            auto* info = playerList.add_players();
            info->set_player_id(p->GetPlayerId());
            info->set_name(p->GetName());
            const PhysicsState& ps = p->GetPhysicsState();
            info->mutable_position()->set_x(ps.pos.x);
            info->mutable_position()->set_y(ps.pos.y);
            info->mutable_position()->set_z(ps.pos.z);
            info->set_yaw(ps.yaw);
            info->set_pitch(ps.pitch);
        }
        Send(player.get(), MakeSendBuffer(MakePacketFromProto(OP_S_PLAYER_LIST, playerList)));
    }

    // S_Spawn — 나의 등장을 기존 플레이어들에게 전파
    {
        Protocol::S_Spawn spawn;
        auto* info = spawn.add_players();
        info->set_player_id(player->GetPlayerId());
        info->set_name(player->GetName());
        const PhysicsState& myPs = player->GetPhysicsState();
        info->mutable_position()->set_x(myPs.pos.x);
        info->mutable_position()->set_y(myPs.pos.y);
        info->mutable_position()->set_z(myPs.pos.z);
        info->set_yaw(myPs.yaw);
        info->set_pitch(myPs.pitch);
        auto buf = MakeSendBuffer(MakePacketFromProto(OP_S_SPAWN, spawn));
        for (auto* p : others)
            Send(p, buf);
    }

    std::cout << "[Server] Player entered: " << name << "\n";
}

void GameRoom::Move(std::shared_ptr<Player> player, const Vec3& pos, float yaw, float pitch)
{
    player->SetPosition(pos);
    player->GetPhysicsState().yaw   = yaw;
    player->GetPhysicsState().pitch = pitch;

    Protocol::S_Move res;
    res.set_player_id(player->GetPlayerId());
    res.mutable_position()->set_x(pos.x);
    res.mutable_position()->set_y(pos.y);
    res.mutable_position()->set_z(pos.z);
    res.set_yaw(yaw);
    res.set_pitch(pitch);

    Broadcast(MakeSendBuffer(MakePacketFromProto(OP_S_MOVE, res)));
}

void GameRoom::Chat(std::shared_ptr<Player> player, const std::string& message)
{
    Protocol::S_Chat res;
    res.set_player_id(player->GetPlayerId());
    res.set_name(player->GetName());
    res.set_message(message);

    Broadcast(MakeSendBuffer(MakePacketFromProto(OP_S_CHAT, res)));
}

void GameRoom::Send(Player* player, const SendBufferRef& buf)
{
    if (Session* s = player->GetSession())
        s->EnqueueSend(buf);
}

void GameRoom::Broadcast(const SendBufferRef& buf, Player* except)
{
    // 게임 루프 스레드 전용 — 락 없이 _players 순회
    for (auto& [id, player] : _players)
    {
        if (player.get() == except) continue;
        if (Session* s = player->GetSession())
            s->EnqueueSend(buf);
    }
}

// ─── 게임 루프 ────────────────────────────────────────────────────────────────

void GameRoom::StartGameLoop()
{
    _running = true;
    _loopThread = std::thread([this]() { GameLoopThread(); });
}

void GameRoom::StopGameLoop()
{
    _running = false;
    if (_loopThread.joinable())
        _loopThread.join();
}

void GameRoom::GameLoopThread()
{
    using namespace std::chrono;
    constexpr auto TICK_INTERVAL = milliseconds(50); // 20Hz

    auto nextTick = steady_clock::now() + TICK_INTERVAL;

    while (_running)
    {
        DrainTasks();
        TickAll(1.f / TICK_RATE);

        auto now = steady_clock::now();
        if (now < nextTick)
            std::this_thread::sleep_until(nextTick);

        nextTick += TICK_INTERVAL;
    }
}

void GameRoom::TickAll(float /*dt*/)
{
    constexpr float CLIENT_DT = 1.f / CLIENT_TICK_RATE;  // 1/60 — 클라이언트 틱 dt와 동일

    for (auto& [id, player] : _players)
    {
        if (!player->IsEntered()) continue;

        // 클라이언트가 이번 서버 틱 사이에 쌓아 놓은 입력을 소진.
        // 각 입력은 클라이언트 예측과 동일한 dt(1/60)로 시뮬레이션.
        // 상한(MAX_STEPS_PER_TICK)을 두어 패킷 버스트 시 중력 과다 적용 방지.
        constexpr int MAX_STEPS_PER_TICK = 4;  // 서버 틱 간격 50ms / 클라이언트 dt 16.7ms ≈ 3, 여유 1 추가
        int steps = 0;
        while (player->HasInput() && steps < MAX_STEPS_PER_TICK)
        {
            InputCmd cmd = player->DequeueInput();
            SimulatePlayer(*player, cmd, CLIENT_DT);
            steps++;
        }
        if (steps == 0)
        {
            // 입력 없음 — zero-input으로 중력·낙하 처리 유지
            SimulatePlayer(*player, InputCmd{}, CLIENT_DT);
        }

        // S_PlayerState 브로드캐스트
        const PhysicsState& state = player->GetPhysicsState();
        Protocol::S_PlayerState res;
        res.set_player_id(player->GetPlayerId());
        res.set_tick(state.lastTick);
        res.mutable_position()->set_x(state.pos.x);
        res.mutable_position()->set_y(state.pos.y);
        res.mutable_position()->set_z(state.pos.z);
        res.set_yaw(state.yaw);
        res.set_vert_vel(state.vertVel);
        res.set_is_grounded(state.isGrounded);

        Broadcast(MakeSendBuffer(MakePacketFromProto(OP_S_PLAYER_STATE, res)));
    }
}

// ─── 지면 충돌 판정 ───────────────────────────────────────────────────────────
// 현재: y=0 평면을 지면으로 판정
// 향후: static_colliders.json 레이캐스트로 교체
GameRoom::GroundResult GameRoom::CheckGround(const Vec3& pos)
{
    // y=0이 지면. GROUND_THRESHOLD(Unity CC skinWidth)만큼 여유를 두어
    // cc.isGrounded와 동일한 조건으로 판정
    constexpr float GROUND_Y = 0.f;
    if (pos.y <= GROUND_Y + GROUND_THRESHOLD)
        return { true, GROUND_Y };
    return { false, GROUND_Y };
}

void GameRoom::SimulatePlayer(Player& player, const InputCmd& cmd, float dt)
{
    PhysicsState& state = player.GetPhysicsState();

    // 1) 이전 틱 위치 기준 지면 판정
    GroundResult ground = CheckGround(state.pos);
    state.isGrounded = ground.isGrounded;

    // 2) 지면에 붙어있을 때 하방 속도 스냅 (Unity: if (grounded && velY < 0) velY = -2f)
    if (state.isGrounded && state.vertVel < 0.f)
        state.vertVel = -2.f;

    // 3) 점프 (지면에 있을 때만)
    if (state.isGrounded && cmd.jump)
        state.vertVel = JUMP_VEL;

    // 4) 중력 (점프 프레임 포함 매 틱 적용 — Unity와 동일 순서)
    state.vertVel -= GRAVITY * dt;

    // 5) yaw / pitch 업데이트
    state.yaw  += cmd.yawDelta;
    state.pitch = std::clamp(state.pitch + cmd.pitchDelta, -80.f, 80.f);

    // 6) 수평 이동: 로컬 입력을 yaw 기준 월드 좌표로 변환
    //    Unity 좌표계: yaw 0° = +Z, 시계방향 증가
    //    클라이언트의 Vector3.ClampMagnitude와 동일하게 대각선 이동 정규화
    float moveLen = std::sqrt(cmd.moveX * cmd.moveX + cmd.moveZ * cmd.moveZ);
    float normX   = (moveLen > 1.f) ? cmd.moveX / moveLen : cmd.moveX;
    float normZ   = (moveLen > 1.f) ? cmd.moveZ / moveLen : cmd.moveZ;
    float rad  = state.yaw * (3.14159265f / 180.f);
    float sinY = std::sin(rad);
    float cosY = std::cos(rad);
    state.pos.x += (sinY * normZ + cosY * normX) * MOVE_SPEED * dt;
    state.pos.z += (cosY * normZ - sinY * normX) * MOVE_SPEED * dt;

    // 7) 수직 이동
    state.pos.y += state.vertVel * dt;

    // 8) 지면 충돌 해결 — 이동 후 위치로 재판정하여 클램프 + velY 처리
    GroundResult postGround = CheckGround(state.pos);
    if (postGround.isGrounded)
    {
        state.pos.y      = postGround.groundY;
        state.isGrounded = true;
        // Unity CharacterController는 지면 접지 시 velY = -2 를 유지함
        state.vertVel    = -2.f;
    }

    // zero-input(tick=0) 시 lastTick을 덮어쓰지 않음.
    // 입력 부재 틱에서 tick=0이 전송되면 클라이언트 reconcile 매칭이 깨짐.
    if (cmd.tick != 0)
        state.lastTick = cmd.tick;
}
