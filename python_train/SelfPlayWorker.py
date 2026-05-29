import sys
import os

sys.path.append(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
sys.path.append(os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))), 'lib'))

import core_engine
from python_nn.ModelManager import ModelManager

class SelfPlayWorker:
    def __init__(self, config, model_manager):
        self.config = config
        self.model_manager = model_manager
        self.board_size = config['env_params']['board_size']
        self.num_simulations = config['mcts_params']['num_simulations']
        self.c_puct = config['mcts_params']['c_puct']

    def play_one_game(self):
        game = core_engine.GoGame(self.board_size)
        # MCTS 不再需要传入 Dir 噪声参数，C++ 会向 Game 获取
        mcts = core_engine.MCTS(self.model_manager.evaluate, self.num_simulations, self.c_puct)

        states = []
        search_probs = []
        players = []

        step_count = 0
        while True:
            temp = 1.0 if step_count < 30 else 0.0
            
            # GetActionProb 会在有必要时施加 Dirichlet 噪声 (由 C++ 内部判断)
            # 为了训练自对弈中的探索，GetActionProb 第二个参数可以控制是否施加噪声
            # 此处我们把该功能移到了 C++ 端，C++ 端只在 root 展开时自动加噪声
            action_prob = mcts.GetActionProb(game, temp)

            state_features = game.GetStateFeatures()
            current_player = game.GetCurrentPlayer()

            states.append(state_features)
            search_probs.append(action_prob)
            players.append(current_player)

            import numpy as np
            action_prob = np.array(action_prob, dtype=np.float64)
            prob_sum = np.sum(action_prob)
            if prob_sum > 0:
                action_prob /= prob_sum
            else:
                action_prob = np.ones(len(action_prob)) / len(action_prob)
            
            action = np.random.choice(len(action_prob), p=action_prob)

            game.Step(int(action))
            mcts.UpdateWithMove(int(action))
            
            step_count += 1

            is_ended, reward = game.GetGameEnded()
            if is_ended:
                winner = None
                if reward > 0.5:
                    winner = core_engine.Player.Black
                elif reward < -0.5:
                    winner = core_engine.Player.White
                else:
                    winner = core_engine.Player.NonePlayer
                
                game_data = []
                for s, prob, p in zip(states, search_probs, players):
                    if winner == core_engine.Player.NonePlayer:
                        z = 0.0
                    elif winner == p:
                        z = 1.0
                    else:
                        z = -1.0
                    game_data.append((s, prob, z))
                
                return game_data
