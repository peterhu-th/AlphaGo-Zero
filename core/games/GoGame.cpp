#include "GoGame.h"
#include <iostream>
#include <sstream>
#include <algorithm>
#include <random>
#include <mutex>

namespace {
    uint64_t zobrist_table[361][2];
    uint64_t zobrist_black_to_move;

    thread_local int visited_[361] = {0};
    thread_local int bfs_generation_ = 0;
    thread_local int bfs_queue_[361];
    
    inline void IncrementBfs() {
        if (bfs_generation_ > 2000000000) {
            bfs_generation_ = 0;
            std::fill(visited_, visited_ + 361, 0);
        }
        bfs_generation_++;
    }

    void InitZobrist() {
    static std::once_flag flag;
    std::call_once(flag, []() {
        std::mt19937_64 rng(12345);
        for (int i = 0; i < 361; ++i) {
            zobrist_table[i][0] = rng(); // 黑
            zobrist_table[i][1] = rng(); // 白
        }
        zobrist_black_to_move = rng();
    });
}
}

GoGame::GoGame(int board_size, float dir_epsilon, float dir_alpha, float komi, int max_moves) : 
    board_size_(board_size), 
    dir_epsilon_(dir_epsilon), 
    dir_alpha_(dir_alpha), 
    komi_(komi),
    action_size_(board_size * board_size + 1),
    max_moves_(max_moves),
    move_count_(0)
{
    InitZobrist();
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
    current_hash_ = zobrist_black_to_move;
    
    previous_states_.clear();
    previous_states_.insert(current_hash_);
    history_.clear();
    history_.push_back(board_);
    pass_count_ = 0;
    move_count_ = 0;
}

Player GoGame::GetCurrentPlayer() const {
    return current_player_;
}

std::vector<int> GoGame::GetLegalMoves() const {
    std::vector<int> legal_moves(action_size_, 0);
    // 遍历棋盘每个点
    for (int i = 0; i < board_size_ * board_size_; ++i) {
        int x = i / board_size_;
        int y = i % board_size_;
        if (IsLegalMove(x, y, current_player_)) {
            legal_moves[i] = 1;
        }
    }
    legal_moves[action_size_ - 1] = 1;
    return legal_moves;
}

Player GoGame::Step(int action) {
    move_count_++;
    if (action == action_size_ - 1) {
        pass_count_++;
        current_player_ = (current_player_ == Player::Black) ? Player::White : Player::Black;
        current_hash_ ^= zobrist_black_to_move;
        
        previous_states_.insert(current_hash_);
        history_.push_back(board_);
        if (history_.size() > 4) history_.pop_front();
        return current_player_;
    }

    int x = action / board_size_;
    int y = action % board_size_;

    if (!IsLegalMove(x, y, current_player_)) {
        throw std::runtime_error("Illegal move!");
    }

    pass_count_ = 0;
    
    int idx = x * board_size_ + y;
    board_[idx] = current_player_;
    
    int p_idx = (current_player_ == Player::Black) ? 0 : 1;
    current_hash_ ^= zobrist_table[idx][p_idx];
    
    // 吃子逻辑
    Player opponent = (current_player_ == Player::Black) ? Player::White : Player::Black;
    int dx[] = {-1, 1, 0, 0};
    int dy[] = {0, 0, -1, 1};
    for (int i = 0; i < 4; ++i) {
        int nx = x + dx[i];
        int ny = y + dy[i];
        if (nx >= 0 && nx < board_size_ && ny >= 0 && ny < board_size_) {
            if (board_[nx * board_size_ + ny] == opponent) {
                if (!HasLiberty(nx, ny, opponent)) {
                    RemoveDeadStones(nx, ny, opponent);
                }
            }
        }
    }

    // 切换玩家
    current_player_ = opponent;
    current_hash_ ^= zobrist_black_to_move;

    previous_states_.insert(current_hash_);
    history_.push_back(board_);
    if (history_.size() > 4) history_.pop_front();
    
    return current_player_;
}

