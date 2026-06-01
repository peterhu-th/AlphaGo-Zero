<template>
  <div class="play-container">
    <div class="header">
      <h2>AlphaGo Zero - 实时对弈</h2>
      <div class="controls">
        <select v-model="mode" @change="initGame">
          <option value="go">围棋 (Go)</option>
          <option value="gomoku">五子棋 (Gomoku)</option>
        </select>
        <select v-model="playerColor" @change="initGame">
          <option :value="1">执黑先手</option>
          <option :value="2">执白后手</option>
        </select>
        <button @click="initGame">重新开始</button>
        <button @click="passMove" :disabled="aiThinking">停一手</button>
        <button @click="applyScore" :disabled="aiThinking">申请数子</button>
        <button @click="undoMove" :disabled="aiThinking || moveHistory.length === 0">悔棋</button>
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
          <p v-if="mode === 'go'" style="color: #888; font-size: 0.9em; margin-top: -10px;">贴目: {{ komi }}</p>
          <p class="status-text">{{ statusMessage }}</p>
          <p v-if="aiThinking" class="thinking-text">AI 正在思考中...</p>
        </div>

        <div class="win-rate-panel" v-if="aiHintEnabled">
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
// @ts-ignore
import initModule from '../game_core.js';

// 游戏配置
const mode = ref('go');
const playerColor = ref(1); // 1: 黑, 2: 白
const boardSize = ref(19);
const komi = ref(0);
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

// Wasm 实例
let wasmModule: any = null;
let wasmGame: any = null;

const initBoard = () => {
  boardState.value = new Array(boardSize.value * boardSize.value).fill(0);
  lastMove.value = null;
  currentHints.value = [];
  winRate.value = 0.5;
  moveHistory.value = [];
  statusMessage.value = playerColor.value === 1 ? '准备就绪，轮到黑方（你）落子' : '准备就绪，等待AI落子';

  if (wasmModule) {
    if (wasmGame) {
      wasmGame.delete();
    }
    wasmGame = new wasmModule.WasmGameWrapper(mode.value, boardSize.value);
  }
};

const undoMove = () => {
  if (aiThinking.value || moveHistory.value.length === 0) return;
  const lastMoveObj = moveHistory.value[moveHistory.value.length - 1];
  
  if (lastMoveObj.color !== playerColor.value && moveHistory.value.length >= 2) {
    moveHistory.value.pop();
    moveHistory.value.pop();
  } else {
    moveHistory.value.pop();
  }
  
  // 重构棋盘
  if (wasmGame) {
    const syncHistory = moveHistory.value.map(m => [m.x, m.y]);
    wasmGame.sync_state(syncHistory);
    boardState.value = Array.from(wasmGame.get_board());
  } else {
    boardState.value = new Array(boardSize.value * boardSize.value).fill(0);
    moveHistory.value.forEach(m => {
      boardState.value[m.y * boardSize.value + m.x] = m.color;
    });
  }
  
  if (moveHistory.value.length > 0) {
    const m = moveHistory.value[moveHistory.value.length - 1];
    lastMove.value = { x: m.x, y: m.y };
  } else {
    lastMove.value = null;
  }
  
  // 同步给后端
  if (ws && ws.readyState === WebSocket.OPEN) {
    const syncHistory = moveHistory.value.map(m => [m.x, m.y]);
    ws.send(JSON.stringify({ action: 'sync_history', history: syncHistory }));
  }
  
  statusMessage.value = '悔棋成功，轮到你落子';
  currentHints.value = [];
};

const passMove = () => {
  if (aiThinking.value || statusMessage.value.includes('结束')) return;
  if (ws && ws.readyState === WebSocket.OPEN) {
    ws.send(JSON.stringify({ action: 'play', move: [-1, -1] }));
    statusMessage.value = '你选择了停一手，等待 AI 响应...';
    aiThinking.value = true;
    currentHints.value = [];
  }
};

