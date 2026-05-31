import numpy as np
import json

action_prob = np.array([0.1, 0.7, 0.2])
idx = np.argsort(action_prob)[-1]
board_width = 19
hy = int(idx % board_width)
hx = int(idx // board_width)
hints = [{"x": hy, "y": hx, "prob": float(action_prob[idx])}]

action = np.argmax(action_prob)
y = int(action % board_width)
x = int(action // board_width)
ai_move = [y, x]

try:
    json.dumps({
        "action": "ai_move", 
        "move": ai_move,
        "win_rate": float(np.max(action_prob)),
        "hints": hints
    })
    print("Success!")
except Exception as e:
    print(f"Error: {e}")
