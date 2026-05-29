#pragma once

#include <vector>
#include <memory>
#include <string>

// 玩家类型定义
enum class Player {
    Black = 1,
    White = -1,
    NonePlayer = 0
};

// 抽象游戏接口
class GameInterface {
public:
    virtual ~GameInterface() = default;

    // 获取棋盘的尺寸
    virtual std::pair<int, int> GetBoardSize() const = 0;

    // 获取动作空间大小
    virtual int GetActionSize() const = 0;

    // 初始化/获取初始状态
    virtual void Reset() = 0;

    // 获取当前玩家的合法动作掩码 (1 表示合法, 0 表示非法)
    virtual std::vector<int> GetLegalMoves() const = 0;

    // 推演下一个状态
    virtual Player Step(int action) = 0;

    // 判断游戏是否结束，并返回终局奖励
    // 是否终局，相对于当前玩家的奖励: 1.0表示黑胜，-1.0表示白胜，0.0表示平局
    virtual std::pair<bool, float> GetGameEnded() const = 0;

    // 获取当前状态的特征表示
    virtual std::vector<float> GetStateFeatures() const = 0;

    // 获取当前执子的玩家
    virtual Player GetCurrentPlayer() const = 0;

    // 复制当前游戏状态
    virtual std::unique_ptr<GameInterface> Clone() const = 0;

    // 将棋盘状态转换为字符串
    virtual std::string ToString() const = 0;

    // 获取狄利克雷噪声 (epsilon, alpha)
    virtual std::pair<float, float> GetDirichletParams() const = 0;
};
