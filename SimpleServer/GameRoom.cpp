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

// ── 맵 로드 ───────────────────────────────────────────────────────────────────

bool GameRoom::LoadMap(const std::string& jsonPath)
{
    return _map.Load(jsonPath);
}

// ── 작업 큐 (크로스 스레드) ───────────────────────────────────────────────────

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

// ── 플레이어 입장/퇴장 ────────────────────────────────────────────────────────

void GameRoom::Enter(std::shared_ptr<Player> player)
{
    _players[player->GetPlayerId()] = std::move(player);
}

void GameRoom::Leave(uint64_t playerId)
{
    auto it = _players.find(playerId);
    if (it == _players.end()) return;

    Protocol::S_Despawn despawn;
    despawn.add_player_ids(playerId);
    Broadcast(MakeSendBuffer(MakePacketFromProto(OP_S_DESPAWN, despawn)));

    _players.erase(it);
}

static void FillPlayerInfo(Protocol::PlayerInfo* info, const Player& player)
{
    const PhysicsState& ps = player.GetPhysicsState();
    info->set_player_id(player.GetPlayerId());
    info->set_name(player.GetName());
    info->mutable_position()->set_x(ps.pos.x);
    info->mutable_position()->set_y(ps.pos.y);
    info->set_dir(static_cast<Protocol::MoveDir>(ps.dir));
    info->set_is_moving(ps.isMoving);
    info->set_is_jumping(ps.isJumping);
}

void GameRoom::EnterGame(std::shared_ptr<Player> player, const std::string& name)
{
    if (name.empty() || name.size() > 20) return;
    if (player->IsEntered()) return;

    player->SetName(name);

    std::vector<Player*> others;
    for (auto& [id, p] : _players)
        if (p.get() != player.get())
            others.push_back(p.get());

    {
        Protocol::S_EnterGame res;
        res.set_success(true);
        FillPlayerInfo(res.mutable_my_info(), *player);
        Send(player.get(), MakeSendBuffer(MakePacketFromProto(OP_S_ENTER_GAME, res)));
    }
    {
        Protocol::S_PlayerList playerList;
        for (const auto& p : others)
            FillPlayerInfo(playerList.add_players(), *p);
        Send(player.get(), MakeSendBuffer(MakePacketFromProto(OP_S_PLAYER_LIST, playerList)));
    }
    {
        Protocol::S_Spawn spawn;
        FillPlayerInfo(spawn.add_players(), *player);
        auto buf = MakeSendBuffer(MakePacketFromProto(OP_S_SPAWN, spawn));
        for (auto* p : others)
            Send(p, buf);
    }

    std::cout << "[Server] Player entered: " << name << "\n";
}

void GameRoom::Chat(std::shared_ptr<Player> player, const std::string& message, int32_t type)
{
    Protocol::S_Chat res;
    res.set_player_id(player->GetPlayerId());
    res.set_name(player->GetName());
    res.set_type(static_cast<Protocol::ChatType>(type));
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
    for (auto& [id, player] : _players)
    {
        if (player.get() == except) continue;
        if (Session* s = player->GetSession())
            s->EnqueueSend(buf);
    }
}

// ── 게임 루프 ─────────────────────────────────────────────────────────────────

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
    constexpr float CLIENT_DT = 1.f / CLIENT_TICK_RATE;

    for (auto& [id, player] : _players)
    {
        if (!player->IsEntered()) continue;

        constexpr int MAX_STEPS_PER_TICK = 8;
        int steps = 0;
        while (player->HasInput() && steps < MAX_STEPS_PER_TICK)
        {
            InputCmd cmd = player->DequeueInput();
            SimulatePlayer(*player, cmd, CLIENT_DT);
            steps++;
        }
        if (steps == 0)
        {
            // 입력 없을 때 실시간 물리 유지 (50Hz 클라이언트 대비 3스텝 = 0.06s ≈ 틱 50ms)
            for (int i = 0; i < 3; i++)
                SimulatePlayer(*player, InputCmd{}, CLIENT_DT);
        }

        const PhysicsState& state = player->GetPhysicsState();

        {
            Protocol::S_PlayerState msg;
            msg.set_player_id(player->GetPlayerId());
            msg.set_last_processed_input_seq(state.lastInputSeq);
            msg.mutable_position()->set_x(state.pos.x);
            msg.mutable_position()->set_y(state.pos.y);
            msg.mutable_velocity()->set_x(state.vel.x);
            msg.mutable_velocity()->set_y(state.isGrounded ? 0.f : state.vel.y);
            msg.set_is_grounded(state.isGrounded);
            msg.set_dir(static_cast<Protocol::MoveDir>(state.dir));
            msg.set_is_moving(state.isMoving);
            msg.set_is_jumping(state.isJumping);
            msg.set_is_on_ladder(state.isOnLadder);
            Send(player.get(), MakeSendBuffer(MakePacketFromProto(OP_S_PLAYER_STATE, msg)));
        }
        {
            Protocol::S_BroadcastMove bcast;
            bcast.set_player_id(player->GetPlayerId());
            bcast.mutable_position()->set_x(state.pos.x);
            bcast.mutable_position()->set_y(state.pos.y);
            bcast.mutable_velocity()->set_x(state.vel.x);
            bcast.mutable_velocity()->set_y(state.isGrounded ? 0.f : state.vel.y);
            bcast.set_dir(static_cast<Protocol::MoveDir>(state.dir));
            bcast.set_is_grounded(state.isGrounded);
            bcast.set_is_moving(state.isMoving);
            bcast.set_is_jumping(state.isJumping);
            bcast.set_server_tick(_serverTick);
            Broadcast(MakeSendBuffer(MakePacketFromProto(OP_S_BROADCAST_MOVE, bcast)), player.get());
        }
    }

    _serverTick++;
}

