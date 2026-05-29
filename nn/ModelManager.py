import torch
import os
from .DualResNet import DualResNet
import numpy as np

class ModelManager:
    def __init__(self, config):
        self.config = config
        self.device = torch.device("cuda" if torch.cuda.is_available() else "cpu")
        
        self.board_width = config['env_params']['board_size']
        self.board_height = config['env_params']['board_size']
        self.action_size = config['env_params']['action_size']
        self.in_channels = config['env_params']['in_channels']

        self.model = DualResNet(
            action_size=self.action_size,
            board_width=self.board_width,
            board_height=self.board_height,
            num_res_blocks=config['network_params']['num_residual_blocks'],
            num_channels=config['network_params']['num_channels'],
            in_channels=self.in_channels
        ).to(self.device)
        self.model.eval()

    def save_model(self, path):
        torch.save(self.model.state_dict(), path)

    def load_model(self, path):
        if os.path.exists(path):
            self.model.load_state_dict(torch.load(path, map_location=self.device, weights_only=True))
            print(f"Model loaded from {path}")
        else:
            print(f"Path {path} does not exist, starting with random weights.")

    def evaluate(self, state_features):
        """
        供 C++ MCTS 回调使用的单例推理函数。
        状态特征输入为 1D 列表，长度为 in_channels * board_height * board_width
        返回值为: (概率分布向量 list, 价值标量 float)
        """
        # 将 1D 列表重塑为 (1, Channels, Height, Width)
        tensor_state = torch.FloatTensor(state_features).view(1, self.in_channels, self.board_height, self.board_width).to(self.device)
        
        with torch.no_grad():
            log_pi, v = self.model(tensor_state)
            # 还原为概率
            pi = torch.exp(log_pi).cpu().numpy().flatten().tolist()
            value = v.item()
            
        return pi, value
