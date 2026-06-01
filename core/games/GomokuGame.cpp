#include "GomokuGame.h"
#include <sstream>
#include <iostream>

bool GomokuGame::lookup_table_initialized_ = false;
uint8_t GomokuGame::lookup_table_[19683];

void GomokuGame::InitLookupTable() {
    if (lookup_table_initialized_) return;
    
    for (int i = 0; i < 19683; ++i) {
        int temp = i;
        int A[9];
        for (int j = 0; j < 9; ++j) {
            A[j] = temp % 3;
            temp /= 3;
        }
        
        if (A[4] != 1) {
            lookup_table_[i] = 0;
            continue;
        }
        
        int l = 4, r = 4;
        while (l > 0 && A[l - 1] == 1) l--;
        while (r < 8 && A[r + 1] == 1) r++;
        int contig = r - l + 1;
        
        uint8_t overline = 0;
        uint8_t five = 0;
        uint8_t fours = 0;
        uint8_t live_threes = 0;
        
        if (contig >= 6) {
            overline = 1;
        } else if (contig == 5) {
            five = 1;
        } else {
            int four_spots = 0;
            for (int j = 0; j < 9; ++j) {
                if (A[j] == 0) {
                    A[j] = 1;
                    int sl = 4, sr = 4;
                    while (sl > 0 && A[sl - 1] == 1) sl--;
                    while (sr < 8 && A[sr + 1] == 1) sr++;
                    if (sr - sl + 1 == 5) four_spots++;
                    A[j] = 0;
                }
            }
            
            bool is_live_four = (contig == 4 && l > 0 && A[l-1] == 0 && r < 8 && A[r+1] == 0);
            if (is_live_four) fours = 1;
            else fours = four_spots;
            
            int live_three_spots = 0;
            for (int j = 0; j < 9; ++j) {
                if (A[j] == 0) {
                    A[j] = 1;
                    int sl = 4, sr = 4;
                    while (sl > 0 && A[sl - 1] == 1) sl--;
                    while (sr < 8 && A[sr + 1] == 1) sr++;
                    if (sr - sl + 1 == 4 && sl > 0 && A[sl-1] == 0 && sr < 8 && A[sr+1] == 0) {
                        live_three_spots++;
                    }
                    A[j] = 0;
                }
            }
            if (live_three_spots > 0) live_threes = 1;
        }
        
        uint8_t val = 0;
        val |= (live_threes & 0x03);
        val |= ((fours & 0x03) << 2);
        val |= ((five & 0x01) << 4);
        val |= ((overline & 0x01) << 5);
        lookup_table_[i] = val;
    }
    
    lookup_table_initialized_ = true;
}

GomokuGame::GomokuGame(int board_size, float dir_epsilon, float dir_alpha)
    : board_size_(board_size), dir_epsilon_(dir_epsilon), dir_alpha_(dir_alpha), action_size_(board_size * board_size) {
    InitLookupTable();
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
    forbidden_points_.clear();
}

std::vector<int> GomokuGame::GetLegalMoves() const {
    std::vector<int> legal_moves(action_size_, 0);
    for (int i = 0; i < action_size_; ++i) {
        if (board_[i] == Player::NonePlayer) {
            if (current_player_ == Player::Black && forbidden_points_.count(i)) {
                legal_moves[i] = 0;
            } else {
                legal_moves[i] = 1;
            }
        }
    }
    return legal_moves;
}

Player GomokuGame::Step(int action) {
    if (action < 0 || action >= action_size_ || board_[action] != Player::NonePlayer) {
        // 禁手不传递给 MCTS 搜索
        return current_player_;
    }
    board_[action] = current_player_;
    
    UpdateForbiddenPoints(action);
    
    current_player_ = (current_player_ == Player::Black) ? Player::White : Player::Black;
    return current_player_;
}

bool GomokuGame::CheckForbidden(int x, int y) const {
    int total_live_threes = 0;
    int total_fours = 0;
    bool has_overline = false;

    int dx[] = {1, 0, 1, 1};
    int dy[] = {0, 1, 1, -1};

    for (int d = 0; d < 4; ++d) {
        int index = 0;
        int p = 1;
        for (int i = -4; i <= 4; ++i) {
            int nx = x + i * dx[d];
            int ny = y + i * dy[d];
            int val = 0;
            if (nx >= 0 && nx < board_size_ && ny >= 0 && ny < board_size_) {
                Player cell = board_[nx * board_size_ + ny];
                if (i == 0) val = 1;
                else if (cell == Player::Black) val = 1;
                else if (cell == Player::White) val = 2;
            } else {
                val = 2;
            }
            index += val * p;
            p *= 3;
        }

        uint8_t state = lookup_table_[index];
        if (state & (1 << 4)) return false;
        if (state & (1 << 5)) has_overline = true;

        total_live_threes += (state & 0x03);
        total_fours += ((state >> 2) & 0x03);
    }

    if (has_overline) return true;
    if (total_fours >= 2) return true;
    if (total_live_threes >= 2) return true;

    return false;
}

void GomokuGame::UpdateForbiddenPoints(int move) {
    int mx = move / board_size_;
    int my = move % board_size_;
    
    forbidden_points_.erase(move);
    
    int dx[] = {1, 0, 1, 1};
    int dy[] = {0, 1, 1, -1};
    
    for (int d = 0; d < 4; ++d) {
        for (int i = -4; i <= 4; ++i) {
            if (i == 0) continue;
            int nx = mx + i * dx[d];
            int ny = my + i * dy[d];
            if (nx >= 0 && nx < board_size_ && ny >= 0 && ny < board_size_) {
                int p = nx * board_size_ + ny;
                if (board_[p] == Player::NonePlayer) {
                    if (CheckForbidden(nx, ny)) {
                        forbidden_points_.insert(p);
                    } else {
                        forbidden_points_.erase(p);
                    }
                }
            }
        }
    }
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
    // channels: current player pieces、opponent player pieces、color indicator、legal moves (empty & not forbidden for current player)
    std::vector<float> features(4 * board_size_ * board_size_, 0.0f);
    
    int offset_self = 0;
    int offset_opp = board_size_ * board_size_;
    int offset_color = 2 * board_size_ * board_size_;
    int offset_legal = 3 * board_size_ * board_size_;
    
    Player opponent = (current_player_ == Player::Black) ? Player::White : Player::Black;
    
    for (int i = 0; i < board_size_ * board_size_; ++i) {
        // 本方棋子
        if (board_[i] == current_player_) features[offset_self + i] = 1.0f;
        // 对方棋子
        else if (board_[i] == opponent) features[offset_opp + i] = 1.0f;
        
        // 当前颜色标识 (黑棋为 1.0，白棋为 -1.0)
        features[offset_color + i] = (current_player_ == Player::Black) ? 1.0f : -1.0f;
        
        // 合法落子标识
        if (board_[i] == Player::NonePlayer) {
            if (current_player_ == Player::Black && forbidden_points_.count(i)) {
                features[offset_legal + i] = 0.0f; // 禁手
            } else {
                features[offset_legal + i] = 1.0f; // 合法空位
            }
        } else {
            features[offset_legal + i] = 0.0f; // 已经被占用的格子
        }
    }
    return features;
}

Player GomokuGame::GetCurrentPlayer() const {
    return current_player_;
}

std::unique_ptr<GameInterface> GomokuGame::Clone() const {
    return std::make_unique<GomokuGame>(*this);
}

std::vector<int> GomokuGame::GetBoard() const {
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