// ── 충돌 판정 (AABB 타일 기반) ────────────────────────────────────────────────
//
// 좌표 규약:
//   pos.x / pos.y = 플레이어 AABB 중심 좌표
//   발 바닥 Y = pos.y - PLAYER_HALF_H
//   머리  Y = pos.y + PLAYER_HALF_H
//
// 구조물은 전부 x·y 축에 평행한 변을 가진 다각형(AABB).
// 타일맵에서 각 셀이 크기 cellSizeX × cellSizeY 의 AABB 하나에 대응.
//   셀 (cx, cy) 의 월드 경계:
//     X = [cx * cellSizeX,  (cx+1) * cellSizeX)
//     Y = [cy * cellSizeY,  (cy+1) * cellSizeY)

// 발 아래 지면 탐색
GameRoom::GroundResult GameRoom::CheckGround(const Vec2& pos, bool ignoreFloating) const
{
    if (!_map.IsLoaded())
    {
        // 맵 미로드 폴백: y=0 을 지면으로 사용
        const float FALLBACK_Y = 0.f;
        bool onGround = (pos.y - PLAYER_HALF_H) <= FALLBACK_Y + GROUND_THRESHOLD;
        return { onGround, FALLBACK_Y };
    }

    const float csx   = _map.CellSizeX();
    const float csy   = _map.CellSizeY();
    const float footY = pos.y - PLAYER_HALF_H;

    // 발 바닥 바로 아래 셀
    int cy    = Map::WorldToCell(footY - GROUND_THRESHOLD, csy);
    // 플레이어 너비 범위의 X 셀 (안쪽 여유 0.01 — 타일 경계 오인 방지)
    int cxMin = Map::WorldToCell(pos.x - PLAYER_HALF_W + 0.01f, csx);
    int cxMax = Map::WorldToCell(pos.x + PLAYER_HALF_W - 0.01f, csx);

    float bestGroundY = -1e9f;
    bool  hit         = false;

    for (int cx = cxMin; cx <= cxMax; cx++)
    {
        bool solid    = _map.IsSolid(cx, cy);
        bool floating = !ignoreFloating && _map.IsFloatingGround(cx, cy);
        if (!solid && !floating) continue;

        float tileTop = _map.CellTopY(cy);
        if (tileTop > footY + GROUND_THRESHOLD) continue;
        if (tileTop > bestGroundY)
        {
            bestGroundY = tileTop;
            hit         = true;
        }
    }

    return { hit, hit ? bestGroundY : 0.f };
}

// 머리 위 천장 탐색
GameRoom::CeilResult GameRoom::CheckCeiling(const Vec2& pos) const
{
    if (!_map.IsLoaded()) return { false, 0.f };

    const float csx   = _map.CellSizeX();
    const float csy   = _map.CellSizeY();
    const float headY = pos.y + PLAYER_HALF_H;

    // 머리 바로 위 셀
    int cy    = Map::WorldToCell(headY + GROUND_THRESHOLD, csy);
    int cxMin = Map::WorldToCell(pos.x - PLAYER_HALF_W + 0.01f, csx);
    int cxMax = Map::WorldToCell(pos.x + PLAYER_HALF_W - 0.01f, csx);

    float bestCeilY = 1e9f;
    bool  hit       = false;

    for (int cx = cxMin; cx <= cxMax; cx++)
    {
        if (!_map.IsSolid(cx, cy)) continue;

        float tileBottom = _map.CellBottomY(cy);
        if (tileBottom < bestCeilY)
        {
            bestCeilY = tileBottom;
            hit       = true;
        }
    }

    return { hit, hit ? bestCeilY : 0.f };
}

