#pragma once

#include "../GameInterface.h"
#include <vector>
#include <unordered_set>
#include <deque>
#include <stdexcept>

class GoGame : public GameInterface {
public:
    GoGame(int board_size, float dir_epsilon = 0.25f, float dir_alpha = 0.03f, float komi = 3.5f, int max_moves = 60);
    ~GoGame() override = default;

    std::pair<int, int> GetBoardSize() const override;
    int GetActionSize() const override;
    void Reset() override;
    std::vector<int> GetLegalMoves() const override;
    Player Step(int action) override;
    std::pair<bool, float> GetGameEnded() const override;
    std::vector<float> GetStateFeatures() const override;
    Player GetCurrentPlayer() const override;
    std::unique_ptr<GameInterface> Clone() const override;
    std::string ToString() const override;
    std::vector<int> GetBoard() const override;
    std::pair<float, float> GetDirichletParams() const override;

private:
    int board_size_;
    float dir_epsilon_;
    float dir_alpha_;
    float komi_;
    int action_size_;
    int max_moves_;
    int move_count_;
    Player current_player_;
    std::vector<Player> board_;
    std::deque<std::vector<Player>> history_;
    
    // Zobrist Hashing
    std::unordered_set<uint64_t> previous_states_;
    uint64_t current_hash_;
    
    // 连续 pass 次数
    int pass_count_;
    
    // 内部方法
    bool IsLegalMove(int x, int y, Player player) const;
    bool HasLiberty(int x, int y, Player player) const;
    void RemoveDeadStones(int x, int y, Player opponent);
    float CalculateScore() const; // Tromp-Taylor flood-fill
};
