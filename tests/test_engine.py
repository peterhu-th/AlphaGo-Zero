import sys
import os

root_dir = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.append(root_dir)
sys.path.append(os.path.join(root_dir, 'lib'))
import core_engine

def test_go_game():
    game = core_engine.GoGame(9)
    print("Initial state:")
    print(game.ToString())
    
    legal_moves = game.GetLegalMoves()
    assert legal_moves[-1] == 1, "PASS should always be legal"
    
    print("Black steps 0 0")
    game.Step(0)
    print(game.ToString())
    
    print("White steps 0 1")
    game.Step(1)
    print(game.ToString())
    
    print("Black steps 1 0")
    game.Step(9)
    print(game.ToString())
    
    print("White steps 1 1")
    game.Step(10)
    print(game.ToString())

    print("Test finished successfully!")

if __name__ == "__main__":
    test_go_game()