// 좌·우 벽 탐색 (moveX > 0: 오른쪽, moveX < 0: 왼쪽)
GameRoom::WallResult GameRoom::CheckWall(const Vec2& pos, float moveX) const
{
    if (!_map.IsLoaded() || moveX == 0.f) return { false, 0.f };

    const float csx = _map.CellSizeX();
    const float csy = _map.CellSizeY();

    // 이동 방향 쪽 플레이어 끝 X (약간 돌출해 경계 셀 포함)
    float sideX = (moveX > 0.f)
        ? pos.x + PLAYER_HALF_W + 0.01f
        : pos.x - PLAYER_HALF_W - 0.01f;

    int cx = Map::WorldToCell(sideX, csx);
    // 발~머리 범위 (여유 0.05 — 지면·천장 타일을 벽으로 오인하는 현상 방지)
    int cyMin = Map::WorldToCell(pos.y - PLAYER_HALF_H + 0.05f, csy);
    int cyMax = Map::WorldToCell(pos.y + PLAYER_HALF_H - 0.05f, csy);

    for (int cy = cyMin; cy <= cyMax; cy++)
    {
        if (!_map.IsSolid(cx, cy)) continue;

        float wallX = (moveX > 0.f)
            ? _map.CellLeftX(cx)  - PLAYER_HALF_W   // 오른쪽 벽: 타일 왼쪽 면 - 반너비
            : _map.CellRightX(cx) + PLAYER_HALF_W;  // 왼쪽 벽: 타일 오른쪽 면 + 반너비
        return { true, wallX };
    }

    return { false, 0.f };
}

// ── 플레이어 물리 시뮬레이션 ──────────────────────────────────────────────────
//
// 충돌 해결 순서: 수평 이동 → 벽 해결 → 수직 이동 → 지면/천장 해결
// 이 순서를 지켜야 코너 끼임 없이 안정적으로 동작한다.

void GameRoom::GetLadderExtent(int cx, int startCy,
                                float& centerX, float& minY, float& maxY) const
{
    const float csx = _map.CellSizeX();
    const float csy = _map.CellSizeY();
    centerX = (cx + 0.5f) * csx;

    int yMin = startCy;
    while (_map.IsLadder(cx, yMin - 1)) yMin--;

    int yMax = startCy;
    while (_map.IsLadder(cx, yMax + 1)) yMax++;

    minY = yMin * csy;         // CellBottomY(yMin)
    maxY = (yMax + 1) * csy;   // CellTopY(yMax)
}

void GameRoom::HandleLadderState(std::shared_ptr<Player> player, bool onLadder,
                                  float px, float py, float vx, float vy)
{
    if (!onLadder) return;  // 이탈은 SimulatePlayer가 처리

    PhysicsState& s = player->GetPhysicsState();
    s.isOnLadder = true;
    s.pos        = { px, py };
    s.vel        = { 0.f, 0.f };
    s.isGrounded = false;
    s.isJumping  = false;

    // 맵에서 사다리 범위 계산
    if (_map.IsLoaded())
    {
        int cx = Map::WorldToCell(px, _map.CellSizeX());
        int cy = Map::WorldToCell(py, _map.CellSizeY());
        // 플레이어 발 위치 기준 셀 탐색
        if (!_map.IsLadder(cx, cy)) cy--;
        if (_map.IsLadder(cx, cy))
            GetLadderExtent(cx, cy, s.ladderCenterX, s.ladderMinY, s.ladderMaxY);
        else
        {
            s.ladderCenterX = px;
            s.ladderMinY    = py - 0.5f;
            s.ladderMaxY    = py + 10.f;
        }
    }

    std::cout << "[Ladder] id=" << player->GetPlayerId()
              << " enter pos=(" << px << "," << py << ")"
              << " center=" << s.ladderCenterX
              << " minY=" << s.ladderMinY
              << " maxY=" << s.ladderMaxY << "\n";
}

