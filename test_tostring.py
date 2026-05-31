import sys, os
root_dir = os.path.dirname(os.path.abspath(__file__))
sys.path.append(root_dir); sys.path.append(os.path.join(root_dir, 'lib'))
import core_engine

game = core_engine.GoGame(9, 0.1, 0.13, 7.5, 60)
print("Initial currentPlayer:", game.GetCurrentPlayer())
print(game.ToString())

# 玩家执黑，下在中心(4,4) -> index 4*9+4 = 40
game.Step(40)
print("After step 1, currentPlayer:", game.GetCurrentPlayer())
print(game.ToString())

# 对手执白，下在中心偏上(3,4) -> index 3*9+4 = 31
game.Step(31)
print("After step 2, currentPlayer:", game.GetCurrentPlayer())
print(game.ToString())
