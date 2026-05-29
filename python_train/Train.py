import sys
import os
import torch
import torch.optim as optim
import torch.nn.functional as F
import yaml
import logging

# 将根目录和 lib 加入 sys.path
root_dir = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.append(root_dir)
sys.path.append(os.path.join(root_dir, 'lib'))

from python_nn.ModelManager import ModelManager
from python_train.SelfPlayWorker import SelfPlayWorker
from python_train.ReplayBuffer import ReplayBuffer

class Trainer:
    def __init__(self, config_path, mode="go"):
        self.mode = mode
        with open(config_path, 'r') as f:
            self.config = yaml.safe_load(f)
            
        self.model_manager = ModelManager(self.config)
        self.optimizer = optim.SGD(self.model_manager.model.parameters(), 
                                   lr=self.config['train_params']['learning_rate'], 
                                   momentum=0.9, 
                                   weight_decay=1e-4)
        
        self.replay_buffer = ReplayBuffer(self.config['train_params']['replay_buffer_size'])
        self.worker = SelfPlayWorker(self.config, self.model_manager, self.mode)
        
        self.weights_dir = os.path.join(root_dir, 'weights', self.mode)
        os.makedirs(self.weights_dir, exist_ok=True)
        
    def collect_selfplay_data(self, num_games):
        logging.info(f"Starting self-play for {num_games} games...")
        for i in range(num_games):
            game_data = self.worker.play_one_game()
            for s, prob, z in game_data:
                self.replay_buffer.add(s, prob, z)
            logging.info(f"Game {i+1} completed, buffer size: {len(self.replay_buffer)}")

    def train_step(self):
        batch_size = self.config['train_params']['batch_size']
        if len(self.replay_buffer) < batch_size:
            logging.warning("Not enough data to train.")
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

    def run_training_loop(self, iterations):
        for it in range(iterations):
            logging.info(f"--- Iteration {it+1} ---")
            self.collect_selfplay_data(2) 
            
            for epoch in range(self.config['train_params']['num_epochs']):
                losses = self.train_step()
                if losses:
                    t_loss, v_loss, p_loss = losses
                    logging.info(f"Epoch {epoch+1}: Total Loss={t_loss:.4f}, Value Loss={v_loss:.4f}, Policy Loss={p_loss:.4f}")
            
            model_path = os.path.join(self.weights_dir, f"model_iter_{it}.pt")
            self.model_manager.save_model(model_path)
            logging.info(f"Model saved to {model_path}")

if __name__ == "__main__":
    import argparse
    parser = argparse.ArgumentParser()
    parser.add_argument('--mode', type=str, default='go', choices=['go', 'gomoku'])
    args = parser.parse_args()

    log_dir = os.path.join(root_dir, 'logs')
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

    trainer = Trainer(os.path.join(root_dir, 'config', f'{args.mode}.yaml'), args.mode)
    trainer.run_training_loop(2)
