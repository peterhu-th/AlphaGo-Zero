<template>
  <div class="board-container">
    <canvas ref="boardCanvas" @click="handleCanvasClick"></canvas>
  </div>
</template>

<script setup lang="ts">
import { ref, onMounted, watch } from 'vue';

const props = defineProps({
  // 棋盘大小，默认 19 用于围棋，15 用于五子棋
  boardSize: {
    type: Number,
    default: 19
  },
  // 棋局状态数组，一维数组长度 boardSize * boardSize，0: 空, 1: 黑, 2: 白
  boardState: {
    type: Array as () => number[],
    default: () => []
  },
  // 提示点位及其胜率/概率，用于 AI 提示
  hints: {
    type: Array as () => { x: number, y: number, prob: number, winRate: number }[],
    default: () => []
  },
  // 最新落子，用于高亮显示
  lastMove: {
    type: Object as () => { x: number, y: number } | null,
    default: null
  }
});

const emit = defineEmits(['move']);

const boardCanvas = ref<HTMLCanvasElement | null>(null);
const ctx = ref<CanvasRenderingContext2D | null>(null);

// 棋盘绘制相关常量配置
const cellSize = 30; // 网格大小
const margin = 40;   // 留白大小，拉开文字与棋盘的距离

// 初始化画布
const initCanvas = () => {
  const canvas = boardCanvas.value;
  if (!canvas) return;
  // 加上两侧坐标预留边距
  const size = (props.boardSize - 1) * cellSize + margin * 2;
  const dpr = window.devicePixelRatio || 1;
  
  canvas.width = size * dpr;
  canvas.height = size * dpr;
  // 移除硬编码的 style 宽高，交由 CSS 控制，实现自由缩放
  
  const context = canvas.getContext('2d');
  if (context) {
    context.scale(dpr, dpr);
    ctx.value = context;
    draw();
  }
};

// 绘制星位
const drawStarPoints = (context: CanvasRenderingContext2D) => {
  let stars: number[] = [];
  if (props.boardSize === 19) {
    stars = [3, 9, 15];
  } else if (props.boardSize === 15) {
    stars = [3, 7, 11];
  } else if (props.boardSize === 9) {
    stars = [2, 4, 6];
  } else {
    return;
  }
  
  context.fillStyle = '#000';
  for (let i of stars) {
    for (let j of stars) {
      // 五子棋通常只画5个星位（四角和中心），或者全部9个都可以
      // 为简化，这里直接绘制交叉点
      context.beginPath();
      context.arc(margin + i * cellSize, margin + j * cellSize, 3, 0, 2 * Math.PI);
      context.fill();
    }
  }
};

// 绘制整个棋盘
const draw = () => {
  if (!ctx.value) return;
  const context = ctx.value;
  const canvas = boardCanvas.value;
  if (!canvas) return;

  // 清空画布并填充背景底色（木纹色系）
  context.clearRect(0, 0, canvas.width, canvas.height);
  context.fillStyle = '#DCB35C'; // 暖木色背景
  context.fillRect(0, 0, canvas.width, canvas.height);

  // 绘制网格线
  context.strokeStyle = '#000';
  context.lineWidth = 1;
  for (let i = 0; i < props.boardSize; i++) {
    // 纵线
    context.beginPath();
    context.moveTo(margin + i * cellSize, margin);
    context.lineTo(margin + i * cellSize, margin + (props.boardSize - 1) * cellSize);
    context.stroke();
    // 横线
    context.beginPath();
    context.moveTo(margin, margin + i * cellSize);
    context.lineTo(margin + (props.boardSize - 1) * cellSize, margin + i * cellSize);
    context.stroke();
  }

  // 绘制坐标轴 (A-T 省略I，1-19)
  context.fillStyle = '#666';
  context.font = '12px Arial';
  context.textAlign = 'center';
  context.textBaseline = 'middle';
  const getChar = (idx: number) => {
    let charCode = 'A'.charCodeAt(0) + idx;
    if (charCode >= 'I'.charCodeAt(0)) charCode++; // 通常围棋坐标跳过 I
    return String.fromCharCode(charCode);
  };
  
  for (let i = 0; i < props.boardSize; i++) {
    // 顶边字母 (距离上方约 margin * 0.4 的位置)
    context.fillText(getChar(i), margin + i * cellSize, margin * 0.4);
    // 左边数字
    context.fillText((props.boardSize - i).toString(), margin * 0.4, margin + i * cellSize);
  }

  // 绘制星位
  drawStarPoints(context);

  // 绘制棋子
  props.boardState.forEach((state, index) => {
    if (state === 0) return;
    const x = index % props.boardSize;
    const y = Math.floor(index / props.boardSize);
    drawStone(context, x, y, state);
  });

  // 绘制最新落子的高亮标识
  if (props.lastMove) {
    const { x, y } = props.lastMove;
    context.strokeStyle = '#f00'; // 红色高亮边框
    context.lineWidth = 2;
    context.beginPath();
    context.arc(margin + x * cellSize, margin + y * cellSize, cellSize / 2 - 2, 0, 2 * Math.PI);
    context.stroke();
  }

  // 绘制 AI 提示
  if (props.hints && props.hints.length > 0) {
    props.hints.forEach(hint => {
      drawHint(context, hint.x, hint.y, hint.winRate);
    });
  }
};

