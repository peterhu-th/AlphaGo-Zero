# AlphaGo Zero 强化学习核心引擎

这是一个基于 AlphaGo Zero 论文复现的两人棋类游戏自对弈强化学习框架。该项目采用了 **C++ (核心引擎与 MCTS)** 和 **Python (神经网络与高层工作流)** 的混合架构，利用 `Pybind11` 交互，实现了计算性能与深度学习灵活性的平衡。

目前项目支持 9x9 围棋、五子棋。

## 1. 配置需要与环境准备

项目推荐使用 Anaconda/Miniconda 进行环境管理。所有依赖（如 PyTorch、Pybind11 等）应当在环境中正确安装：

- Python 3.8+
- PyTorch (支持 CUDA 以实现最快训练)
- Pybind11
- CMake (用于构建 C++ 拓展)
- Numpy, PyYAML

## 2. 文件结构与说明

项目的核心代码分为三个主要部分，并通过顶层脚本协调：

```text
AlphaGo/
├── cpp_core/               # C++ 核心库：提供极速的树搜索与规则演算
│   ├── GameInterface.h     # 抽象的两人对战棋类接口
│   ├── games/GoGame.h/cpp  # 围棋/五子棋 等具体游戏规则实现
│   ├── MCTS.h/cpp          # 蒙特卡洛树搜索算法（附带 Dirichlet 噪声及复用逻辑）
│   └── Pybind_wrapper.cpp  # Python 包装器，将 C++ 类导出给 Python 
├── python_nn/              # 神经网络：用于评估盘面价值和输出走棋概率
│   ├── DualResNet.py       # 包含了策略头(Policy Head)与价值头(Value Head)的残差网络
│   └── ModelManager.py     # 模型生命周期管理，提供给 MCTS 供 C++ 端回调评估
├── python_train/           # 强化学习流水线
│   ├── Evaluate.py         # 新旧模型对抗评估机制
│   ├── ReplayBuffer.py     # 经验回放池，用于打乱训练数据增强泛化
│   ├── SelfPlayWorker.py   # 自对弈工作进程，利用 MCTS 搜集落子数据
│   └── Train.py            # 训练核心入口：自对弈 -> 经验回放池 -> 模型更新循环
├── api_frontend/           # 交互端
│   └── GameServer.py       # 人机对弈命令行终端
├── config/                 # 配置文件目录
│   └── Config.yaml         # 控制棋盘尺寸、神经网络深度、MCTS模拟次数等超参
├── setup.py                # C++ 扩展编译脚本
└── test_engine.py          # 基础逻辑连通性测试脚本
```

## 3. 运行步骤

### 编译 C++ 扩展
在 Conda 虚拟环境中，使用以下命令编译加速引擎，并将其移动到 `lib/` 目录供 Python 调用：
```bash
python setup.py build_ext --inplace
mkdir -p lib
mv core_engine*.so lib/
```

### 修改超参数
在 `config/Config.yaml` 修改训练的超参数。

### 自对弈训练
执行以下命令，程序将开始长期的自对弈搜集数据并更新模型：
```bash
python python_train/Train.py
```
> 训练的权重将保存在 `weights/` 目录，训练日志会输出到控制台并记录于 `logs/train_log.txt` 中。

### 人机对战测试
启动对战终端（终端会自动扫描 `weights/` 加载最新的权重）：
```bash
python frontend/GameServer.py
```
终端会询问您选择执黑（先手）还是执白，通过输入诸如 `A 1` (行 列) 的坐标来进行落子互动！