bool GoGame::IsLegalMove(int x, int y, Player player) const {
    int idx = x * board_size_ + y;
    if (board_[idx] != Player::NonePlayer) return false;

    // 复制一份棋盘用于模拟落子
    Player temp_board[361];
    std::copy(board_.begin(), board_.end(), temp_board);
    temp_board[idx] = player;

    uint64_t next_hash = current_hash_;
    int p_idx = (player == Player::Black) ? 0 : 1;
    next_hash ^= zobrist_table[idx][p_idx];

    Player opponent = (player == Player::Black) ? Player::White : Player::Black;
    int opp_idx = (opponent == Player::Black) ? 0 : 1;
    
    IncrementBfs();
    int dx[] = {-1, 1, 0, 0};
    int dy[] = {0, 0, -1, 1};

    // 模拟吃子
    for (int i = 0; i < 4; ++i) {
        int nx = x + dx[i];
        int ny = y + dy[i];
        if (nx >= 0 && nx < board_size_ && ny >= 0 && ny < board_size_) {
            int nidx = nx * board_size_ + ny;
            if (temp_board[nidx] == opponent && visited_[nidx] != bfs_generation_) {
                int head = 0, tail = 0;
                bfs_queue_[tail++] = nidx;
                visited_[nidx] = bfs_generation_;
                
                bool has_liberty = false;
                
                int block_arr[361];
                int block_size = 0;

                while (head < tail) {
                    int cidx = bfs_queue_[head++];
                    block_arr[block_size++] = cidx;

                    int cx = cidx / board_size_;
                    int cy = cidx % board_size_;

                    for (int d = 0; d < 4; ++d) {
                        int nnx = cx + dx[d];
                        int nny = cy + dy[d];
                        if (nnx >= 0 && nnx < board_size_ && nny >= 0 && nny < board_size_) {
                            int nnidx = nnx * board_size_ + nny;
                            if (temp_board[nnidx] == Player::NonePlayer) {
                                has_liberty = true;
                            } else if (temp_board[nnidx] == opponent && visited_[nnidx] != bfs_generation_) {
                                visited_[nnidx] = bfs_generation_;
                                bfs_queue_[tail++] = nnidx;
                            }
                        }
                    }
                }

                if (!has_liberty) {
                    for (int k = 0; k < block_size; ++k) {
                        int bidx = block_arr[k];
                        temp_board[bidx] = Player::NonePlayer;
                        next_hash ^= zobrist_table[bidx][opp_idx];
                    }
                }
            }
        }
    }

    // 检查自己落子后是否有气（自杀规则禁手）
    IncrementBfs();
    int head = 0, tail = 0;
    bfs_queue_[tail++] = idx;
    visited_[idx] = bfs_generation_;
    bool self_has_liberty = false;

    while (head < tail) {
        int cidx = bfs_queue_[head++];
        int cx = cidx / board_size_;
        int cy = cidx % board_size_;

        for (int d = 0; d < 4; ++d) {
            int nnx = cx + dx[d];
            int nny = cy + dy[d];
            if (nnx >= 0 && nnx < board_size_ && nny >= 0 && nny < board_size_) {
                int nnidx = nnx * board_size_ + nny;
                if (temp_board[nnidx] == Player::NonePlayer) {
                    self_has_liberty = true;
                    break;
                } else if (temp_board[nnidx] == player && visited_[nnidx] != bfs_generation_) {
                    visited_[nnidx] = bfs_generation_;
                    bfs_queue_[tail++] = nnidx;
                }
            }
        }
        if (self_has_liberty) break;
    }

    if (!self_has_liberty) return false;

    next_hash ^= zobrist_black_to_move;

    // 打劫规则检查
    if (previous_states_.find(next_hash) != previous_states_.end()) {
        return false;
    }

    return true;
}

bool GoGame::HasLiberty(int x, int y, Player player) const {
    int dx[] = {-1, 1, 0, 0};
    int dy[] = {0, 0, -1, 1};
    
    IncrementBfs();
    int head = 0, tail = 0;
    int start_idx = x * board_size_ + y;
    
    bfs_queue_[tail++] = start_idx;
    visited_[start_idx] = bfs_generation_;

    while (head < tail) {
        int cidx = bfs_queue_[head++];
        int cx = cidx / board_size_;
        int cy = cidx % board_size_;

        for (int i = 0; i < 4; ++i) {
            int nx = cx + dx[i];
            int ny = cy + dy[i];
            if (nx >= 0 && nx < board_size_ && ny >= 0 && ny < board_size_) {
                int nidx = nx * board_size_ + ny;
                if (board_[nidx] == Player::NonePlayer) return true;
                if (board_[nidx] == player && visited_[nidx] != bfs_generation_) {
                    visited_[nidx] = bfs_generation_;
                    bfs_queue_[tail++] = nidx;
                }
            }
        }
    }
    return false;
}

void GoGame::RemoveDeadStones(int x, int y, Player opponent) {
    int dx[] = {-1, 1, 0, 0};
    int dy[] = {0, 0, -1, 1};
    int opp_idx = (opponent == Player::Black) ? 0 : 1;
    
    IncrementBfs();
    int head = 0, tail = 0;
    int start_idx = x * board_size_ + y;
    
    bfs_queue_[tail++] = start_idx;
    visited_[start_idx] = bfs_generation_;

    while (head < tail) {
        int cidx = bfs_queue_[head++];
        
        // Remove stone and update hash
        board_[cidx] = Player::NonePlayer;
        current_hash_ ^= zobrist_table[cidx][opp_idx];

        int cx = cidx / board_size_;
        int cy = cidx % board_size_;

        for (int i = 0; i < 4; ++i) {
            int nx = cx + dx[i];
            int ny = cy + dy[i];
            if (nx >= 0 && nx < board_size_ && ny >= 0 && ny < board_size_) {
                int nidx = nx * board_size_ + ny;
                if (board_[nidx] == opponent && visited_[nidx] != bfs_generation_) {
                    visited_[nidx] = bfs_generation_;
                    bfs_queue_[tail++] = nidx;
                }
            }
        }
    }
}

