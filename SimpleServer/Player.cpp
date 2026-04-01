#include "Player.h"

Player::Player(uint64_t id, std::string name)
    : _playerId(id), _name(std::move(name))
{
}

void Player::EnqueueInput(const InputCmd& cmd)
{
    std::lock_guard<std::mutex> lock(_inputMutex);
    // 버퍼 초과 시 가장 오래된 입력 제거
    if (static_cast<int>(_inputBuffer.size()) >= MAX_INPUT_BUFFER)
        _inputBuffer.pop_front();
    _inputBuffer.push_back(cmd);
}

InputCmd Player::DequeueInput()
{
    std::lock_guard<std::mutex> lock(_inputMutex);
    if (_inputBuffer.empty())
        return InputCmd{};  // 버퍼 소진 시 zero-input 반환 (이동 없음)

    InputCmd cmd = _inputBuffer.front();
    _inputBuffer.pop_front();
    return cmd;
}

bool Player::HasInput() const
{
    std::lock_guard<std::mutex> lock(_inputMutex);
    return !_inputBuffer.empty();
}
