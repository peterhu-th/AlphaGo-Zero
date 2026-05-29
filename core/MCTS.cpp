#include "MCTS.h"
#include <cmath>
#include <algorithm>
#include <iostream>
#include <random>

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
    for (size_t i = 0; i < legal_moves.size(); ++i) {
        if (legal_moves[i] == 1 && children_.find(i) == children_.end()) {
            children_[i] = new Node(this, action_probs[i]);
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
        std::vector<float> features = game->GetStateFeatures();
        auto [action_probs, root_value] = eval_fn_(features);
        root_->expand(game->GetLegalMoves(), action_probs);
    }

    if (root_->is_expanded() && temp > 0.0f) {
        auto [dir_eps, dir_alpha] = game->GetDirichletParams();
        std::mt19937 gen(std::random_device{}());
        std::gamma_distribution<float> gamma(dir_alpha, 1.0f);
        
        std::vector<float> noise;
        float noise_sum = 0.0f;
        for (size_t i = 0; i < root_->children_.size(); ++i) {
            float n = gamma(gen);
            noise.push_back(n);
            noise_sum += n;
        }

        int idx = 0;
        for (auto& pair : root_->children_) {
            float n = noise[idx] / noise_sum;
            pair.second->prior_prob_ = (1 - dir_eps) * pair.second->prior_prob_ + dir_eps * n;
            idx++;
        }
    }

    // 运行多次模拟
    for (int i = 0; i < num_simulations_; ++i) {
        Node* node = root_;
        auto cloned_game = game->Clone();
        
        // 1. Select
        while (node->is_expanded()) {
            float max_u = -1e9f;
            int best_action = -1;
            Node* best_child = nullptr;
            for (auto& pair : node->children_) {
                float u = pair.second->get_u(c_puct_);
                if (u > max_u) {
                    max_u = u;
                    best_action = pair.first;
                    best_child = pair.second;
                }
            }
            if (best_child) {
                node = best_child;
                cloned_game->Step(best_action);
            } else {
                break;
            }
        }

        // 2. Evaluate & Expand
        auto [is_ended, reward] = cloned_game->GetGameEnded();
        float value = 0.0f;
        if (!is_ended) {
            auto eval_result = eval_fn_(cloned_game->GetStateFeatures());
            std::vector<float> child_probs = eval_result.first;
            value = eval_result.second;
            node->expand(cloned_game->GetLegalMoves(), child_probs);
            value = -value;
        } else {
            value = -reward;
        }

        // 3. Backup
        while (node != nullptr) {
            node->update(value);
            value = -value;
            node = node->parent_;
        }
    }

    int action_size = game->GetActionSize();
    std::vector<float> probs(action_size, 0.0f);

    int total_visits = 0;
    for (auto& pair : root_->children_) {
        total_visits += pair.second->visit_count_;
    }

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
        float sum = 0.0f;
        for (auto& pair : root_->children_) {
            probs[pair.first] = std::pow(pair.second->visit_count_, 1.0f / temp);
            sum += probs[pair.first];
        }
        if (sum > 0) {
            for (int i = 0; i < action_size; ++i) probs[i] /= sum;
        }
    }

    return probs;
}

void MCTS::Search(GameInterface* game) {
    // 逻辑已被移入 GetActionProb
}