std::pair<bool, float> GoGame::GetGameEnded() const {
    if (pass_count_ >= 2 || move_count_ >= max_moves_) {
        float score = CalculateScore();
        float reward = 0.0f;
        
        if (score > 0) reward = 1.0f;           // 黑胜
        else if (score < 0) reward = -1.0f;     // 白胜

        if (current_player_ == Player::White) {
            reward = -reward;
        }
        
        return {true, reward};
    }
    return {false, 0.0f};
}

float GoGame::CalculateScore() const {
    int black_score = 0;
    int white_score = 0;

    // 1. 计算盘面实子
    for (int i = 0; i < board_size_ * board_size_; ++i) {
        if (board_[i] == Player::Black) black_score++;
        else if (board_[i] == Player::White) white_score++;
    }

    // 2. Flood-fill 围空计算
    IncrementBfs();
    int dx[] = {-1, 1, 0, 0};
    int dy[] = {0, 0, -1, 1};

    for (int i = 0; i < board_size_ * board_size_; ++i) {
        if (board_[i] == Player::NonePlayer && visited_[i] != bfs_generation_) {
            int head = 0, tail = 0;
            bfs_queue_[tail++] = i;
            visited_[i] = bfs_generation_;

            int empty_count = 0;
            bool touches_black = false;
            bool touches_white = false;

            while (head < tail) {
                int cidx = bfs_queue_[head++];
                empty_count++;

                int cx = cidx / board_size_;
                int cy = cidx % board_size_;

                for (int d = 0; d < 4; ++d) {
                    int nx = cx + dx[d];
                    int ny = cy + dy[d];
                    if (nx >= 0 && nx < board_size_ && ny >= 0 && ny < board_size_) {
                        int nidx = nx * board_size_ + ny;
                        if (board_[nidx] == Player::Black) {
                            touches_black = true;
                        } else if (board_[nidx] == Player::White) {
                            touches_white = true;
                        } else if (board_[nidx] == Player::NonePlayer && visited_[nidx] != bfs_generation_) {
                            visited_[nidx] = bfs_generation_;
                            bfs_queue_[tail++] = nidx;
                        }
                    }
                }
            }

            if (touches_black && !touches_white) {
                black_score += empty_count;
            } else if (touches_white && !touches_black) {
                white_score += empty_count;
            }
        }
    }

    return static_cast<float>(black_score) - (static_cast<float>(white_score) + komi_);
}

std::vector<float> GoGame::GetStateFeatures() const {
    // 通道数为 9: 最近4步的当前玩家棋子(0-3), 最近4步的对手棋子(4-7), 当前玩家颜色(8)
    std::vector<float> features(9 * board_size_ * board_size_, 0.0f);
    Player opponent = (current_player_ == Player::Black) ? Player::White : Player::Black;

    int hist_size = history_.size();
    
    for (int t = 0; t < 4; ++t) {
        // 从最新的开始倒推
        int hist_idx = hist_size - 1 - t;
        if (hist_idx >= 0) {
            const auto& b = history_[hist_idx];
            int offset_current = t * board_size_ * board_size_;
            int offset_opponent = (4 + t) * board_size_ * board_size_;
            
            for (int i = 0; i < board_size_ * board_size_; ++i) {
                if (b[i] == current_player_) features[offset_current + i] = 1.0f;
                if (b[i] == opponent) features[offset_opponent + i] = 1.0f;
            }
        }
    }
    
    int offset_color = 8 * board_size_ * board_size_;
    for (int i = 0; i < board_size_ * board_size_; ++i) {
        features[offset_color + i] = (current_player_ == Player::Black) ? 1.0f : 0.0f;
    }
    return features;
}

std::unique_ptr<GameInterface> GoGame::Clone() const {
    auto clone = std::make_unique<GoGame>(board_size_, dir_epsilon_, dir_alpha_, komi_, max_moves_);
    clone->board_ = this->board_;
    clone->current_player_ = this->current_player_;
    clone->previous_states_ = this->previous_states_;
    clone->current_hash_ = this->current_hash_;
    clone->history_ = this->history_;
    clone->pass_count_ = this->pass_count_;
    clone->move_count_ = this->move_count_;
    return clone;
}

std::vector<int> GoGame::GetBoard() const {
    std::vector<int> res(board_size_ * board_size_, 0);
    for (int i = 0; i < board_size_ * board_size_; ++i) {
        if (board_[i] == Player::Black) {
            res[i] = 1;
        } else if (board_[i] == Player::White) {
            res[i] = 2;
        }
    }
    return res;
}

std::string GoGame::ToString() const {
    std::stringstream ss;
    for (int x = 0; x < board_size_; ++x) {
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
    return {dir_epsilon_, dir_alpha_};
}
