import sys
import os
import argparse

sys.path.append(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
sys.path.append(os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))), 'lib'))

import core_engine
from python_nn.ModelManager import ModelManager
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
            game = core_engine.GoGame(self.config['env_params']['board_size'], dir_eps, dir_alpha)
        else:
            game = core_engine.GomokuGame(self.config['env_params']['board_size'], dir_eps, dir_alpha)
        
        if current_best_is_black:
            black_eval = self.best_model.evaluate
            white_eval = self.candidate_model.evaluate
        else:
            black_eval = self.candidate_model.evaluate
            white_eval = self.best_model.evaluate

        num_sim = self.config['mcts_params']['num_simulations']
        c_puct = self.config['mcts_params']['c_puct']
        black_mcts = core_engine.MCTS(black_eval, num_sim, c_puct)
        white_mcts = core_engine.MCTS(white_eval, num_sim, c_puct)

        while True:
            current_player = game.GetCurrentPlayer()
            if current_player == core_engine.Player.Black:
                action_prob = black_mcts.GetActionProb(game, 0.0) 
            else:
                action_prob = white_mcts.GetActionProb(game, 0.0)

            import numpy as np
            action = np.argmax(action_prob)
            game.Step(int(action))
            black_mcts.UpdateWithMove(int(action))
            white_mcts.UpdateWithMove(int(action))

            is_ended, reward = game.GetGameEnded()
            if is_ended:
                if reward > 0.5:
                    winner = current_player 
                elif reward < -0.5:
                    winner = core_engine.Player.White if current_player == core_engine.Player.Black else core_engine.Player.Black
                else:
                    return 0

                if current_best_is_black:
                    return 1 if winner == core_engine.Player.White else -1
                else:
                    return 1 if winner == core_engine.Player.Black else -1

    def evaluate(self, num_games=10):
        print(f"Evaluating candidate model for {num_games} games...")
        candidate_wins = 0
        for i in range(num_games):
            current_best_is_black = (i % 2 == 0)
            res = self.play_match(current_best_is_black)
            if res == 1:
                candidate_wins += 1
            print(f"Game {i+1} completed, candidate win: {res == 1}")
        
        win_rate = candidate_wins / num_games
        print(f"Candidate win rate: {win_rate:.2f}")
        
        if win_rate >= self.config['train_params'].get('eval_win_rate_threshold', 0.55):
            print("Candidate model is better. Replacing best model.")
            return True
        else:
            print("Candidate model rejected.")
            return False

if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument('--mode', type=str, default='go', choices=['go', 'gomoku'])
    args = parser.parse_args()
    
    root_dir = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
    config_file = os.path.join(root_dir, 'config', f'{args.mode}.yaml')
    evaluator = Evaluator(config_file, args.mode)
