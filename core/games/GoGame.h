#pragma once

#include "../GameInterface.h"
#include <vector>
#include <unordered_set>
#include <set>
#include <deque>

class GoGame : public GameInterface {
public:
    GoGame(int board_size, float dir_epsilon = 0.25f, float dir_alpha = 0.03f);
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
    std::pair<float, float> GetDirichletParams() const override;

private:
    int board_size_;
    float dir_epsilon_;
    float dir_alpha_;
    int action_size_;
    Player current_player_;
    std::vector<Player> board_;
    std::deque<std::vector<Player>> history_;
    
    // 用于打劫检测的哈希记录
    std::set<std::string> previous_states_;
    
    // 连续 pass 次数
    int pass_count_;
    
    // 内部方法
    bool IsLegalMove(int x, int y, Player player) const;
    bool HasLiberty(int x, int y, Player player, std::vector<bool>& visited) const;
    void RemoveDeadStones(int x, int y, Player opponent);
    std::string GetBoardHash() const;
    float CalculateScore() const; // Tromp-Taylor 规则算分
};
