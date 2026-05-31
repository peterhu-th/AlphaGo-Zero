import sys
import os

root_dir = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.append(os.path.join(root_dir, 'lib'))

import core_engine

def print_board(game):
    board_str = game.ToString()
    print(board_str)

def main():
    game = core_engine.GomokuGame(15, 0.0, 0.0)

    moves = [
        (7, 6), (0, 0),
        (7, 8), (0, 1),
        (6, 7), (0, 2),
        (8, 7), (0, 3)
    ]
    
    for x, y in moves:
        game.Step(x * 15 + y)
        
    print_board(game)
    legal = game.GetLegalMoves()
    target = 7 * 15 + 7
    print(f"Is (7, 7) a legal move for Black? {legal[target]}")
    
    # Double Four
    game.Reset()
    moves3 = [
        (7, 5), (0, 0),
        (7, 6), (0, 1),
        (7, 8), (0, 2),
        (6, 7), (0, 3),
        (8, 7), (0, 4),
        (9, 7), (0, 5)
    ]
    for x, y in moves3:
        game.Step(x * 15 + y)
    print_board(game)
    legal = game.GetLegalMoves()
    print(f"Is (7, 7) a legal move for Black? {legal[target]}")

if __name__ == "__main__":
    main()
