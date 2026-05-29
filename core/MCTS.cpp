#include "MCTS.h"
#include <cmath>
#include <algorithm>
#include <iostream>
#include <random>
#include <limits>

Node::Node(Node* parent, float prior_prob) 
    : parent_(parent), visit_count_(0), value_sum_(0.0f), prior_prob_(prior_prob) {}

Node::~Node() {
    for (auto& pair : children_) {
        delete pair.second;
    }
}

float Node::get_u(float c_puct) const {
    float q_value = visit_count_ > 0 ? value_sum_ / visit_count_ : 0.0f;
    float u_value = c_puct * prior_prob_ * std::sqrt(parent_->visit_count_) / (1.0f + visit_count_);
    return q_value + u_value;
}

void Node::update(float v) {
    visit_count_++;
    value_sum_ += v;
}

void Node::expand(const std::vector<int>& legal_moves, const std::vector<float>& action_probs) {
    float sum = 0.0f;
    for (size_t i = 0; i < legal_moves.size(); ++i) {
        if (legal_moves[i] == 1) {
            children_[i] = new Node(this, action_probs[i]);
        }
    }
    if (sum < 1e-8) sum = 1.0f;
    for (size_t i = 0; i < legal_moves.size(); ++i) {
        if (legal_moves[i] == 1 && children_.find(i) == children_.end()) {
            children_[i] = new Node(this, action_probs[i] / sum);
        }
    }
}

bool Node::is_expanded() const {
    return !children_.empty();
}

MCTS::MCTS(EvalCallback eval_fn, int num_simulations, float c_puct)
    : eval_fn_(std::move(eval_fn)), num_simulations_(num_simulations), c_puct_(c_puct),
      root_(new Node(nullptr, 1.0f)) {}

MCTS::~MCTS() {
    if (root_) delete root_;
}

void MCTS::UpdateWithMove(int last_action) {
    if (root_->children_.find(last_action) != root_->children_.end()) {
        Node* new_root = root_->children_[last_action];
        root_->children_.erase(last_action);
        delete root_;
        root_ = new_root;
        root_->parent_ = nullptr;
    } else {
        delete root_;
        root_ = new Node(nullptr, 1.0f);
    }
}

std::vector<float> MCTS::GetActionProb(GameInterface* game, float temp) {
    if (!root_->is_expanded()) {
        Search(game);
    }
    auto dir_params = game->GetDirichletParams();
    float epsilon = dir_params.first;
    float alpha = dir_params.second;

    if (epsilon > 0.0f && !root_->children_.empty()) {
        std::gamma_distribution<float> gamma(alpha, 1.0f);
        // 使用 thread_local 保证极速且线程安全的随机数生成
        static thread_local std::mt19937 rng(std::random_device{}());
        
        float sum = 0.0f;
        std::vector<float> noise;
        noise.reserve(root_->children_.size());
        
        for (size_t i = 0; i < root_->children_.size(); ++i) {
            float n = gamma(rng);
            noise.push_back(n);
            sum += n;
        }
        
        if (sum < 1e-8f) sum = 1.0f;
        
        int idx = 0;
        for (auto& pair : root_->children_) {
            // 将噪声混合进原始先验概率中
            pair.second->prior_prob_ = (1.0f - epsilon) * pair.second->prior_prob_ + epsilon * (noise[idx] / sum);
            idx++;
        }
    }

    // 运行多次模拟
    for (int i = 0; i < num_simulations_; ++i) {
        Search(game);
    }

    int action_size = game->GetActionSize();
    std::vector<float> probs(action_size, 0.0f);

    if (temp == 0.0f) {
        int best_action = -1;
        int max_visit = -1;
        for (auto& pair : root_->children_) {
            if (pair.second->visit_count_ > max_visit) {
                max_visit = pair.second->visit_count_;
                best_action = pair.first;
            }
        }
        if (best_action != -1) {
            probs[best_action] = 1.0f;
        }
    } else {
        float max_visit = 0.0f;
        for (auto& pair : root_->children_) {
            if (pair.second->visit_count_ > max_visit) {
                max_visit = static_cast<float>(pair.second->visit_count_);
            }
        }
        if (max_visit == 0.0f) max_visit = 1.0f;
        float sum = 0.0f;
        for (auto& pair : root_->children_) {
            // 将底数锁定在 [0, 1] 区间
            probs[pair.first] = std::pow(pair.second->visit_count_ / max_visit, 1.0f / temp);
            sum += probs[pair.first];
        }
        for (auto& pair : root_->children_) {
            probs[pair.first] /= sum;
        }
    }

    return probs;
}

void MCTS::Search(GameInterface* game) {
    std::unique_ptr<GameInterface> cloned_game = game->Clone();
    Node* node = root_;
    // 1. Select
    // 从根节点开始，利用 PUCT 公式一直向下寻找，直到遇到一个未展开的叶子节点
    while (node->is_expanded()) {
        float max_u = -std::numeric_limits<float>::infinity();
        int best_action = -1;
        Node* best_child = nullptr;

        // 遍历所有合法的子节点，寻找 PUCT 值最大的那个动作
        for (auto& pair : node->children_) {
            int action = pair.first;
            Node* child = pair.second;
            
            // 计算当前子节点的 UCB 上界 (Q值 + U值)
            float u = child->get_u(c_puct_);

            if (u > max_u) {
                max_u = u;
                best_action = action;
                best_child = child;
            }
        }
        if (best_action != -1) {
            cloned_game->Step(best_action);
            node = best_child;
        } else {
            break; 
        }
    }

    // 2. Evaluate & Expand
    auto end_state = cloned_game->GetGameEnded();
    bool is_terminal = end_state.first;
    float value = 0.0f;

    if (!is_terminal) {
        // eval_fn_ 返回值：(先验概率分布向量, 局面胜率预测)
        auto eval_result = eval_fn_(cloned_game->GetStateFeatures());
        const std::vector<float>& action_probs = eval_result.first;
        value = eval_result.second;

        // 利用神经网络输出的先验概率，扩展当前这个叶子节点
        node->expand(cloned_game->GetLegalMoves(), action_probs);
    } else {
        // 如果游戏已经在叶子节点分出胜负，直接使用终局真实奖励
        value = end_state.second;
    }
    value = -value; 

    while (node != nullptr) {
        node->update(value);
        value = -value;         // 每向上一层，视角切换一次
        node = node->parent_;
    }
}
