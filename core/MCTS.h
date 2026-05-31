#pragma once

#include <vector>
#include <memory>
#include <functional>
#include <utility>
#include "GameInterface.h"

using EvalCallback = std::function<std::pair<std::vector<std::vector<float>>, std::vector<float>>(const std::vector<std::vector<float>>&)>;

class Node {
public:
    Node* parent_;
    std::vector<std::pair<int, std::unique_ptr<Node>>> children_;
    int visit_count_;
    float value_sum_;
    float prior_prob_;

    Node(Node* parent, float prior_prob);
    ~Node() = default;

    float get_u(float c_puct, float sqrt_parent_visit) const;
    void update(float v);
    void expand(const std::vector<int>& legal_moves, const std::vector<float>& action_probs);
    bool is_expanded() const;
};

class MCTS {
public:
    MCTS(EvalCallback eval_fn, int num_simulations, float c_puct, int virtual_loss_batch_size = 8, float virtual_loss = 3.0f);
    ~MCTS() = default;

    std::vector<float> GetActionProb(GameInterface* game, float temp = 1.0f);
    void UpdateWithMove(int last_action);

private:
    std::unique_ptr<Node> root_;
    EvalCallback eval_fn_;
    int num_simulations_;
    float c_puct_;
    int batch_size_;
    float virtual_loss_;

    void SearchBatched(GameInterface* root_game);
};