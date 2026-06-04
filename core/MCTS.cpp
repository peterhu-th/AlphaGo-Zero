#include "MCTS.h"
#include <cmath>
#include <algorithm>
#include <iostream>
#include <random>
#include <limits>


Node::Node(Node* parent, float prior_prob) 
    : parent_(parent), visit_count_(0), value_sum_(0.0f), prior_prob_(prior_prob) {}

float Node::get_u(float c_puct, float sqrt_parent_visit) const {
    float q_value = visit_count_ > 0 ? value_sum_ / static_cast<float>(visit_count_) : 0.0f;
    float u_value = c_puct * prior_prob_ * sqrt_parent_visit / (1.0f + visit_count_);
    return q_value + u_value;
}

void Node::update(float v) {
    visit_count_++;
    value_sum_ += v;
}

void Node::expand(const std::vector<int>& legal_moves, const std::vector<float>& action_probs) {
    float sum = 0.0f;
    int legal_count = 0;
    for (size_t i = 0; i < legal_moves.size(); ++i) {
        if (legal_moves[i] == 1) {
            sum += action_probs[i];
            legal_count++;
        }
    }
    
    children_.reserve(legal_count);
    
    if (sum < 1e-8f) {
        float uniform_prob = 1.0f / static_cast<float>(legal_count);
        for (size_t i = 0; i < legal_moves.size(); ++i) {
            if (legal_moves[i] == 1) {
                children_.emplace_back(i, std::make_unique<Node>(this, uniform_prob));
            }
        }
    } else {
        for (size_t i = 0; i < legal_moves.size(); ++i) {
            if (legal_moves[i] == 1) {
                children_.emplace_back(i, std::make_unique<Node>(this, action_probs[i] / sum));
            }
        }
    }
}

bool Node::is_expanded() const {
    return !children_.empty();
}


MCTS::MCTS(EvalCallback eval_fn, int num_simulations, float c_puct, int virtual_loss_batch_size, float virtual_loss)
    : root_(std::make_unique<Node>(nullptr, 1.0f)),
      eval_fn_(std::move(eval_fn)), 
      num_simulations_(num_simulations), 
      c_puct_(c_puct), 
      batch_size_(virtual_loss_batch_size),
      virtual_loss_(virtual_loss) {}

void MCTS::UpdateWithMove(int last_action) {
    auto it = std::find_if(root_->children_.begin(), root_->children_.end(),
                           [last_action](const auto& pair) { return pair.first == last_action; });
    
    if (it != root_->children_.end()) {
        std::unique_ptr<Node> new_root = std::move(it->second);
        new_root->parent_ = nullptr;
        root_ = std::move(new_root);
    } else {
        root_ = std::make_unique<Node>(nullptr, 1.0f);
    }
}

