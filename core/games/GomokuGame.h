#pragma once

#include "../GameInterface.h"
#include <vector>
#include <set>

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
};