// 绘制单个棋子
const drawStone = (context: CanvasRenderingContext2D, x: number, y: number, state: number) => {
  context.beginPath();
  context.arc(margin + x * cellSize, margin + y * cellSize, cellSize / 2 - 1, 0, 2 * Math.PI);
  
  // 简单的渐变，增加立体感
  const gradient = context.createRadialGradient(
    margin + x * cellSize - 4, margin + y * cellSize - 4, 1,
    margin + x * cellSize, margin + y * cellSize, cellSize / 2
  );

  if (state === 1) { // 黑子
    gradient.addColorStop(0, '#666');
    gradient.addColorStop(1, '#000');
  } else { // 白子
    gradient.addColorStop(0, '#fff');
    gradient.addColorStop(1, '#ccc');
  }
  
  context.fillStyle = gradient;
  context.fill();
  context.lineWidth = 0.5;
  context.strokeStyle = '#333';
  context.stroke();
};

// 绘制提示点
const drawHint = (context: CanvasRenderingContext2D, x: number, y: number, winRate: number) => {
  context.beginPath();
  context.arc(margin + x * cellSize, margin + y * cellSize, 6, 0, 2 * Math.PI);
  // 根据胜率设置颜色，胜率越高越偏蓝，越低越偏红
  const r = Math.floor(255 * (1 - winRate));
  const b = Math.floor(255 * winRate);
  context.fillStyle = `rgba(${r}, 0, ${b}, 0.8)`;
  context.fill();
  
  // 绘制胜率文字
  context.fillStyle = '#fff';
  context.font = '10px Arial';
  context.textAlign = 'center';
  context.textBaseline = 'middle';
  context.fillText((winRate * 100).toFixed(0) + '%', margin + x * cellSize, margin + y * cellSize);
};

// 处理点击落子
const handleCanvasClick = (event: MouseEvent) => {
  const canvas = boardCanvas.value;
  if (!canvas) return;
  const rect = canvas.getBoundingClientRect();
  
  // 增加缩放系数，保证 CSS 缩放后点击坐标准确
  const scaleX = canvas.width / rect.width;
  const scaleY = canvas.height / rect.height;
  
  const clickX = (event.clientX - rect.left) * scaleX;
  const clickY = (event.clientY - rect.top) * scaleY;

  // 计算最近的交叉点
  const x = Math.round((clickX - margin) / cellSize);
  const y = Math.round((clickY - margin) / cellSize);

  if (x >= 0 && x < props.boardSize && y >= 0 && y < props.boardSize) {
    emit('move', { x, y });
  }
};

// 监听状态变化重新渲染
watch(() => props.boardState, draw, { deep: true });
watch(() => props.hints, draw, { deep: true });
watch(() => props.lastMove, draw, { deep: true });
watch(() => props.boardSize, initCanvas);

onMounted(() => {
  initCanvas();
});
</script>

<style scoped>
.board-container {
  display: flex;
  justify-content: center;
  align-items: center;
  padding: 30px;
  background: linear-gradient(145deg, #ffffff, #f0f0f0);
  border-radius: 16px;
  box-shadow: 10px 10px 20px #d9d9d9, -10px -10px 20px #ffffff;
  width: 100%;
  aspect-ratio: 1 / 1;
  margin: 0 auto;
}
canvas {
  cursor: pointer;
  width: 100%;
  height: auto;
  aspect-ratio: 1 / 1;
  border-radius: 4px;
  box-shadow: inset 0 0 10px rgba(0,0,0,0.5), 0 15px 30px rgba(0,0,0,0.3);
}
</style>
