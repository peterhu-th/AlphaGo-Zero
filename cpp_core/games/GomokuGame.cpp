#include "GomokuGame.h"
#include <sstream>
#include <iostream>

GomokuGame::GomokuGame(int board_size, float dir_epsilon, float dir_alpha)
    : board_size_(board_size), dir_epsilon_(dir_epsilon), dir_alpha_(dir_alpha), action_size_(board_size * board_size) {
    Reset();
}

std::pair<int, int> GomokuGame::GetBoardSize() const {
    return {board_size_, board_size_};
}

int GomokuGame::GetActionSize() const {
    return action_size_;
}

void GomokuGame::Reset() {
    board_.assign(board_size_ * board_size_, Player::NonePlayer);
    current_player_ = Player::Black;
}

std::vector<int> GomokuGame::GetLegalMoves() const {
    std::vector<int> legal_moves(action_size_, 0);
    for (int i = 0; i < action_size_; ++i) {
        if (board_[i] == Player::NonePlayer) {
            legal_moves[i] = 1;
        }
    }
    return legal_moves;
}

Player GomokuGame::Step(int action) {
    if (action < 0 || action >= action_size_ || board_[action] != Player::NonePlayer) {
        // Illegal move is not expected to be chosen by MCTS.
        return current_player_;
    }
    board_[action] = current_player_;
    current_player_ = (current_player_ == Player::Black) ? Player::White : Player::Black;
    return current_player_;
}

bool GomokuGame::CheckWin(int x, int y, Player player) const {
    int dx[] = {1, 0, 1, 1};
    int dy[] = {0, 1, 1, -1};
    
    for (int d = 0; d < 4; ++d) {
        int count = 1;
        
        // 正向
        int nx = x + dx[d];
        int ny = y + dy[d];
        while (nx >= 0 && nx < board_size_ && ny >= 0 && ny < board_size_ && board_[nx * board_size_ + ny] == player) {
            count++;
            nx += dx[d];
            ny += dy[d];
        }
        
        // 反向
        nx = x - dx[d];
        ny = y - dy[d];
        while (nx >= 0 && nx < board_size_ && ny >= 0 && ny < board_size_ && board_[nx * board_size_ + ny] == player) {
            count++;
            nx -= dx[d];
            ny -= dy[d];
        }
        
        if (count >= 5) {
            return true;
        }
    }
    return false;
}

std::pair<bool, float> GomokuGame::GetGameEnded() const {
    bool is_full = true;
    for (int i = 0; i < board_size_; ++i) {
        for (int j = 0; j < board_size_; ++j) {
            Player p = board_[i * board_size_ + j];
            if (p != Player::NonePlayer) {
                if (CheckWin(i, j, p)) {
                    // 由于走步后玩家会切换，所以如果 p (走完这步的人) 赢了
                    // 那么相对于当前的 current_player_ 来说，结果是输了。
                    // 返回的 float value 是当前 current_player_ 视角的价值。
                    // 如果 p 是当前的上一手，他赢了，说明当前回合的人输了 (-1)。
                    return {true, -1.0f};
                }
            } else {
                is_full = false;
            }
        }
    }
    
    if (is_full) {
        return {true, 0.0f};
    }
    
    return {false, 0.0f};
}

std::vector<float> GomokuGame::GetStateFeatures() const {
    // 3 channels: current player, opponent player, color indicator
    std::vector<float> features(3 * board_size_ * board_size_, 0.0f);
    int offset_self = 0;
    int offset_opp = board_size_ * board_size_;
    int offset_color = 2 * board_size_ * board_size_;
    
    Player opponent = (current_player_ == Player::Black) ? Player::White : Player::Black;
    
    for (int i = 0; i < board_size_ * board_size_; ++i) {
        if (board_[i] == current_player_) features[offset_self + i] = 1.0f;
        else if (board_[i] == opponent) features[offset_opp + i] = 1.0f;
        
        features[offset_color + i] = (current_player_ == Player::Black) ? 1.0f : -1.0f;
    }
    
    return features;
}

Player GomokuGame::GetCurrentPlayer() const {
    return current_player_;
}

std::unique_ptr<GameInterface> GomokuGame::Clone() const {
    return std::make_unique<GomokuGame>(*this);
}

std::string GomokuGame::ToString() const {
    std::stringstream ss;
    for (int i = 0; i < board_size_; ++i) {
        for (int j = 0; j < board_size_; ++j) {
            if (board_[i * board_size_ + j] == Player::Black) ss << "X ";
            else if (board_[i * board_size_ + j] == Player::White) ss << "O ";
            else ss << ". ";
        }
        ss << "\n";
    }
    return ss.str();
}

std::pair<float, float> GomokuGame::GetDirichletParams() const {
    return {dir_epsilon_, dir_alpha_};
}
