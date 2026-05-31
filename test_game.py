import sys, os
root_dir = os.path.dirname(os.path.abspath(__file__))
sys.path.append(root_dir); sys.path.append(os.path.join(root_dir, 'lib'))
import core_engine
game = core_engine.GoGame(9, 0.1, 0.13, 7.5, 60)
# 让它产生一些X, O, 以便测试
game.Step(40) # 1
game.Step(41) # 2
print("Length:", len(game.ToString()))
print("REPR:")
print(repr(game.ToString()))
