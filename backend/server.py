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

def convert_to_builtin(obj):
    if isinstance(obj, np.generic):
        return obj.item()
    elif isinstance(obj, list):
        return [convert_to_builtin(i) for i in obj]
    elif isinstance(obj, dict):
        return {k: convert_to_builtin(v) for k, v in obj.items()}
    return obj

def calculate_score(board_state, board_width, komi):
    visited = [False] * (board_width * board_width)
    black_score = 0
    white_score = 0
    unknown = 0
    
    for i in range(len(board_state)):
        if board_state[i] == 1:
            black_score += 1
        elif board_state[i] == 2:
            white_score += 1
        elif board_state[i] == 0 and not visited[i]:
            queue = [i]
            visited[i] = True
            area = 1
            touches_black = False
            touches_white = False
            
            while queue:
                curr = queue.pop(0)
                cx = curr % board_width
                cy = curr // board_width
                
                neighbors = []
                if cx > 0: neighbors.append(curr - 1)
                if cx < board_width - 1: neighbors.append(curr + 1)
                if cy > 0: neighbors.append(curr - board_width)
                if cy < board_width - 1: neighbors.append(curr + board_width)
                
                for n in neighbors:
                    if board_state[n] == 1:
                        touches_black = True
                    elif board_state[n] == 2:
                        touches_white = True
                    elif board_state[n] == 0 and not visited[n]:
                        visited[n] = True
                        queue.append(n)
                        area += 1
            
            if touches_black and not touches_white:
                black_score += area
            elif touches_white and not touches_black:
                white_score += area
            else:
                unknown += area

    diff = black_score - (white_score + komi)
    return diff, black_score, white_score, unknown

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
        self.komi = 0.0

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
            self.komi = config['env_params'].get('komi', 7.5)
            max_moves = config['env_params'].get('max_moves', 60)
            self.game = core_engine.GoGame(self.board_width, dir_eps, dir_alpha, self.komi, max_moves)
        else:
            self.komi = 0.0
            self.game = core_engine.GomokuGame(self.board_width, dir_eps, dir_alpha)

        num_sim = config['mcts_params']['num_simulations']
        c_puct = config['mcts_params']['c_puct']
        self.mcts = core_engine.MCTS(self.model_manager.evaluate, num_sim, c_puct)

    def get_board_state(self):
        if not self.game:
            return []
        board_str = self.game.ToString()
        state = []
        for char in board_str:
            if char == 'X':
                state.append(1)
            elif char == 'O':
                state.append(2)
            elif char == '.':
                state.append(0)
        return state

    def play_move(self, x, y):
        if x == -1 and y == -1:
            action = self.board_width * self.board_width
        else:
            action = x * self.board_width + y
            
        legal_moves = self.game.GetLegalMoves()
        if legal_moves[action] == 0:
            return False # 非法落子（打劫/禁手/已有子）
            
        self.game.Step(action)
        self.mcts.UpdateWithMove(action)
        return True

    def get_black_win_rate(self):
        if not self.game: return 0.5
        features = self.game.GetStateFeatures()
        pi, val = self.model_manager.evaluate([features])
        if not val: return 0.5
        v = val[0]
        # AlphaGo 的 Value head 输出为 [-1, 1]，1表示当前玩家必胜
        if "Black" in str(self.game.GetCurrentPlayer()):
            return (v + 1) / 2
        else:
            return (1 - v) / 2

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
                hy = int(idx % self.board_width)
                hx = int(idx // self.board_width)
                hints.append({"x": hy, "y": hx, "prob": float(action_prob[idx]), "winRate": 0.5 + float(action_prob[idx])/2}) # 简化胜率
                
        if action == self.board_width * self.board_width:
            return None, hints, 0.5
        y = int(action % self.board_width)
        x = int(action // self.board_width)
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
                await websocket.send_text(json.dumps(convert_to_builtin({
                    "action": "ready",
                    "board_size": int(gm.board_width),
                    "komi": float(gm.komi)
                })))

            elif action == 'sync_history':
                # 悔棋等情况时重构整盘历史
                history = payload.get('history', [])
                gm.init_game(gm.mode)
                for move in history:
                    gm.play_move(move[1], move[0])
                await websocket.send_text(json.dumps(convert_to_builtin({"action": "synced"})))

            elif action == 'apply_score':
                # 发送申请数子：计算胜负并模拟结束
                state = gm.get_board_state()
                diff, b_s, w_s, unk = calculate_score(state, gm.board_width, gm.komi)
                
                # 如果有大量公气或未定区域，说明还没下完
                if unk > gm.board_width:
                    await websocket.send_text(json.dumps(convert_to_builtin({
                        "action": "score_rejected",
                        "message": f"棋局尚未完成！盘面仍有 {unk} 目公气或未定边界。"
                    })))
                else:
                    gm.play_move(-1, -1)
                    gm.play_move(-1, -1)
                    is_ended, reward = gm.game.GetGameEnded()
                    await websocket.send_text(json.dumps(convert_to_builtin({
                        "action": "game_over", 
                        "result": reward,
                        "diff": diff,
                        "b_s": b_s,
                        "w_s": w_s,
                        "board_state": gm.get_board_state()
                    })))

            elif action == 'ai_move_request':
                # 请求 AI 单纯走一步
                ai_win_rate_black = gm.get_black_win_rate()
                await websocket.send_text(json.dumps(convert_to_builtin({"action": "thinking", "win_rate": ai_win_rate_black})))
                
                is_ai_black = "Black" in str(gm.game.GetCurrentPlayer())
                ai_win_prob = ai_win_rate_black if is_ai_black else (1.0 - ai_win_rate_black)
                if ai_win_prob < 0.01:
                    await websocket.send_text(json.dumps(convert_to_builtin({
                        "action": "game_over", 
                        "result": -1 if is_ai_black else 1,
                        "reason": "AI 觉得胜率渺茫，投子认输",
                        "board_state": gm.get_board_state()
                    })))
                else:
                    await asyncio.sleep(0.1)
                    ai_move, hints, _ = gm.get_ai_move()
                    if ai_move:
                        gm.play_move(ai_move[1], ai_move[0])
                    is_ended, reward = gm.game.GetGameEnded()
                    if is_ended:
                        await websocket.send_text(json.dumps(convert_to_builtin({
                            "action": "game_over", 
                            "result": reward,
                            "board_state": gm.get_board_state()
                        })))
                    else:
                        await websocket.send_text(json.dumps(convert_to_builtin({
                            "action": "ai_move", 
                            "move": ai_move,
                            "win_rate": gm.get_black_win_rate(),
                            "hints": hints,
                            "board_state": gm.get_board_state()
                        })))

            elif action == 'play':
                move = payload.get('move')
                if move:
                    success = gm.play_move(move[1], move[0])
                    if not success:
                        await websocket.send_text(json.dumps(convert_to_builtin({
                            "action": "illegal_move",
                            "board_state": gm.get_board_state()
                        })))
                        continue
                    
                    # 玩家合法落子后同步盘面（处理提子等规则）
                    await websocket.send_text(json.dumps(convert_to_builtin({
                        "action": "sync_board",
                        "board_state": gm.get_board_state()
                    })))

                    is_ended, reward = gm.game.GetGameEnded()
                    if is_ended:
                        await websocket.send_text(json.dumps(convert_to_builtin({
                            "action": "game_over", 
                            "result": reward,
                            "board_state": gm.get_board_state()
                        })))
                        continue
                    
                    # 模拟思考过程
                    ai_win_rate_black = gm.get_black_win_rate()
                    await websocket.send_text(json.dumps(convert_to_builtin({"action": "thinking", "win_rate": ai_win_rate_black})))
                    
                    is_ai_black = "Black" in str(gm.game.GetCurrentPlayer())
                    ai_win_prob = ai_win_rate_black if is_ai_black else (1.0 - ai_win_rate_black)
                    if ai_win_prob < 0.01:
                        await websocket.send_text(json.dumps(convert_to_builtin({
                            "action": "game_over", 
                            "result": -1 if is_ai_black else 1,
                            "reason": "AI 觉得胜率渺茫，投子认输",
                            "board_state": gm.get_board_state()
                        })))
                        continue
                        
                    await asyncio.sleep(0.1)
                    
                    ai_move, hints, _ = gm.get_ai_move()
                    if ai_move:
                        gm.play_move(ai_move[1], ai_move[0])
                    
                    is_ended, reward = gm.game.GetGameEnded()
                    if is_ended:
                        await websocket.send_text(json.dumps(convert_to_builtin({
                            "action": "game_over", 
                            "result": reward,
                            "board_state": gm.get_board_state()
                        })))
                    else:
                        await websocket.send_text(json.dumps(convert_to_builtin({
                            "action": "ai_move", 
                            "move": ai_move,
                            "win_rate": gm.get_black_win_rate(),
                            "hints": hints,
                            "board_state": gm.get_board_state()
                        })))

    except WebSocketDisconnect:
        if client_id in game_managers:
            del game_managers[client_id]

if __name__ == "__main__":
    import uvicorn
    uvicorn.run(app, host="0.0.0.0", port=8000)