void MCTS::SearchBatched(GameInterface* root_game) {
    std::vector<Node*> leaf_nodes;
    std::vector<std::vector<float>> leaf_features;
    std::vector<std::vector<int>> legal_moves_batch;
    std::vector<std::pair<bool, float>> terminal_states;
    std::vector<std::unique_ptr<GameInterface>> leaf_games;

    leaf_nodes.reserve(batch_size_);
    leaf_features.reserve(batch_size_);
    legal_moves_batch.reserve(batch_size_);
    terminal_states.reserve(batch_size_);

    int current_simulations = 0;
    while (current_simulations < num_simulations_) {
        int collect_count = 0;
        
        leaf_nodes.clear();
        leaf_features.clear();
        legal_moves_batch.clear();
        terminal_states.clear();

        while (collect_count < batch_size_ && current_simulations < num_simulations_) {
            std::unique_ptr<GameInterface> cloned_game = root_game->Clone();
            Node* node = root_.get();
            
            // Select: PUCT 算法选择最优节点，迭代到未展开的叶子节点
            while (node->is_expanded()) {
                float max_u = -std::numeric_limits<float>::infinity();
                int best_action = -1;
                Node* best_child = nullptr;

                float sqrt_parent_visit = std::sqrt(static_cast<float>(node->visit_count_));

                for (const auto& pair : node->children_) {
                    float u = pair.second->get_u(c_puct_, sqrt_parent_visit);
                    if (u > max_u) {
                        max_u = u;
                        best_action = pair.first;
                        best_child = pair.second.get();
                    }
                }
                
                if (best_action != -1) {
                    cloned_game->Step(best_action);
                    node = best_child;
                } else {
                    break;
                }
            }

            // 检查游戏是否结束
            auto end_state = cloned_game->GetGameEnded();
            bool is_terminal = end_state.first;
            
            // 虚拟损失，降低当前路径的吸引力
            if (!is_terminal) {
                Node* vl_curr = node;
                while (vl_curr != nullptr) {
                    vl_curr->visit_count_ += 1;
                    vl_curr->value_sum_ -= virtual_loss_; 
                    vl_curr = vl_curr->parent_;
                }

                leaf_nodes.push_back(node);
                leaf_features.push_back(cloned_game->GetStateFeatures());
                legal_moves_batch.push_back(cloned_game->GetLegalMoves());
                terminal_states.push_back(end_state);
                collect_count++;
            } else {
                // 终止态立即反向传播
                float value = -end_state.second;
                Node* curr = node;
                while (curr != nullptr) {
                    curr->update(value);
                    value = -value;
                    curr = curr->parent_;
                }
            }
            current_simulations++;
        }

        // Estimate: 运行神经网络评估
        if (collect_count > 0) {
            auto eval_result = eval_fn_(leaf_features);
            const auto& batch_action_probs = eval_result.first;
            const auto& batch_values = eval_result.second;

            for (int i = 0; i < collect_count; ++i) {
                Node* node = leaf_nodes[i];
                
                // 消除虚拟损失
                Node* vl_curr = node;
                while (vl_curr != nullptr) {
                    vl_curr->visit_count_ -= 1;
                    vl_curr->value_sum_ += virtual_loss_;
                    vl_curr = vl_curr->parent_;
                }

                float value = batch_values[i];
                if (!node->is_expanded()) {
                    node->expand(legal_moves_batch[i], batch_action_probs[i]);
                }

                // Backpropagation: 反向传播真实价值
                value = -value;
                Node* curr = node;
                while (curr != nullptr) {
                    curr->update(value);
                    value = -value;
                    curr = curr->parent_;
                }
            }
        }
    }
}

std::vector<float> MCTS::GetActionProb(GameInterface* game, float temp) {
    if (!root_->is_expanded()) {
        SearchBatched(game);
    }
    auto dir_params = game->GetDirichletParams();
    float epsilon = dir_params.first;
    float alpha = dir_params.second;

    // 添加狄利克雷噪声 (仅对根节点)
    if (epsilon > 0.0f && !root_->children_.empty()) {
        std::gamma_distribution<float> gamma(alpha, 1.0f);
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
        
        for (size_t i = 0; i < root_->children_.size(); ++i) {
            root_->children_[i].second->prior_prob_ = 
                (1.0f - epsilon) * root_->children_[i].second->prior_prob_ + epsilon * (noise[i] / sum);
        }
    }

    SearchBatched(game);

    int action_size = game->GetActionSize();
    std::vector<float> probs(action_size, 0.0f);

    if (temp == 0.0f) {
        int best_action = -1;
        int max_visit = -1;
        for (const auto& pair : root_->children_) {
            if (pair.second->visit_count_ > max_visit) {
                max_visit = pair.second->visit_count_;
                best_action = pair.first;
            }
        }
        if (best_action != -1) {
            probs[best_action] = 1.0f;
        }
    } else {
        int max_visit = 0;
        for (const auto& pair : root_->children_) {
            if (pair.second->visit_count_ > max_visit) {
                max_visit = pair.second->visit_count_;
            }
        }
        
        float max_v = max_visit > 0 ? static_cast<float>(max_visit) : 1.0f;
        float sum = 0.0f;
        float inv_temp = 1.0f / temp;
        
        for (const auto& pair : root_->children_) {
            probs[pair.first] = std::pow(static_cast<float>(pair.second->visit_count_) / max_v, inv_temp);
            sum += probs[pair.first];
        }
        
        if (sum > 0.0f) {
            for (auto& pair : root_->children_) {
                probs[pair.first] /= sum;
            }
        }
    }

    return probs;
}