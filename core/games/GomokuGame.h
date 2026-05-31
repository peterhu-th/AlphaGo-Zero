#pragma once

#include "../GameInterface.h"
#include <vector>
#include <set>
#include <cstdint>

class GomokuGame : public GameInterface {
public:
    GomokuGame(int board_size, float dir_epsilon = 0.25f, float dir_alpha = 0.3f);
    ~GomokuGame() override = default;

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
    
    bool CheckWin(int x, int y, Player player) const;

    // 禁手优化：查表法与增量缓存
    std::set<int> forbidden_points_;
    void UpdateForbiddenPoints(int move);
    bool CheckForbidden(int x, int y) const;

    static void InitLookupTable();
    static bool lookup_table_initialized_;
    // 预计算表，大小 3^9 = 19683
    // bit 0-1: live_threes (活三数量)
    // bit 2-3: fours (四数量)
    // bit 4: five (是否连五)
    // bit 5: overline (是否长连)
    static uint8_t lookup_table_[19683];
};
