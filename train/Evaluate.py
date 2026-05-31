import sys
import os
import argparse
import logging

sys.path.append(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
sys.path.append(os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))), 'lib'))

import core_engine
from nn.ModelManager import ModelManager
import yaml

class Evaluator:
    def __init__(self, config_path, mode="go"):
        self.mode = mode
        with open(config_path, 'r') as f:
            self.config = yaml.safe_load(f)
            
        self.best_model = ModelManager(self.config)
        self.candidate_model = ModelManager(self.config)

    def load_models(self, best_path, candidate_path):
        self.best_model.load_model(best_path)
        self.candidate_model.load_model(candidate_path)

    def play_match(self, current_best_is_black=True):
        dir_eps = self.config['mcts_params']['dirichlet_epsilon']
        dir_alpha = self.config['mcts_params']['dirichlet_alpha']
        
        if self.mode == 'go':
            komi = self.config['env_params'].get('komi', 6)
            max_moves = self.config['env_params'].get('max_moves', 120)
            game = core_engine.GoGame(self.config['env_params']['board_size'], dir_eps, dir_alpha, komi, max_moves)
        elif self.mode == 'gomoku':
            game = core_engine.GomokuGame(self.config['env_params']['board_size'], dir_eps, dir_alpha)
        else:
            raise NotImplementedError(f"Mode {self.mode} is not supported yet.")
        
        if current_best_is_black:
            black_eval = self.best_model.evaluate
            white_eval = self.candidate_model.evaluate
        else:
            black_eval = self.candidate_model.evaluate
            white_eval = self.best_model.evaluate

        num_sim = self.config['mcts_params']['num_simulations']
        c_puct = self.config['mcts_params']['c_puct']
        batch_size = self.config['mcts_params'].get('virtual_loss_batch_size', 8)
        virtual_loss = self.config['mcts_params'].get('virtual_loss', 3.0)
        black_mcts = core_engine.MCTS(black_eval, num_sim, c_puct, batch_size, virtual_loss)
        white_mcts = core_engine.MCTS(white_eval, num_sim, c_puct, batch_size, virtual_loss)

        eval_temp_threshold = self.config['train_params'].get('eval_temp_threshold', 4)
        move_num = 0
        move_sequence = []
        
        while True:
            current_player = game.GetCurrentPlayer()
            temp = 1.0 if move_num < eval_temp_threshold else 0.0
            if current_player == core_engine.Player.Black:
                action_prob = black_mcts.GetActionProb(game, temp) 
            else:
                action_prob = white_mcts.GetActionProb(game, temp)

            import numpy as np
            if temp == 0.0:
                action = np.argmax(action_prob)
            else:
                action_prob = np.array(action_prob, dtype=np.float64)
                action_prob /= np.sum(action_prob) 
                action = np.random.choice(len(action_prob), p=action_prob)
            move_sequence.append(int(action))
            game.Step(int(action))
            black_mcts.UpdateWithMove(int(action))
            white_mcts.UpdateWithMove(int(action))
            move_num += 1

            is_ended, reward = game.GetGameEnded()
            if is_ended:
                if reward > 0.5:
                    winner = current_player 
                elif reward < -0.5:
                    winner = core_engine.Player.White if current_player == core_engine.Player.Black else core_engine.Player.Black
                else:
                    return 0, move_sequence, core_engine.Player.NonePlayer

                if current_best_is_black:
                    return (1 if winner == core_engine.Player.White else -1), move_sequence, winner
                else:
                    return (1 if winner == core_engine.Player.Black else -1), move_sequence, winner

    def evaluate(self, num_games=10, iteration=0, iteration_start_time=None):
        logging.info(f"Evaluating candidate model for {num_games} games...")
        candidate_wins = 0
        for i in range(num_games):
            current_best_is_black = (i % 2 == 0)
            res, move_sequence, winner = self.play_match(current_best_is_black)
            if res == 1:
                candidate_wins += 1
            logging.info(f"Game {i+1}: candidate win: {res == 1}")
            
            render_interval = self.config['train_params'].get('render_interval', 10)
            if (i + 1) % render_interval == 0:
                from train.Render import render_game
                render_game(move_sequence, self.config, i + 1, winner, self.mode, iteration, iteration_start_time)
        
        win_rate = candidate_wins / num_games
        logging.info(f"Candidate win rate: {win_rate:.2f}")
        
        if win_rate >= self.config['train_params'].get('eval_win_rate_threshold', 0.55):
            logging.warning("Candidate model is better. Replacing best model.")
            return True
        else:
            logging.info("Candidate model rejected.")
            return False

if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument('--mode', type=str, default='go', help='Game mode: go, gomoku, or others')
    args = parser.parse_args()
    
    root_dir = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
    config_file = os.path.join(root_dir, 'config', f'{args.mode}.yaml')
    evaluator = Evaluator(config_file, args.mode)
