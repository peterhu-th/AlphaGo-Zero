import sys, os, yaml
root_dir = os.path.dirname(os.path.abspath(__file__))
sys.path.append(root_dir); sys.path.append(os.path.join(root_dir, 'lib'))
import core_engine
from nn.ModelManager import ModelManager
with open('config/go.yaml', 'r') as f:
    config = yaml.safe_load(f)
mm = ModelManager(config)
game = core_engine.GoGame(9, 0.1, 0.13, 7.5, 60)
player = game.GetCurrentPlayer()
features = game.GetStateFeatures()
pi, val = mm.evaluate([features])
print("Player:", player)
print("Value:", val)
