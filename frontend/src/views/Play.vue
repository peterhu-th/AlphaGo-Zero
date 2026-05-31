<template>
  <div class="play-container">
    <div class="header">
      <h2>AlphaGo Zero - 实时对弈</h2>
      <div class="controls">
        <select v-model="mode" @change="initGame">
          <option value="go">围棋 (Go)</option>
          <option value="gomoku">五子棋 (Gomoku)</option>
        </select>
        <button @click="initGame">重新开始</button>
        <label class="switch">
          <input type="checkbox" v-model="aiHintEnabled">
          <span class="slider round"></span>
          AI 提示
        </label>
      </div>
    </div>

    <div class="game-area">
      <!-- 棋盘组件 -->
      <div class="board-wrapper">
        <Board 
          :board-size="boardSize" 
          :board-state="boardState"
          :hints="aiHintEnabled ? currentHints : []"
          :last-move="lastMove"
          @move="handlePlayerMove"
        />
      </div>

      <!-- 侧边栏：胜率和状态 -->
      <div class="sidebar">
        <div class="status-panel">
          <h3>游戏状态</h3>
          <p class="status-text">{{ statusMessage }}</p>
          <p v-if="aiThinking" class="thinking-text">AI 正在思考中...</p>
        </div>

        <div class="win-rate-panel">
          <h3>黑方胜率估算</h3>
          <div class="progress-bar-container">
            <div class="progress-bar" :style="{ width: winRatePercent + '%' }"></div>
          </div>
          <p class="win-rate-text">{{ winRatePercent.toFixed(1) }}%</p>
        </div>
      </div>
    </div>
  </div>
</template>

<script setup lang="ts">
import { ref, onMounted, onUnmounted, computed } from 'vue';
import Board from '../components/Board.vue';

// 游戏配置
const mode = ref('go');
const boardSize = ref(19);
const aiHintEnabled = ref(false);

// 游戏状态
const boardState = ref<number[]>([]);
const lastMove = ref<{x: number, y: number} | null>(null);
const currentHints = ref<{x: number, y: number, prob: number, winRate: number}[]>([]);

// UI状态
const statusMessage = ref('连接中...');
const aiThinking = ref(false);
const winRate = ref(0.5); // 0 到 1 之间，黑方胜率
const winRatePercent = computed(() => winRate.value * 100);

// 对局记录
const moveHistory = ref<{x: number, y: number, color: number, winRate: number}[]>([]);

// WebSocket 实例
let ws: WebSocket | null = null;
let gameId: string | null = null;

const initBoard = () => {
  boardState.value = new Array(boardSize.value * boardSize.value).fill(0);
  lastMove.value = null;
  currentHints.value = [];
  winRate.value = 0.5;
  moveHistory.value = [];
  statusMessage.value = '准备就绪，轮到黑方（你）落子';
};

const initGame = async () => {
  if (ws) {
    ws.close();
  }
  initBoard();
  
  // 初始化 WebSocket 连接
  ws = new WebSocket(`ws://localhost:8000/ws/game`);
  
  ws.onopen = () => {
    ws?.send(JSON.stringify({ action: 'init', mode: mode.value }));
    statusMessage.value = '等待你的落子...';
  };

  ws.onmessage = (event) => {
    const data = JSON.parse(event.data);
    
    if (data.action === 'ready') {
      if (data.board_size) {
        boardSize.value = data.board_size;
        initBoard(); // 尺寸更新后重新初始化一次以避免越界
      }
    }
    else if (data.action === 'thinking') {
      aiThinking.value = true;
      if (data.win_rate !== undefined) winRate.value = data.win_rate;
      if (data.hints) currentHints.value = data.hints; // 实时提示
    } 
    else if (data.action === 'ai_move') {
      aiThinking.value = false;
      const { move, win_rate } = data;
      const [x, y] = move;
      boardState.value[y * boardSize.value + x] = 2; // 白子
      lastMove.value = { x, y };
      if (win_rate !== undefined) winRate.value = win_rate;
      moveHistory.value.push({x, y, color: 2, winRate: winRate.value});
      statusMessage.value = 'AI 落子完毕，轮到你';
      currentHints.value = [];
    }
    else if (data.action === 'game_over') {
      aiThinking.value = false;
      statusMessage.value = `游戏结束！结果: ${data.result}`;
      
      // 保存棋谱
      const record = {
        id: Date.now().toString(),
        mode: mode.value,
        boardSize: boardSize.value,
        timestamp: Date.now(),
        result: data.result,
        moves: moveHistory.value
      };
      const stored = localStorage.getItem('alphago_records') || '[]';
      const records = JSON.parse(stored);
      records.push(record);
      localStorage.setItem('alphago_records', JSON.stringify(records));
    }
    else if (data.action === 'error') {
      statusMessage.value = `错误: ${data.message}`;
    }
  };

  ws.onerror = () => {
    statusMessage.value = 'WebSocket 连接失败，请确保后端服务运行于 8000 端口。';
  };
};

