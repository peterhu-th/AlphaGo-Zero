import sys
import os
import torch
import torch.optim as optim
import torch.nn.functional as F
import yaml
import logging
import shutil

root_dir = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.append(root_dir)
sys.path.append(os.path.join(root_dir, 'lib'))

from nn.ModelManager import ModelManager
from train.SelfPlayWorker import SelfPlayWorker
from train.ReplayBuffer import ReplayBuffer
from train.Evaluate import Evaluator

class Trainer:
    def __init__(self, config_path, mode="go", no_epsilon=False):
        self.no_epsilon = no_epsilon
        self.mode = mode
        self.config_path = config_path
        with open(config_path, 'r') as f:
            self.config = yaml.safe_load(f)
            
        self.model_manager = ModelManager(self.config)
        self.optimizer = optim.SGD(self.model_manager.model.parameters(), 
                                   lr=self.config['train_params']['learning_rate'], 
                                   momentum=self.config['train_params'].get('momentum', 0.9), 
                                   weight_decay=self.config['train_params'].get('weight_decay', 1e-4))
        # self.optimizer = optim.AdamW(self.model_manager.model.parameters(),
        #                             lr=self.config['train_params'].get('learning_rate', 1e-3),
        #                             weight_decay=self.config['train_params'].get('weight_decay', 1e-4))
        self.replay_buffer = ReplayBuffer(self.config['train_params']['replay_buffer_size'])
        self.worker = SelfPlayWorker(self.config, self.model_manager, self.mode, self.no_epsilon)
        
        self.weights_dir = os.path.join(root_dir, 'weights', self.mode)
        os.makedirs(self.weights_dir, exist_ok=True)
        
        self.best_model_path = os.path.join(self.weights_dir, "best_model.pt")
        if os.path.exists(self.best_model_path):
            logging.info(f"Loaded existing best model from {self.best_model_path}")
            self.model_manager.load_model(self.best_model_path)
        else:
            logging.info(f"No existing model found. Saving initial model as {self.best_model_path}")
            self.model_manager.save_model(self.best_model_path)
        

    def collect_selfplay_data(self, num_games, iteration=0, iteration_start_time=None):
        logging.info(f"Starting self-play for {num_games} games...")
        import core_engine
        steps = []
        black_wins = 0
        for i in range(num_games):
            game_data, step_count, winner, move_sequence = self.worker.play_one_game()
            for s, prob, z in game_data:
                self.replay_buffer.add(s, prob, z)
            
            steps.append(step_count)
            if winner == core_engine.Player.Black:
                black_wins += 1
                
            if (i + 1) % (num_games / 10) == 0:
                logging.info(f"Game {i+1} : {step_count} moves. Winner: {winner}. Buffer size: {len(self.replay_buffer)}")
        
        if len(steps) > 0:
            max_s = max(steps)
            min_s = min(steps)
            avg_s = sum(steps) / len(steps)
            win_rate = black_wins / len(steps)
            logging.info(f"Self-play Summary: Max Steps: {max_s}, Min Steps: {min_s}, Avg Steps: {avg_s:.1f}, Black Win Rate: {win_rate:.2%}")


    def train_step(self):
        batch_size = self.config['train_params']['batch_size']
        if len(self.replay_buffer) < batch_size:
            return

        states, probs, winners = self.replay_buffer.sample(batch_size)
        
        bw = self.config['env_params']['board_size']
        bh = self.config['env_params']['board_size']
        c = self.config['env_params']['in_channels']

        states = torch.FloatTensor(states).view(-1, c, bh, bw).to(self.model_manager.device)
        probs = torch.FloatTensor(probs).to(self.model_manager.device)
        winners = torch.FloatTensor(winners).unsqueeze(1).to(self.model_manager.device)

        self.model_manager.model.train()
        self.optimizer.zero_grad()
        
        log_pi, v = self.model_manager.model(states)
        
        value_loss = F.mse_loss(v, winners)
        policy_loss = -torch.sum(probs * log_pi) / probs.size(0)
        
        total_loss = value_loss + policy_loss
        
        total_loss.backward()
        self.optimizer.step()
        
        self.model_manager.model.eval()
        
        return total_loss.item(), value_loss.item(), policy_loss.item()


    def pre_populate_buffer(self):
        initial_pop = self.config['train_params'].get('initial_population', self.config['train_params']['batch_size'])
        if len(self.replay_buffer) < initial_pop:
            logging.info(f"Pre-populating replay buffer up to {initial_pop} samples...")
            while len(self.replay_buffer) < initial_pop:
                self.collect_selfplay_data(1)
            logging.info("Pre-population complete.")


    def run_training_loop(self, iterations):
        # 初始化评估器
        evaluator = Evaluator(self.config_path, self.mode)
        
        for it in range(iterations):
            logging.info("")
            logging.info(f"--- Iteration {it+1}/{iterations} ---")
            num_games = self.config['train_params'].get('num_games_per_iteration', 10)
            import datetime
            iteration_start_time = datetime.datetime.now().strftime("%Y%m%d_%H%M%S")
            self.collect_selfplay_data(num_games, it + 1, iteration_start_time) 
            
            # 训练前，将当前网络权重先存为临时模型
            temp_model_path = os.path.join(self.weights_dir, "temp_model.pt")
            self.model_manager.save_model(temp_model_path)
            
            new_samples_estimate = self.config['train_params']['num_games_per_iteration'] * 80 * 8
            batch_size = self.config['train_params']['batch_size']
            num_batches = max(1, new_samples_estimate // batch_size)
            
            for epoch in range(self.config['train_params']['num_epochs']):
                total_t, total_v, total_p = 0, 0, 0
                for _ in range(num_batches):
                    losses = self.train_step()
                    if losses:
                        total_t += losses[0]
                        total_v += losses[1]
                        total_p += losses[2]
                
                if num_batches > 0:
                    logging.info(f"Epoch {epoch+1}: Total Loss={total_t/num_batches:.4f}, Value Loss={total_v/num_batches:.4f}, Policy Loss={total_p/num_batches:.4f}")

            # 评估候选模型
            logging.info("Evaluating candidate model against the best model...")
            evaluator.load_models(self.best_model_path, temp_model_path)
            
            num_eval_games = self.config['train_params'].get('num_eval_games', 10)
            is_better = evaluator.evaluate(num_eval_games, it + 1, iteration_start_time)
            
            if is_better:
                logging.warning("Candidate model accepted! Saving as new best model.")
                self.model_manager.save_model(self.best_model_path)
            else:
                logging.info("Candidate model rejected. Reverting weights...")
                self.model_manager.load_model(self.best_model_path)


if __name__ == "__main__":
    import argparse
    parser = argparse.ArgumentParser()
    parser.add_argument('--mode', type=str, default='go', help='Game mode: go, gomoku, or others')
    parser.add_argument('--no-epsilon', action='store_true', help='Disable Dirichlet noise during self-play')
    args = parser.parse_args()

    log_dir = os.path.join(root_dir, 'logs', args.mode)
    os.makedirs(log_dir, exist_ok=True)
    
    for handler in logging.root.handlers[:]:
        logging.root.removeHandler(handler)
        
    logging.basicConfig(
        level=logging.INFO,
        format='%(asctime)s [%(levelname)s] %(message)s',
        handlers=[
            logging.FileHandler(os.path.join(log_dir, f'{args.mode}_train_log.txt')),
            logging.StreamHandler(sys.stdout)
        ]
    )

    trainer = Trainer(os.path.join(root_dir, 'config', f'{args.mode}.yaml'), args.mode, args.no_epsilon)
    num_iterations = trainer.config['train_params'].get('num_iterations', 100)
    trainer.run_training_loop(num_iterations)
