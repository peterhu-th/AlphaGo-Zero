import os
import sys
import yaml
import glob
import re
import json
import asyncio
from fastapi import FastAPI, WebSocket, WebSocketDisconnect
from fastapi.middleware.cors import CORSMiddleware
import numpy as np

root_dir = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.append(root_dir)
sys.path.append(os.path.join(root_dir, 'lib'))

import core_engine
from nn.ModelManager import ModelManager

app = FastAPI()

# 允许跨域
app.add_middleware(
    CORSMiddleware,
    allow_origins=["*"],
    allow_credentials=True,
    allow_methods=["*"],
    allow_headers=["*"],
)

class GameManager:
    def __init__(self):
        self.game = None
        self.mcts = None
        self.model_manager = None
        self.board_width = 19
        self.mode = 'go'

    def init_game(self, mode="go"):
        self.mode = mode
        config_path = os.path.join(root_dir, 'config', f'{mode}.yaml')
        with open(config_path, 'r') as f:
            config = yaml.safe_load(f)

        if self.model_manager is None:
            self.model_manager = ModelManager(config)

        # 加载最新权重
        weights_dir = os.path.join(root_dir, 'weights', mode)
        if os.path.exists(weights_dir):
            models = glob.glob(os.path.join(weights_dir, 'model_iter_*.pt'))
            if models:
                models.sort(key=lambda x: int(re.search(r'model_iter_(\d+)\.pt', x).group(1)))
                latest_model = models[-1]
                self.model_manager.load_model(latest_model)
                print(f"Loaded {latest_model}")
            else:
                best_model = os.path.join(weights_dir, "best_model.pt")
                if os.path.exists(best_model):
                    self.model_manager.load_model(best_model)
                    print(f"Loaded {best_model}")
        
        self.board_width = config['env_params']['board_size']
        dir_eps = config['mcts_params']['dirichlet_epsilon']
        dir_alpha = config['mcts_params']['dirichlet_alpha']

        if mode == 'go':
            komi = config['env_params'].get('komi', 7.5)
            max_moves = config['env_params'].get('max_moves', 60)
            self.game = core_engine.GoGame(self.board_width, dir_eps, dir_alpha, komi, max_moves)
        else:
            self.game = core_engine.GomokuGame(self.board_width, dir_eps, dir_alpha)

        num_sim = config['mcts_params']['num_simulations']
        c_puct = config['mcts_params']['c_puct']
        self.mcts = core_engine.MCTS(self.model_manager.evaluate, num_sim, c_puct)

    def play_move(self, x, y):
        action = x * self.board_width + y
        self.game.Step(action)
        self.mcts.UpdateWithMove(action)

    def get_ai_move(self):
        # 让 MCTS 计算动作概率
        action_prob = self.mcts.GetActionProb(self.game, 0.0) # temp 0 获取最佳
        # 简单模拟思考过程中不断返回
        action = np.argmax(action_prob)
        
        # 为了 AI 提示，我们可以将前几个最大的返回
        hints = []
        # 将一维索引转为坐标，如果是 pass 就是 board_width*board_width
        for idx in np.argsort(action_prob)[-3:]: # 取最高的前3个
            if idx != self.board_width * self.board_width and action_prob[idx] > 0:
                hy = idx % self.board_width
                hx = idx // self.board_width
                hints.append({"x": hy, "y": hx, "prob": float(action_prob[idx]), "winRate": 0.5 + float(action_prob[idx])/2}) # 简化胜率
                
        if action == self.board_width * self.board_width:
            return None, hints, 0.5
        y = action % self.board_width
        x = action // self.board_width
        return [y, x], hints, float(np.max(action_prob))

game_managers = {}

@app.websocket("/ws/game")
async def websocket_endpoint(websocket: WebSocket):
    await websocket.accept()
    client_id = id(websocket)
    gm = GameManager()
    game_managers[client_id] = gm

    try:
        while True:
            data = await websocket.receive_text()
            payload = json.loads(data)
            action = payload.get('action')

            if action == 'init':
                mode = payload.get('mode', 'go')
                gm.init_game(mode)
                await websocket.send_text(json.dumps({
                    "action": "ready",
                    "board_size": gm.board_width
                }))
                
            elif action == 'play':
                move = payload.get('move')
                if move:
                    # 前端传过来的 move 格式是 [x, y]，x对应列，y对应行
                    # 按照 GameServer 的转换，落子 index 为行 * width + 列 -> move[1] * width + move[0]
                    # 我们调整 GameManager 中为 play_move(y, x) 
                    gm.play_move(move[1], move[0])
                    
                    is_ended, reward = gm.game.GetGameEnded()
                    if is_ended:
                        await websocket.send_text(json.dumps({"action": "game_over", "result": reward}))
                        continue
                    
                    # 模拟思考过程
                    await websocket.send_text(json.dumps({"action": "thinking", "win_rate": 0.55}))
                    # 在真实环境中这里需要放在异步线程中运行 MCTS 搜索
                    # 由于当前 Python C++ 扩展可能没有完全异步，先简单地阻塞调用
                    await asyncio.sleep(0.5)
                    
                    ai_move, hints, best_prob = gm.get_ai_move()
                    if ai_move:
                        gm.play_move(ai_move[1], ai_move[0])
                    
                    is_ended, reward = gm.game.GetGameEnded()
                    if is_ended:
                        await websocket.send_text(json.dumps({"action": "game_over", "result": reward}))
                    else:
                        await websocket.send_text(json.dumps({
                            "action": "ai_move", 
                            "move": ai_move,
                            "win_rate": best_prob,
                            "hints": hints
                        }))

    except WebSocketDisconnect:
        if client_id in game_managers:
            del game_managers[client_id]

if __name__ == "__main__":
    import uvicorn
    uvicorn.run(app, host="0.0.0.0", port=8000)
