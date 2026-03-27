#pragma once
#include "Session.h"
#include "Player.h"
#include <memory>

class ClientSession : public Session
{
public:
    using Session::Session;

    // SetPlayer는 반드시 Start() 호출 전(OnConnected 콜백 내)에만 호출해야 함
    // Start() 이후에는 워커 스레드에서 GetPlayer()가 호출될 수 있으므로 동시 쓰기 금지
    void                    SetPlayer(std::shared_ptr<Player> player) { _player = std::move(player); }
    std::shared_ptr<Player> GetPlayer() const                         { return _player; }

private:
    std::shared_ptr<Player> _player;
};
