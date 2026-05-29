import torch
import torch.nn as nn
import torch.nn.functional as F

class ConvBlock(nn.Module):
    def __init__(self, in_channels, out_channels):
        super(ConvBlock, self).__init__()
        self.conv = nn.Conv2d(in_channels, out_channels, kernel_size=3, padding=1, bias=False)
        self.bn = nn.BatchNorm2d(out_channels)

    def forward(self, x):
        return F.relu(self.bn(self.conv(x)))

class ResBlock(nn.Module):
    def __init__(self, channels):
        super(ResBlock, self).__init__()
        self.conv1 = nn.Conv2d(channels, channels, kernel_size=3, padding=1, bias=False)
        self.bn1 = nn.BatchNorm2d(channels)
        self.conv2 = nn.Conv2d(channels, channels, kernel_size=3, padding=1, bias=False)
        self.bn2 = nn.BatchNorm2d(channels)

    def forward(self, x):
        residual = x
        out = F.relu(self.bn1(self.conv1(x)))
        out = self.bn2(self.conv2(out))
        out += residual
        return F.relu(out)

class DualResNet(nn.Module):
    def __init__(self, action_size, board_width, board_height, num_res_blocks=5, num_channels=64, in_channels=3):
        super(DualResNet, self).__init__()
        self.action_size = action_size
        self.board_width = board_width
        self.board_height = board_height
        
        # 初始卷积块
        self.conv_block = ConvBlock(in_channels, num_channels)
        
        # 残差块序列
        self.res_blocks = nn.ModuleList([ResBlock(num_channels) for _ in range(num_res_blocks)])
        
        # 策略头 (Policy Head)
        self.policy_conv = nn.Conv2d(num_channels, 2, kernel_size=1, bias=False)
        self.policy_bn = nn.BatchNorm2d(2)
        self.policy_fc = nn.Linear(2 * board_width * board_height, self.action_size)
        
        # 价值头 (Value Head)
        self.value_conv = nn.Conv2d(num_channels, 1, kernel_size=1, bias=False)
        self.value_bn = nn.BatchNorm2d(1)
        self.value_fc1 = nn.Linear(1 * board_width * board_height, 256)
        self.value_fc2 = nn.Linear(256, 1)

    def forward(self, x):
        # x shape: (batch_size, in_channels, board_height, board_width)
        x = self.conv_block(x)
        for block in self.res_blocks:
            x = block(x)
            
        # 策略头计算
        p = F.relu(self.policy_bn(self.policy_conv(x)))
        p = p.view(p.size(0), -1)
        p = self.policy_fc(p)
        policy_out = F.log_softmax(p, dim=1)
        
        # 价值头计算
        v = F.relu(self.value_bn(self.value_conv(x)))
        v = v.view(v.size(0), -1)
        v = F.relu(self.value_fc1(v))
        value_out = torch.tanh(self.value_fc2(v))
        
        return policy_out, value_out
