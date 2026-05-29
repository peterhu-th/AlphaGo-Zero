import sys
import os
import yaml
import glob
import re
import argparse

root_dir = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.append(root_dir)
sys.path.append(os.path.join(root_dir, 'lib'))

import core_engine
from python_nn.ModelManager import ModelManager

class HumanVsAIGame:
    def __init__(self, config_path, mode="go"):
        self.mode = mode
        with open(config_path, 'r') as f:
            self.config = yaml.safe_load(f)
            
        self.model_manager = ModelManager(self.config)
        
        weights_dir = os.path.join(root_dir, 'weights', self.mode)
        if os.path.exists(weights_dir):
            models = glob.glob(os.path.join(weights_dir, 'model_iter_*.pt'))
            if models:
                models.sort(key=lambda x: int(re.search(r'model_iter_(\d+)\.pt', x).group(1)))
                latest_model = models[-1]
                print(f"Loading latest model: {latest_model}")
                self.model_manager.load_model(latest_model)
            else:
                print(f"No trained models found in {weights_dir}. Using random weights.")
        else:
            print(f"{weights_dir} directory not found. Using random weights.")
        
        self.board_width = self.config['env_params']['board_size']
        
        dir_eps = self.config['mcts_params']['dirichlet_epsilon']
        dir_alpha = self.config['mcts_params']['dirichlet_alpha']
        
        if self.mode == 'go':
            self.game = core_engine.GoGame(self.board_width, dir_eps, dir_alpha)
        else:
            self.game = core_engine.GomokuGame(self.board_width, dir_eps, dir_alpha)
        
        num_sim = self.config['mcts_params']['num_simulations']
        c_puct = self.config['mcts_params']['c_puct']
        self.mcts = core_engine.MCTS(self.model_manager.evaluate, num_sim, c_puct)

    def print_board(self):
        print("\n" + "="*30)
        cols = " ".join([chr(ord('A') + i) for i in range(self.board_width)])
        print("    " + cols)
        
        board_str = self.game.ToString().split('\n')
        row_idx = 1
        for row in board_str:
            if row.strip() == "":
                continue
            print(f"{row_idx:2d} {row}")
            row_idx += 1
            if row_idx > self.board_width:
                break
        print("="*30 + "\n")

    def play(self):
        print(f"Welcome to AlphaGo Zero - Human vs AI ({self.mode.upper()})")
        
        while True:
            choice = input("Do you want to play Black (first) or White (second)? (b/w): ").strip().lower()
            if choice in ['b', 'w']:
                break
            print("Invalid choice.")
            
        human_player = core_engine.Player.Black if choice == 'b' else core_engine.Player.White
        
        while True:
            self.print_board()
            current_player = self.game.GetCurrentPlayer()
            
            is_ended, reward = self.game.GetGameEnded()
            if is_ended:
                print("Game Ended!")
                if reward > 0.5:
                    winner = current_player
                elif reward < -0.5:
                    winner = core_engine.Player.White if current_player == core_engine.Player.Black else core_engine.Player.Black
                else:
                    winner = core_engine.Player.NonePlayer
                    
                if winner == human_player:
                    print("Congratulations, you win!")
                elif winner == core_engine.Player.NonePlayer:
                    print("It's a draw!")
                else:
                    print("AI wins!")
                break

            if current_player == human_player:
                legal_moves = self.game.GetLegalMoves()
                while True:
                    user_input = input(f"Your move (e.g., 'A1' to '{chr(ord('A')+self.board_width-1)}{self.board_width}' or 'pass'): ").strip().upper()
                    if user_input == 'PASS':
                        if self.mode == 'gomoku':
                            print("PASS is not allowed in Gomoku. Try again.")
                            continue
                        action = self.board_width * self.board_width
                        break
                    
                    try:
                        import re
                        match = re.match(r'([A-Z])\s*(\d+)', user_input)
                        if match:
                            col_char = match.group(1)
                            row_num = int(match.group(2))
                            
                            y = ord(col_char) - ord('A')
                            x = row_num - 1
                            
                            action = x * self.board_width + y
                            if 0 <= y < self.board_width and 0 <= x < self.board_width and legal_moves[action] == 1:
                                break
                            else:
                                print("Illegal move or spot already taken. Try again.")
                        else:
                            print("Invalid input format. Use 'A1' or 'A 1'.")
                    except:
                        print("Invalid input format.")
                
                self.game.Step(action)
                self.mcts.UpdateWithMove(action)
            else:
                print("AI is thinking...")
                action_prob = self.mcts.GetActionProb(self.game, 0.0)
                import numpy as np
                action = np.argmax(action_prob)
                if action == self.board_width * self.board_width:
                    print("AI played: PASS")
                else:
                    x = action // self.board_width
                    y = action % self.board_width
                    print(f"AI played: {chr(ord('A') + y)}{x + 1}")
                self.game.Step(int(action))
                self.mcts.UpdateWithMove(int(action))

if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument('--mode', type=str, default='go', choices=['go', 'gomoku'])
    args = parser.parse_args()
    
    config_file = os.path.join(root_dir, 'config', f'{args.mode}.yaml')
    game = HumanVsAIGame(config_file, args.mode)
    game.play()
