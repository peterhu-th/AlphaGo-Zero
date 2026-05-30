#pragma once

#include <vector>
#include <map>
#include <functional>
#include "GameInterface.h"

using EvalCallback = std::function<std::pair<std::vector<std::vector<float>>, std::vector<float>>(const std::vector<std::vector<float>>&)>;

class Node {
public:
    Node* parent_;
    std::map<int, Node*> children_;
    int visit_count_;
    float value_sum_;
    float prior_prob_;

    Node(Node* parent, float prior_prob);
    ~Node();

    float get_u(float c_puct) const;
    void update(float v);
    void expand(const std::vector<int>& legal_moves, const std::vector<float>& action_probs);
    bool is_expanded() const;
};

class MCTS {
public:
    MCTS(EvalCallback eval_fn, int num_simulations, float c_puct, int virtual_loss_batch_size = 8, float virtual_loss = 3.0f);
    ~MCTS();

    std::vector<float> GetActionProb(GameInterface* game, float temp = 1.0f);
    void UpdateWithMove(int last_action);

private:
    Node* root_;
    EvalCallback eval_fn_;
    int num_simulations_;
    float c_puct_;
    int batch_size_;
    float virtual_loss_;

    void SearchBatched(GameInterface* root_game);
};
