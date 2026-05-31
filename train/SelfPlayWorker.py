import sys
import os
import numpy as np
import logging

sys.path.append(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
sys.path.append(os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))), 'lib'))

import core_engine
from nn.ModelManager import ModelManager

class SelfPlayWorker:
    def __init__(self, config, model_manager, mode="go", no_epsilon=False):
        self.config = config
        self.model_manager = model_manager
        self.mode = mode
        self.no_epsilon = no_epsilon
        self.board_size = config['env_params']['board_size']
        self.num_simulations = config['mcts_params']['num_simulations']
        self.c_puct = config['mcts_params']['c_puct']
        self.temperature_threshold = config['mcts_params'].get('temperature_threshold', 30)


    def get_symmetries(self, state_flat, pi_flat, z, in_channels):
        board_size = self.board_size
        state = np.array(state_flat).reshape(in_channels, board_size, board_size)
        pi = np.array(pi_flat)
        
        if self.mode == 'go':
            pi_board = pi[:-1].reshape(board_size, board_size)
            pi_pass = pi[-1]
        elif self.mode == 'gomoku':
            pi_board = pi.reshape(board_size, board_size)
            pi_pass = None
        else:
            # 预留给其他棋类
            pi_board = pi.reshape(board_size, board_size)
            pi_pass = None
            
        symm_data = []
        
        for i in range(4):
            s_rot = np.rot90(state, k=i, axes=(1, 2))
            p_rot = np.rot90(pi_board, k=i)
            
            for flip in [False, True]:
                if flip:
                    s_trans = np.flip(s_rot, axis=2)
                    p_trans = np.fliplr(p_rot)
                else:
                    s_trans = s_rot
                    p_trans = p_rot
                    
                if self.mode == 'go':
                    p_final = np.append(p_trans.flatten(), pi_pass)
                else:
                    p_final = p_trans.flatten()
                    
                symm_data.append((s_trans.flatten().tolist(), p_final.tolist(), z))
                
        return symm_data


    def play_one_game(self):
        dir_eps = 0.0 if self.no_epsilon else self.config['mcts_params']['dirichlet_epsilon']
        dir_alpha = self.config['mcts_params']['dirichlet_alpha']
        if self.mode == 'go':
            komi = self.config['env_params'].get('komi', 7.5)
            max_moves = self.config['env_params'].get('max_moves', 60)
            game = core_engine.GoGame(self.board_size, dir_eps, dir_alpha, komi, max_moves)
        elif self.mode == 'gomoku':
            game = core_engine.GomokuGame(self.board_size, dir_eps, dir_alpha)
        else:
            raise NotImplementedError(f"Mode {self.mode} is not supported yet.")

        batch_size = self.config['mcts_params'].get('virtual_loss_batch_size', 8)
        virtual_loss = self.config['mcts_params'].get('virtual_loss', 3.0)
        mcts = core_engine.MCTS(self.model_manager.evaluate, self.num_simulations, self.c_puct, batch_size, virtual_loss)
        

        states = []
        search_probs = []
        players = []
        move_sequence = []

        step_count = 0
        while True:
            temp = 1.0 if step_count < self.temperature_threshold else 0.0
            
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
            move_sequence.append(int(action))

            game.Step(int(action))
            mcts.UpdateWithMove(int(action))
            
            step_count += 1

            is_ended, reward = game.GetGameEnded()
            if is_ended:
                winner = None
                current_player = game.GetCurrentPlayer()
                if reward > 0.5:
                    winner = current_player
                elif reward < -0.5:
                    winner = core_engine.Player.White if current_player == core_engine.Player.Black else core_engine.Player.Black
                else:
                    winner = core_engine.Player.NonePlayer
                
                game_data = []
                in_channels = self.config['env_params']['in_channels']

                for s, prob, p in zip(states, search_probs, players):
                    if winner == core_engine.Player.NonePlayer:
                        z = 0.0
                    elif winner == p:
                        z = 1.0
                    else:
                        z = -1.0
                    symm_samples = self.get_symmetries(s, prob, z, in_channels)
                    game_data.extend(symm_samples)
                
                return game_data, step_count, winner, move_sequence
