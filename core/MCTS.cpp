#include "MCTS.h"
#include <cmath>
#include <algorithm>
#include <iostream>
#include <random>
#include <limits>
#include <memory>


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
            sum += action_probs[i];
        }
    }
    if (sum < 1e-8) sum = 1.0f;
    for (size_t i = 0; i < legal_moves.size(); ++i) {
        if (legal_moves[i] == 1) {
            children_[i] = new Node(this, action_probs[i] / sum);
        }
    }
}


bool Node::is_expanded() const {
    return !children_.empty();
}


MCTS::MCTS(EvalCallback eval_fn, int num_simulations, float c_puct, int virtual_loss_batch_size, float virtual_loss)
    : root_(new Node(nullptr, 1.0f)),
      eval_fn_(std::move(eval_fn)), 
      num_simulations_(num_simulations), 
      c_puct_(c_puct), 
      batch_size_(virtual_loss_batch_size),
      virtual_loss_(virtual_loss) {}

      
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


void MCTS::SearchBatched(GameInterface* root_game) {
    std::vector<Node*> leaf_nodes;
    std::vector<std::vector<float>> leaf_features;
    std::vector<std::vector<int>> legal_moves_batch;
    std::vector<std::pair<bool, float>> terminal_states;
    std::vector<std::unique_ptr<GameInterface>> leaf_games;

    int current_simulations = 0;
    while (current_simulations < num_simulations_) {
        int collect_count = 0;
        
        leaf_nodes.clear();
        leaf_features.clear();
        legal_moves_batch.clear();
        terminal_states.clear();
        leaf_games.clear();

        while (collect_count < batch_size_ && current_simulations < num_simulations_) {
            std::unique_ptr<GameInterface> cloned_game = root_game->Clone();
            Node* node = root_;
            
            // 1. Select
            while (node->is_expanded()) {
                float max_u = -std::numeric_limits<float>::infinity();
                int best_action = -1;
                Node* best_child = nullptr;

                for (auto& pair : node->children_) {
                    int action = pair.first;
                    Node* child = pair.second;
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

            auto end_state = cloned_game->GetGameEnded();
            bool is_terminal = end_state.first;

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
                // Terminal state, backpropagate immediately without NN evaluation
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

        if (collect_count > 0) {
            // Evaluate all collected leaves in a single batch
            auto eval_result = eval_fn_(leaf_features);
            const std::vector<std::vector<float>>& batch_action_probs = eval_result.first;
            const std::vector<float>& batch_values = eval_result.second;

            for (int i = 0; i < collect_count; ++i) {
                Node* node = leaf_nodes[i];
                
                Node* vl_curr = node;
                while (vl_curr != nullptr) {
                    vl_curr->visit_count_ -= 1;
                    vl_curr->value_sum_ += virtual_loss_;
                    vl_curr = vl_curr->parent_;
                }

                // 获取神经网络的真实 value 并回传
                float value = batch_values[i];
                if (!node->is_expanded()) {
                    node->expand(legal_moves_batch[i], batch_action_probs[i]);
                }

                // 真实的 value 必须交替符号，因为博弈双方视角对立
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
        
        int idx = 0;
        for (auto& pair : root_->children_) {
            pair.second->prior_prob_ = (1.0f - epsilon) * pair.second->prior_prob_ + epsilon * (noise[idx] / sum);
            idx++;
        }
    }

    SearchBatched(game);

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
            probs[pair.first] = std::pow(pair.second->visit_count_ / max_visit, 1.0f / temp);
            sum += probs[pair.first];
        }
        for (auto& pair : root_->children_) {
            probs[pair.first] /= sum;
        }
    }

    return probs;
}
