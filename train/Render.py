import os
import sys
import datetime
import matplotlib.pyplot as plt
import matplotlib.patches as patches
import core_engine

def render_game(move_sequence, config, game_idx, winner, mode="go", iteration=0, iteration_start_time=None):
    board_size = config['env_params']['board_size']
    komi = config['env_params'].get('komi', 6)
    
    dir_eps = 0.0
    dir_alpha = 0.03
    max_moves = config['env_params'].get('max_moves', 60)
    
    if mode == "go":
        game = core_engine.GoGame(board_size, dir_eps, dir_alpha, komi, max_moves)
    elif mode == 'gomoku':
        game = core_engine.GomokuGame(board_size, dir_eps, dir_alpha)
    else:
        raise ValueError(f"Unsupported mode: {mode}")
        
    board_state = {}
    captures = []
    
    for step, action in enumerate(move_sequence):
        move_num = step + 1
        current_player = game.GetCurrentPlayer()
        
        old_str = [line.strip().split() for line in game.ToString().strip().split('\n') if line.strip()]
        
        game.Step(action)
        
        new_str = [line.strip().split() for line in game.ToString().strip().split('\n') if line.strip()]
        
        if action == board_size * board_size:
            captures.append(f"Move {move_num}: PASS")
            continue
            
        x = action // board_size
        y = action % board_size
        
        opponent_char = 'O' if current_player == core_engine.Player.Black else 'X'
        
        captured_this_step = []
        for i in range(board_size):
            for j in range(board_size):
                if i < len(old_str) and j < len(old_str[i]) and i < len(new_str) and j < len(new_str[i]):
                    if old_str[i][j] == opponent_char and new_str[i][j] == '.':
                        captured_this_step.append((i, j))
                        if (i, j) in board_state:
                            del board_state[(i, j)]
        
        if captured_this_step:
            captures.append(f"Move {move_num} captured {len(captured_this_step)} stones")
            
        board_state[(x, y)] = {'player': current_player, 'move': move_num}
        
    fig, ax = plt.subplots(figsize=(8, 10))
    ax.set_facecolor('#E8C478')
    
    for i in range(board_size):
        ax.plot([0, board_size - 1], [i, i], color='black', linewidth=1)
        ax.plot([i, i], [0, board_size - 1], color='black', linewidth=1)
        
    if board_size == 9:
        stars = [(2, 2), (2, 6), (6, 2), (6, 6), (4, 4)]
    elif board_size == 19:
        stars = [(3, 3), (3, 9), (3, 15), (9, 3), (9, 9), (9, 15), (15, 3), (15, 9), (15, 15)]
    else:
        stars = []
        
    for sx, sy in stars:
        ax.add_patch(patches.Circle((sy, board_size - 1 - sx), 0.15, color='black'))
        
    for (x, y), info in board_state.items():
        player = info['player']
        move = info['move']
        
        color = 'black' if player == core_engine.Player.Black else 'white'
        text_color = 'white' if player == core_engine.Player.Black else 'black'
        
        plot_x = y
        plot_y = board_size - 1 - x
        
        ax.add_patch(patches.Circle((plot_x, plot_y), 0.45, color=color, zorder=10))
        ax.text(plot_x, plot_y, str(move), color=text_color, ha='center', va='center', zorder=11, fontsize=8)
        
    ax.set_xlim(-1, board_size)
    ax.set_ylim(-1, board_size)
    ax.axis('off')
    
    time_str = datetime.datetime.now().strftime("%Y-%m-%d %H:%M:%S")
    winner_str = "Black" if winner == core_engine.Player.Black else "White" if winner == core_engine.Player.White else "Draw"

    title = f"AlphaGo Zero Self-Play | Game {game_idx}\n"
    title += f"Black: Current Best vs White: Current Best\n"
    title += f"Mode: {mode.upper()} | Komi: {komi} | Winner: {winner_str} | Time: {time_str}"
    
    ax.set_title(title, pad=20, fontsize=12)
    
    captures_str = "Event Log (Captures / Pass):\n" + "\n".join(captures) if captures else "No captures."
    if len(captures) > 12:
        captures_str = "Event Log (Captures / Pass):\n...\n" + "\n".join(captures[-12:])
        
    plt.figtext(0.5, 0.02, captures_str, ha="center", fontsize=9, bbox={"facecolor":"white", "alpha":0.8, "pad":5})
    
    root_dir = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
    
    if iteration_start_time is None:
        iteration_start_time = datetime.datetime.now().strftime("%Y%m%d_%H%M%S")
    folder_name = f"{iteration_start_time}_{iteration}"
    
    image_dir = os.path.join(root_dir, 'logs', mode, 'records', folder_name)
    os.makedirs(image_dir, exist_ok=True)
    
    file_path = os.path.join(image_dir, f'{game_idx}.png')
    plt.savefig(file_path, bbox_inches='tight')
    plt.close()