void GameRoom::SimulatePlayer(Player& player, const InputCmd& cmd, float dt)
{
    PhysicsState& s = player.GetPhysicsState();

    // 사다리 물리 시뮬레이션 (서버 권위)
    if (s.isOnLadder)
    {
        const float useDt = (cmd.deltaTime > 0.f && cmd.deltaTime < 0.1f) ? cmd.deltaTime : dt;

        // 점프 → 사다리 이탈 후 공중 물리
        if (cmd.jump)
        {
            s.isOnLadder = false;
            s.vel.y      = JUMP_FORCE;
            s.vel.x      = 0.f;
            s.isJumping  = true;
            s.isGrounded = false;
            std::cout << "[Ladder] id=" << player.GetPlayerId() << " jump-exit\n";
            // 이탈 후 동일 프레임 일반 물리로 fall-through
        }
        else
        {
            float vy = 0.f;
            if (cmd.moveUp)   vy =  CLIMB_SPEED;
            if (cmd.moveDown) vy = -CLIMB_SPEED;

            s.vel.x  = 0.f;
            s.vel.y  = vy;
            s.pos.x  = s.ladderCenterX;
            s.pos.y += vy * useDt;

            // 사다리 범위 클램프 (상단은 PLAYER_HALF_H 여유 포함 — 클라이언트 이탈 임계값과 일치)
            if (s.pos.y < s.ladderMinY)                    s.pos.y = s.ladderMinY;
            if (s.pos.y > s.ladderMaxY + PLAYER_HALF_H)   s.pos.y = s.ladderMaxY + PLAYER_HALF_H;

            // 하단 이탈: solid ground 도달 + 능동 하강 중 (vy < 0)
            // vy == 0(정지) 일 때는 이탈하지 않음 — 지면 위에서 진입 시 즉시 이탈 방지
            // ignoreFloating=true: 내려오는 발판을 다시 ground로 인식하는 현상 방지
            //   (클라이언트도 사다리 탑승 시 IgnoreCollision(floating, true)로 동일하게 처리)
            if (vy < 0.f)
            {
                GroundResult gr = CheckGround(s.pos, true);
                if (gr.hit)
                {
                    s.isOnLadder = false;
                    s.pos.y      = gr.groundY + PLAYER_HALF_H;
                    s.vel.y      = 0.f;
                    s.vel.x      = 0.f;
                    s.isGrounded = true;
                    std::cout << "[Ladder] id=" << player.GetPlayerId()
                              << " bottom-exit groundY=" << gr.groundY << "\n";
                }
            }

            // 상단 이탈: 사다리 상단(+PLAYER_HALF_H) 초과 + 상승 중
            // 클라이언트 이탈 임계값(ladderMaxY + extents.y)과 일치시켜 oscillation 방지
            if (s.isOnLadder && vy > 0.f && s.pos.y >= s.ladderMaxY + PLAYER_HALF_H)
            {
                s.isOnLadder = false;
                s.vel.y      = 0.f;
                s.vel.x      = 0.f;
                std::cout << "[Ladder] id=" << player.GetPlayerId() << " top-exit\n";
                return;  // fall-through 없이 종료 — 다음 틱에서 정상 물리 적용
            }

            if (s.isOnLadder) return;  // 이탈 안 했으면 물리 종료
            // 하단 이탈 후 동일 프레임 일반 물리로 fall-through
        }
    }

    // 1. dt 결정 (클라이언트 deltaTime 유효 시 사용, 범위 초과 시 서버 기본값)
    const float useDt = (cmd.deltaTime > 0.f && cmd.deltaTime < 0.1f) ? cmd.deltaTime : dt;

    // drop-through 타이머 갱신
    if (cmd.dropThrough)
        s.dropThroughTimer = 0.25f;
    else if (s.dropThroughTimer > 0.f)
        s.dropThroughTimer = std::max(0.f, s.dropThroughTimer - useDt);

    const bool ignoreFloating = s.dropThroughTimer > 0.f;

    // 2. 수평 방향 결정
    float moveX = 0.f;
    if (cmd.moveLeft)  moveX = -1.f;
    if (cmd.moveRight) moveX =  1.f;

    s.isMoving = (moveX != 0.f);
    if      (moveX < 0.f)             s.dir = MoveDir::Left;
    else if (moveX > 0.f)             s.dir = MoveDir::Right;
    if (cmd.faceDir != MoveDir::None) s.dir = cmd.faceDir;

    // 3. 이동 전 접지 판정 — floating ground는 하강/정지 시에만 지면으로 인정
    bool ignoreFloatingPremove = ignoreFloating || (s.vel.y > 0.f);
    GroundResult ground = CheckGround(s.pos, ignoreFloatingPremove);
    s.isGrounded = ground.hit;

    // 4. 접지 시 하방 속도 고정 (지면 밀착 유지)
    if (s.isGrounded && s.vel.y <= 0.f)
        s.vel.y = -2.f;

    // 5. 점프 (접지 상태에서만 허용)
    // [DEBUG_JUMP] 점프 커맨드 수신 로그 — 제거 시 아래 블록 전체 삭제
    if (cmd.jump)
        std::cout << "[DEBUG_JUMP] jump cmd received: isGrounded=" << s.isGrounded
                  << " vel.y=" << s.vel.y
                  << " pos=(" << s.pos.x << "," << s.pos.y << ")"
                  << " seq=" << cmd.inputSeq << "\n";
    // [DEBUG_JUMP] 거부 시 발 위치 상세
    if (cmd.jump && !s.isGrounded)
    {
        GroundResult gr = CheckGround(s.pos);
        float footY = s.pos.y - PLAYER_HALF_H;
        std::cout << "[DEBUG_JUMP] jump REJECTED: footY=" << footY
                  << " groundY=" << gr.groundY
                  << " gap=" << (footY - gr.groundY)
                  << " seq=" << cmd.inputSeq << "\n";
    }
    // [DEBUG_JUMP] end
    if (cmd.jump && s.isGrounded)
    {
        // [DEBUG_JUMP] 점프 적용 로그 — 제거 시 아래 한 줄 삭제
        std::cout << "[DEBUG_JUMP] jump applied: vel.y " << s.vel.y << " -> " << JUMP_FORCE << "\n";
        // [DEBUG_JUMP] end
        s.vel.y     = JUMP_FORCE;
        s.isJumping = true;
        s.isGrounded = false;
    }

    // 6. 중력 (공중일 때만 적용)
    if (!s.isGrounded)
    {
        s.vel.y -= GRAVITY * useDt;
        if (s.vel.y < MAX_FALL_SPEED) s.vel.y = MAX_FALL_SPEED;
    }

    // ── 수평 이동 + 벽 충돌 해결 ─────────────────────────────────────────────
    s.pos.x += moveX * MOVE_SPEED * useDt;

    if (moveX != 0.f)
    {
        WallResult wall = CheckWall(s.pos, moveX);
        if (wall.hit)
        {
            // [DEBUG_WALL] 공중 벽 충돌 로그
            if (!s.isGrounded)
                std::cout << "[DEBUG_WALL] air wall hit: moveX=" << moveX
                          << " pos=(" << s.pos.x << "->" << wall.wallX << "," << s.pos.y << ")"
                          << " vel.y=" << s.vel.y << " seq=" << cmd.inputSeq << "\n";
            // [DEBUG_WALL] end
            s.pos.x = wall.wallX;
            s.vel.x = 0.f;
        }
    }

    // ── 수직 이동 + 지면/천장 충돌 해결 ─────────────────────────────────────
    const bool wasGrounded = s.isGrounded;  // [DEBUG_LAND] 착지 전환 감지용

    if (s.vel.y > 0.f)
    {
        // 상승 중: 한 번에 이동 후 천장 충돌 체크
        s.pos.y += s.vel.y * useDt;
        CeilResult ceil = CheckCeiling(s.pos);
        if (ceil.hit)
        {
            s.pos.y = ceil.ceilY - PLAYER_HALF_H;
            s.vel.y = 0.f;
        }
    }
    else
    {
        // 하강 또는 정지: GROUND_THRESHOLD 단위로 서브스텝 → 고속 낙하 관통 방지
        // useDt 최대 0.1s 시 최대 이동량 = 15×0.1 = 1.5 > GROUND_THRESHOLD(0.1) → 관통 가능
        const float subStepSize = GROUND_THRESHOLD * 0.9f;
        const float totalMove   = std::abs(s.vel.y * useDt);
        const int   numSteps    = (totalMove > 0.f)
                                  ? std::max(1, (int)std::ceil(totalMove / subStepSize))
                                  : 1;
        const float subDt = useDt / numSteps;

        for (int i = 0; i < numSteps; i++)
        {
            s.pos.y += s.vel.y * subDt;
            GroundResult gr = CheckGround(s.pos, ignoreFloating);
            if (gr.hit)
            {
                s.pos.y      = gr.groundY + PLAYER_HALF_H;
                s.vel.y      = -2.f;
                s.isGrounded = true;
                s.isJumping  = false;
                // [DEBUG_LAND] 착지 순간만 출력 (grounded 유지 중엔 생략)
                if (!wasGrounded)
                    std::cout << "[DEBUG_LAND] 착지: seq=" << cmd.inputSeq
                              << " groundY=" << gr.groundY
                              << " snapY=" << s.pos.y
                              << " ignoreFloating=" << ignoreFloating
                              << " dropTimer=" << s.dropThroughTimer << "\n";
                // [DEBUG_LAND] end
                break;
            }
        }
    }

    // 7. lastInputSeq 갱신 (Reconciliation 기준점)
    if (cmd.inputSeq != 0)
        s.lastInputSeq = cmd.inputSeq;
}