const handlePlayerMove = ({ x, y }: { x: number, y: number }) => {
  if (aiThinking.value) return; // AI 思考时禁止落子
  const index = y * boardSize.value + x;
  if (boardState.value[index] !== 0) return; // 已经有子

  // 本地落黑子
  boardState.value[index] = 1;
  lastMove.value = { x, y };
  moveHistory.value.push({x, y, color: 1, winRate: winRate.value});
  statusMessage.value = '落子成功，AI 开始思考...';
  aiThinking.value = true;
  currentHints.value = [];

  // 发送给后端
  if (ws && ws.readyState === WebSocket.OPEN) {
    ws.send(JSON.stringify({ action: 'play', move: [x, y] }));
  }
};

onMounted(() => {
  initGame();
});

onUnmounted(() => {
  if (ws) ws.close();
});
</script>

<style scoped>
.play-container {
  max-width: 1300px;
  margin: 0 auto;
  padding: 20px;
}
.header {
  display: flex;
  justify-content: space-between;
  align-items: center;
  margin-bottom: 30px;
  border-bottom: 1px solid #eaeaea;
  padding-bottom: 15px;
}
.controls {
  display: flex;
  gap: 15px;
  align-items: center;
}
select, button {
  padding: 10px 20px;
  border-radius: 8px;
  border: 1px solid #ddd;
  background-color: #fff;
  cursor: pointer;
  font-weight: bold;
  transition: all 0.3s ease;
  box-shadow: 0 2px 4px rgba(0,0,0,0.05);
}
button:hover, select:hover {
  background-color: #f8f9fa;
  transform: translateY(-1px);
  box-shadow: 0 4px 8px rgba(0,0,0,0.1);
}
.game-area {
  display: flex;
  gap: 40px;
  justify-content: center; /* 居中显示 */
  align-items: flex-start;
  flex-wrap: wrap; /* 小屏幕自动换行 */
}
/* 给棋盘单独的包裹器控制宽度 */
.board-wrapper {
  flex: 2;
  min-width: 400px;
  display: flex;
  justify-content: center;
}
.sidebar {
  flex: 1;
  min-width: 300px;
  display: flex;
  flex-direction: column;
  gap: 20px;
}
.status-panel, .win-rate-panel {
  background: white;
  padding: 25px;
  border-radius: 16px;
  box-shadow: 0 10px 25px rgba(0,0,0,0.05);
  border: 1px solid #f0f0f0;
}
.thinking-text {
  color: #ff9800;
  font-weight: bold;
  animation: pulse 1.5s infinite;
}
@keyframes pulse {
  0% { opacity: 0.6; }
  50% { opacity: 1; }
  100% { opacity: 0.6; }
}
.progress-bar-container {
  width: 100%;
  height: 20px;
  background-color: #fff; /* 白方 */
  border: 1px solid #ddd;
  border-radius: 10px;
  overflow: hidden;
  position: relative;
}
.progress-bar {
  height: 100%;
  background-color: #333; /* 黑方 */
  transition: width 0.3s ease;
}
.win-rate-text {
  text-align: center;
  font-weight: bold;
  margin-top: 10px;
}

/* Toggle Switch */
.switch {
  position: relative;
  display: inline-flex;
  align-items: center;
  gap: 10px;
  cursor: pointer;
}
.switch input {
  opacity: 0;
  width: 0;
  height: 0;
}
.slider {
  position: relative;
  width: 40px;
  height: 20px;
  background-color: #ccc;
  transition: .4s;
  border-radius: 20px;
}
.slider:before {
  position: absolute;
  content: "";
  height: 16px;
  width: 16px;
  left: 2px;
  bottom: 2px;
  background-color: white;
  transition: .4s;
  border-radius: 50%;
}
input:checked + .slider {
  background-color: #2196F3;
}
input:checked + .slider:before {
  transform: translateX(20px);
}
</style>
