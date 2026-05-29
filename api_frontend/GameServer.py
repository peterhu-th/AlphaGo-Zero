import sys
import os
import yaml
import glob
import re

root_dir = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.append(root_dir)
sys.path.append(os.path.join(root_dir, 'lib'))

import core_engine
from python_nn.ModelManager import ModelManager

class HumanVsAIGame:
    def __init__(self, config_path):
        with open(config_path, 'r') as f:
            self.config = yaml.safe_load(f)
            
        self.model_manager = ModelManager(self.config)
        
        # 扫描最新的权重
        weights_dir = os.path.join(root_dir, 'weights')
        if os.path.exists(weights_dir):
            models = glob.glob(os.path.join(weights_dir, 'model_iter_*.pt'))
            if models:
                # 找到最大的 iter 编号
                models.sort(key=lambda x: int(re.search(r'model_iter_(\d+)\.pt', x).group(1)))
                latest_model = models[-1]
                print(f"Loading latest model: {latest_model}")
                self.model_manager.load_model(latest_model)
            else:
                print("No trained models found in weights/. Using random weights.")
        else:
            print("weights/ directory not found. Using random weights.")
        
        self.board_width = self.config['env_params']['board_size']
        self.game = core_engine.GoGame(self.board_width)
        
        num_sim = self.config['mcts_params']['num_simulations']
        c_puct = self.config['mcts_params']['c_puct']
        self.mcts = core_engine.MCTS(self.model_manager.evaluate, num_sim, c_puct)

    def print_board(self):
        print("\n" + "="*30)
        print("    " + " ".join([str(i) for i in range(self.board_width)]))
        
        board_str = self.game.ToString().split('\n')
        # AlphaGo Zero 内部使用的是 1 维表示，通过 ToString 提供最基础视图。
        # 我们这里将其美化。
        for i in range(self.board_width):
            row = []
            for j in range(self.board_width):
                # 借助 game.GetLegalMoves 只能看空地，由于没有直接导出棋盘状态矩阵，我们依赖 ToString
                pass
        print(self.game.ToString())
        print("="*30 + "\n")

    def play(self):
        print("Welcome to AlphaGo Zero - Human vs AI")
        
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
                    user_input = input("Your move (row col or 'pass'): ").strip()
                    if user_input.lower() == 'pass':
                        action = self.board_width * self.board_width
                        break
                    
                    try:
                        x, y = map(int, user_input.split())
                        action = x * self.board_width + y
                        if action >= 0 and action < len(legal_moves) and legal_moves[action] == 1:
                            break
                        else:
                            print("Illegal move or spot already taken. Try again.")
                    except:
                        print("Invalid input format. Use 'row col' (e.g., '0 1').")
                
                self.game.Step(action)
                self.mcts.UpdateWithMove(action)
            else:
                print("AI is thinking...")
                # AI 落子时不加温度（temp=0.0）以选择最大访问次数的动作
                action_prob = self.mcts.GetActionProb(self.game, 0.0)
                import numpy as np
                action = np.argmax(action_prob)
                if action == self.board_width * self.board_width:
                    print("AI played: PASS")
                else:
                    x = action // self.board_width
                    y = action % self.board_width
                    print(f"AI played: {x} {y}")
                self.game.Step(int(action))
                self.mcts.UpdateWithMove(int(action))

if __name__ == "__main__":
    game = HumanVsAIGame(os.path.join(root_dir, 'config', 'Config.yaml'))
    game.play()
