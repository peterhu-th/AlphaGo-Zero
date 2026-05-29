import collections
import random

class ReplayBuffer:
    def __init__(self, capacity):
        self.buffer = collections.deque(maxlen=capacity)

    def add(self, state_features, search_prob, winner):
        """
        保存一条经验: (状态特征, MCTS输出概率, 最终赢家)
        """
        self.buffer.append((state_features, search_prob, winner))

    def sample(self, batch_size):
        batch = random.sample(self.buffer, min(batch_size, len(self.buffer)))
        states, probs, winners = zip(*batch)
        return list(states), list(probs), list(winners)

    def __len__(self):
        return len(self.buffer)
