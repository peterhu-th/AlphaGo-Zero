# AlphaGo Zero 强化学习与 Web 交互引擎

这是一个基于 AlphaGo Zero 论文复现的自对弈强化学习框架与现代 Web 交互项目。该项目采用了 **C++ (核心引擎与 MCTS)** 和 **Python (神经网络与高层工作流)** 的混合架构，实现了计算性能与深度学习灵活性的平衡，并通过 **FastAPI + Vue3** 提供 Web 交互。

目前项目支持围棋（默认配置 9x9）以及五子棋（默认配置 15x15）。

## 1. 环境准备

项目推荐使用 Anaconda/Miniconda 进行环境管理。核心依赖如下：

- Python 3.8+
- PyTorch (支持 CUDA 以实现最快训练)
- Pybind11, CMake (用于构建 C++ 拓展)
- Numpy, PyYAML
- FastAPI, Uvicorn, Websockets (用于后端通信)
- Node.js & npm (用于前端 Vue3 构建)

## 2. 文件结构与说明

项目的代码架构分为算法核心与 Web 交互两大主线：

```text
AlphaGo/
├── core/                   # C++ 核心库：提供极速的树搜索与规则演算
│   ├── GameInterface.h     # 抽象的两人对战棋类接口
│   ├── games/GoGame.h/cpp  # 围棋/五子棋 等具体游戏规则实现
│   └── MCTS.h/cpp          # 蒙特卡洛树搜索算法（附带 Dirichlet 噪声及复用逻辑）
├── nn/                     # 神经网络：用于评估盘面价值和输出走棋概率
│   ├── DualResNet.py       # 包含了策略头(Policy Head)与价值头(Value Head)的残差网络
│   └── ModelManager.py     # 模型生命周期管理，提供给 MCTS 供 C++ 端回调评估
├── train/                  # 强化学习流水线
│   ├── Evaluate.py         # 新旧模型对抗评估机制
│   ├── SelfPlayWorker.py   # 自对弈工作进程，利用 MCTS 搜集落子数据
│   ├── Train.py            # 训练核心入口：自对弈 -> 经验回放池 -> 模型更新循环
│   └── Render.py           # 棋谱渲染脚本：使用 matplotlib 生成对局过程的可视化图片
├── backend/                # 后端服务
│   ├── server.py           # FastAPI WebSocket 引擎接口 (提供实时对弈服务)
│   └── cli_play.py         # 基于终端的命令行版人机对弈测试脚本
├── frontend/               # 前端 Web UI (Vue3 + TypeScript + Vite)
│   ├── src/components/     # 包含 Canvas 绘制的高性能响应式棋盘 (Board.vue)
│   └── src/views/          # 实时对弈界面 (Play.vue), 棋谱列表 (Records.vue), 胜率复盘 (Replay.vue)
├── config/                 # 配置文件目录 (分别支持 go.yaml 和 gomoku.yaml)
├── lib/                    # 存放编译好的 C++ 引擎动态链接库 (.so)
├── weights/                # 存放深度学习模型权重 (如 best_model.pt)
├── tests/                  # 测试代码 (C++引擎与Python接口逻辑验证)
└── setup.py                # C++ 扩展编译脚本
```

## 3. 运行步骤

### 3.1 编译核心引擎
在 Python 虚拟环境中，使用以下命令编译加速引擎，并将其移动到 `lib/` 目录：
```bash
pip install -e .
mkdir -p lib
mv core_engine*.so lib/
```

### 3.2 启动模型训练 (可选)
如果需要自己从头训练模型：
```bash
# 默认训练围棋 (支持 --mode gomoku 训练五子棋，支持 --no-epsilon 关闭探索噪声)
python train/Train.py --mode go
```
> 训练的权重将保存在 `weights/<mode>/` 目录，训练日志会记录于 `logs/<mode>/train_log.txt` 中。
> 在训练和评估过程中，自动生成的棋谱图片会按照时间保存在 `logs/<mode>/records/` 目录下的对应子文件夹中。

### 3.3 启动 Web 交互服务
您需要分别启动后端 API 与前端页面：

**启动后端 (FastAPI + WebSocket)**
```bash
cd backend
python server.py
```
*(后端运行在 `http://localhost:8000`)*

**启动前端 (Vue3 + Vite)**
打开另一个终端：
```bash
cd frontend
npm install   # 首次运行需要安装依赖
npm run dev
```
*(前端运行在 `http://localhost:5173`)*。