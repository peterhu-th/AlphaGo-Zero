#include "GoGame.h"
#include <iostream>
#include <sstream>
#include <queue>
#include <algorithm>

GoGame::GoGame(int board_size) : board_size_(board_size), action_size_(board_size * board_size + 1) {
    Reset();
}

std::pair<int, int> GoGame::GetBoardSize() const {
    return {board_size_, board_size_};
}

int GoGame::GetActionSize() const {
    return action_size_;
}

void GoGame::Reset() {
    board_.assign(board_size_ * board_size_, Player::NonePlayer);
    current_player_ = Player::Black;
    previous_states_.clear();
    previous_states_.insert(GetBoardHash());
    pass_count_ = 0;
}

Player GoGame::GetCurrentPlayer() const {
    return current_player_;
}

std::vector<int> GoGame::GetLegalMoves() const {
    std::vector<int> legal_moves(action_size_, 0);
    // 检查棋盘上每个点
    for (int i = 0; i < board_size_ * board_size_; ++i) {
        int x = i / board_size_;
        int y = i % board_size_;
        if (IsLegalMove(x, y, current_player_)) {
            legal_moves[i] = 1;
        }
    }
    // PASS 总是合法的
    legal_moves[action_size_ - 1] = 1;
    return legal_moves;
}

Player GoGame::Step(int action) {
    if (action == action_size_ - 1) {
        // 执行 PASS
        pass_count_++;
        current_player_ = (current_player_ == Player::Black) ? Player::White : Player::Black;
        previous_states_.insert(GetBoardHash());
        return current_player_;
    }

    int x = action / board_size_;
    int y = action % board_size_;

    if (!IsLegalMove(x, y, current_player_)) {
        // 非法动作，这里简化处理，直接 PASS
        pass_count_++;
        current_player_ = (current_player_ == Player::Black) ? Player::White : Player::Black;
        return current_player_;
    }

    pass_count_ = 0;
    board_[action] = current_player_;
    
    // 吃子逻辑
    Player opponent = (current_player_ == Player::Black) ? Player::White : Player::Black;
    int dx[] = {-1, 1, 0, 0};
    int dy[] = {0, 0, -1, 1};
    for (int i = 0; i < 4; ++i) {
        int nx = x + dx[i];
        int ny = y + dy[i];
        if (nx >= 0 && nx < board_size_ && ny >= 0 && ny < board_size_) {
            if (board_[nx * board_size_ + ny] == opponent) {
                std::vector<bool> visited(board_size_ * board_size_, false);
                if (!HasLiberty(nx, ny, opponent, visited)) {
                    RemoveDeadStones(nx, ny, opponent);
                }
            }
        }
    }

    previous_states_.insert(GetBoardHash());
    current_player_ = opponent;
    return current_player_;
}

bool GoGame::IsLegalMove(int x, int y, Player player) const {
    int idx = x * board_size_ + y;
    if (board_[idx] != Player::NonePlayer) return false;

    // 复制一份棋盘用于模拟落子
    std::vector<Player> temp_board = board_;
    temp_board[idx] = player;

    // 模拟吃子
    Player opponent = (player == Player::Black) ? Player::White : Player::Black;
    int dx[] = {-1, 1, 0, 0};
    int dy[] = {0, 0, -1, 1};
    bool captured_any = false;

    for (int i = 0; i < 4; ++i) {
        int nx = x + dx[i];
        int ny = y + dy[i];
        if (nx >= 0 && nx < board_size_ && ny >= 0 && ny < board_size_) {
            if (temp_board[nx * board_size_ + ny] == opponent) {
                // 判断对方这块棋是否还有气
                std::vector<bool> visited(board_size_ * board_size_, false);
                std::queue<std::pair<int, int>> q;
                q.push({nx, ny});
                visited[nx * board_size_ + ny] = true;
                bool has_liberty = false;
                std::vector<std::pair<int, int>> block;

                while (!q.empty()) {
                    auto [cx, cy] = q.front();
                    q.pop();
                    block.push_back({cx, cy});

                    for (int d = 0; d < 4; ++d) {
                        int nnx = cx + dx[d];
                        int nny = cy + dy[d];
                        if (nnx >= 0 && nnx < board_size_ && nny >= 0 && nny < board_size_) {
                            if (temp_board[nnx * board_size_ + nny] == Player::NonePlayer) {
                                has_liberty = true;
                            } else if (temp_board[nnx * board_size_ + nny] == opponent && !visited[nnx * board_size_ + nny]) {
                                visited[nnx * board_size_ + nny] = true;
                                q.push({nnx, nny});
                            }
                        }
                    }
                }

                if (!has_liberty) {
                    captured_any = true;
                    for (auto& p : block) {
                        temp_board[p.first * board_size_ + p.second] = Player::NonePlayer;
                    }
                }
            }
        }
    }

    // 检查自己落子后是否有气（自杀规则禁手）
    std::vector<bool> visited(board_size_ * board_size_, false);
    std::queue<std::pair<int, int>> q;
    q.push({x, y});
    visited[idx] = true;
    bool self_has_liberty = false;

    while (!q.empty()) {
        auto [cx, cy] = q.front();
        q.pop();

        for (int d = 0; d < 4; ++d) {
            int nnx = cx + dx[d];
            int nny = cy + dy[d];
            if (nnx >= 0 && nnx < board_size_ && nny >= 0 && nny < board_size_) {
                if (temp_board[nnx * board_size_ + nny] == Player::NonePlayer) {
                    self_has_liberty = true;
                    break;
                } else if (temp_board[nnx * board_size_ + nny] == player && !visited[nnx * board_size_ + nny]) {
                    visited[nnx * board_size_ + nny] = true;
                    q.push({nnx, nny});
                }
            }
        }
        if (self_has_liberty) break;
    }

    if (!self_has_liberty) return false; // 自杀动作

    // 打劫规则检查
    std::string new_hash;
    for (int i = 0; i < board_size_ * board_size_; ++i) {
        if (temp_board[i] == Player::Black) new_hash += "B";
        else if (temp_board[i] == Player::White) new_hash += "W";
        else new_hash += ".";
    }

    if (previous_states_.find(new_hash) != previous_states_.end()) {
        return false; // 重复局面（打劫禁手）
    }

    return true;
}

