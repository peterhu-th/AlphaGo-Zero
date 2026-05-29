#pragma once

#include "GameInterface.h"
#include <vector>
#include <unordered_map>
#include <memory>
#include <functional>

class Node {
public:
    Node(Node* parent, float prior_prob);
    ~Node();

    float get_u(float c_puct) const;
    void update(float v);
    void expand(const std::vector<int>& legal_moves, const std::vector<float>& action_probs);
    bool is_expanded() const;

    Node* parent_;
    std::unordered_map<int, Node*> children_;
    
    int visit_count_;
    float value_sum_;
    float prior_prob_;
};

// 评估函数类型：传入状态特征，返回 (动作概率分布, 局面价值)
using EvalCallback = std::function<std::pair<std::vector<float>, float>(const std::vector<float>&)>;

class MCTS {
public:
    MCTS(EvalCallback eval_fn, int num_simulations, float c_puct);
    ~MCTS();

    // 获取给定状态的动作概率分布，temp 为温度参数
    std::vector<float> GetActionProb(GameInterface* game, float temp = 1.0f);

    // 树复用：随着游戏的进行，向下移动根节点，释放无关的树枝
    void UpdateWithMove(int last_action);

private:
    void Search(GameInterface* game);
    
    EvalCallback eval_fn_;
    int num_simulations_;
    float c_puct_;
    
    Node* root_;
};