const applyScore = () => {
  if (aiThinking.value || statusMessage.value.includes('结束')) return;
  if (ws && ws.readyState === WebSocket.OPEN) {
    ws.send(JSON.stringify({ action: 'apply_score' }));
    statusMessage.value = '正在申请数子并计算胜负...';
    aiThinking.value = true;
  }
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
    
    if (data.board_state) {
      boardState.value = data.board_state;
    }
    
    if (data.action === 'ready') {
      if (data.board_size) {
        boardSize.value = data.board_size;
        komi.value = data.komi || 0;
        initBoard(); // 尺寸更新后重新初始化一次以避免越界
      }
      if (playerColor.value === 2) {
        statusMessage.value = 'AI 思考先手...';
        aiThinking.value = true;
        if (ws && ws.readyState === WebSocket.OPEN) {
          ws.send(JSON.stringify({ action: 'ai_move_request' }));
        }
      }
    }
    else if (data.action === 'sync_board') {
      // 仅用于同步盘面状态，消除提子等本地难以推演的变化
    }
    else if (data.action === 'illegal_move') {
      aiThinking.value = false;
      statusMessage.value = '落子非法（打劫或禁手）！请重新落子。';
      moveHistory.value.pop(); // 撤回刚才加入的历史
      currentHints.value = [];
      if (wasmGame) {
         const historyForWasm = moveHistory.value.map(m => [m.x, m.y]);
         wasmGame.sync_state(historyForWasm);
         boardState.value = Array.from(wasmGame.get_board());
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
      const aiColor = playerColor.value === 1 ? 2 : 1;
      // boardState 的赋值已经被 data.board_state 统一处理
      lastMove.value = { x, y };
      if (win_rate !== undefined) winRate.value = win_rate;
      if (x === -1 && y === -1) {
        moveHistory.value.push({x, y, color: aiColor, winRate: winRate.value});
        statusMessage.value = 'AI 选择了停一手，轮到你';
      } else {
        moveHistory.value.push({x, y, color: aiColor, winRate: winRate.value});
        statusMessage.value = 'AI 落子完毕，轮到你';
      }
      
      if (wasmGame) {
         wasmGame.play_move(x, y, aiColor);
         boardState.value = Array.from(wasmGame.get_board());
      }
      currentHints.value = [];
    }
    else if (data.action === 'score_rejected') {
      aiThinking.value = false;
      statusMessage.value = data.message;
    }
    else if (data.action === 'game_over') {
      aiThinking.value = false;
      let resText = "平局";
      if (data.reason) {
         resText = data.reason;
      } else if (data.diff !== undefined) {
         if (data.diff > 0) resText = `黑胜 ${data.diff} 目 (黑${data.b_s} 白${data.w_s}+贴目)`;
         else if (data.diff < 0) resText = `白胜 ${-data.diff} 目 (白${data.w_s}+贴目 黑${data.b_s})`;
      } else {
         if (data.result > 0) resText = "黑棋胜";
         else if (data.result < 0) resText = "白棋胜";
      }
      statusMessage.value = `游戏结束！${resText}`;
      
      // 保存棋谱
      const record = {
        id: Date.now().toString(),
        date: new Date().toLocaleString(),
        mode: mode.value,
        boardSize: boardSize.value,
        playerColor: playerColor.value,
        result: resText,
        history: moveHistory.value.map(m => [m.x, m.y])
      };
      
      const records = JSON.parse(localStorage.getItem('alphago_records') || '[]');
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

  if (wasmGame) {
    const isValid = wasmGame.play_move(x, y, playerColor.value);
    if (!isValid) {
      statusMessage.value = '此处非法落子或已有子！';
      return;
    }
    // 乐观渲染
    boardState.value = Array.from(wasmGame.get_board());
  } else {
    const index = y * boardSize.value + x;
    if (boardState.value[index] !== 0) {
      statusMessage.value = '此处已有子！';
      return;
    }
  }

  lastMove.value = { x, y };
  moveHistory.value.push({x, y, color: playerColor.value, winRate: winRate.value});
  statusMessage.value = '校验落子中，AI 准备思考...';
  aiThinking.value = true;
  currentHints.value = [];

  if (ws && ws.readyState === WebSocket.OPEN) {
    ws.send(JSON.stringify({ action: 'play', move: [x, y] }));
  }
};

onMounted(async () => {
  try {
    wasmModule = await initModule();
  } catch (e) {
    console.error("Wasm 模块加载失败: ", e);
  }
  initGame();
});

onUnmounted(() => {
  if (ws) ws.close();
  if (wasmGame) wasmGame.delete();
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