bool GoGame::HasLiberty(int x, int y, Player player, std::vector<bool>& visited) const {
    int dx[] = {-1, 1, 0, 0};
    int dy[] = {0, 0, -1, 1};
    
    std::queue<std::pair<int, int>> q;
    q.push({x, y});
    visited[x * board_size_ + y] = true;

    while (!q.empty()) {
        auto [cx, cy] = q.front();
        q.pop();

        for (int i = 0; i < 4; ++i) {
            int nx = cx + dx[i];
            int ny = cy + dy[i];
            if (nx >= 0 && nx < board_size_ && ny >= 0 && ny < board_size_) {
                if (board_[nx * board_size_ + ny] == Player::NonePlayer) return true;
                if (board_[nx * board_size_ + ny] == player && !visited[nx * board_size_ + ny]) {
                    visited[nx * board_size_ + ny] = true;
                    q.push({nx, ny});
                }
            }
        }
    }
    return false;
}

void GoGame::RemoveDeadStones(int x, int y, Player opponent) {
    int dx[] = {-1, 1, 0, 0};
    int dy[] = {0, 0, -1, 1};
    
    std::vector<bool> visited(board_size_ * board_size_, false);
    std::queue<std::pair<int, int>> q;
    q.push({x, y});
    visited[x * board_size_ + y] = true;

    while (!q.empty()) {
        auto [cx, cy] = q.front();
        q.pop();
        board_[cx * board_size_ + cy] = Player::NonePlayer;

        for (int i = 0; i < 4; ++i) {
            int nx = cx + dx[i];
            int ny = cy + dy[i];
            if (nx >= 0 && nx < board_size_ && ny >= 0 && ny < board_size_) {
                if (board_[nx * board_size_ + ny] == opponent && !visited[nx * board_size_ + ny]) {
                    visited[nx * board_size_ + ny] = true;
                    q.push({nx, ny});
                }
            }
        }
    }
}

std::string GoGame::GetBoardHash() const {
    std::string hash;
    for (int i = 0; i < board_size_ * board_size_; ++i) {
        if (board_[i] == Player::Black) hash += "B";
        else if (board_[i] == Player::White) hash += "W";
        else hash += ".";
    }
    return hash;
}

std::pair<bool, float> GoGame::GetGameEnded() const {
    if (pass_count_ >= 2) {
        float score = CalculateScore();
        if (score > 0) return {true, 1.0f}; // 黑胜
        if (score < 0) return {true, -1.0f}; // 白胜
        return {true, 0.0f}; // 平局
    }
    return {false, 0.0f};
}

float GoGame::CalculateScore() const {
    // 简化的 Tromp-Taylor 规则：计算占据和包围的空点
    int black_score = 0;
    int white_score = 0;
    
    // 省略复杂的连通分量染色，仅做最简单的占有计算。
    // 在真正的比赛中应当计算气或用flood-fill算地。
    for (int i = 0; i < board_size_ * board_size_; ++i) {
        if (board_[i] == Player::Black) black_score++;
        else if (board_[i] == Player::White) white_score++;
    }
    // 扣除贴目，如果是 9x9 可以是 7.5 或者 0。这里简化为 0 或者后续配置。
    return static_cast<float>(black_score - white_score);
}

std::vector<float> GoGame::GetStateFeatures() const {
    // 为了网络输入，生成特征平面，通道0: 当前玩家的子, 通道1: 对方玩家的子, 通道2: 当前玩家颜色
    std::vector<float> features(3 * board_size_ * board_size_, 0.0f);
    Player opponent = (current_player_ == Player::Black) ? Player::White : Player::Black;
    
    for (int i = 0; i < board_size_ * board_size_; ++i) {
        if (board_[i] == current_player_) features[i] = 1.0f;
        if (board_[i] == opponent) features[board_size_ * board_size_ + i] = 1.0f;
        features[2 * board_size_ * board_size_ + i] = (current_player_ == Player::Black) ? 1.0f : 0.0f;
    }
    return features;
}

std::unique_ptr<GameInterface> GoGame::Clone() const {
    auto clone = std::make_unique<GoGame>(board_size_);
    clone->board_ = this->board_;
    clone->current_player_ = this->current_player_;
    clone->previous_states_ = this->previous_states_;
    clone->pass_count_ = this->pass_count_;
    return clone;
}

std::string GoGame::ToString() const {
    std::stringstream ss;
    ss << "  ";
    for(int i=0; i<board_size_; ++i) ss << i << " ";
    ss << "\n";
    for (int x = 0; x < board_size_; ++x) {
        ss << x << " ";
        for (int y = 0; y < board_size_; ++y) {
            int idx = x * board_size_ + y;
            if (board_[idx] == Player::Black) ss << "X ";
            else if (board_[idx] == Player::White) ss << "O ";
            else ss << ". ";
        }
        ss << "\n";
    }
    return ss.str();
}

std::pair<float, float> GoGame::GetDirichletParams() const {
    return {0.25f, 0.03f};
}
