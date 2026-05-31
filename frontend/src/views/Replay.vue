<template>
  <div class="replay-container">
    <div class="header">
      <h2>对局复盘</h2>
      <button class="back-btn" @click="goBack">返回列表</button>
    </div>

    <div v-if="!record" class="loading">
      正在加载棋谱...
    </div>
    <div v-else class="content-area">
      <!-- 左侧：棋盘 -->
      <div class="board-area">
        <Board 
          :board-size="boardSize" 
          :board-state="currentBoardState"
          :last-move="currentLastMove"
        />
        <div class="controls">
          <button @click="prevStep" :disabled="currentStep <= 0">上一步</button>
          <span>第 {{ currentStep }} / {{ totalSteps }} 手</span>
          <button @click="nextStep" :disabled="currentStep >= totalSteps">下一步</button>
        </div>
      </div>

      <!-- 右侧：数据与胜率分析 -->
      <div class="analysis-area">
        <div class="info-panel">
          <p><strong>对局模式：</strong> {{ record.mode === 'go' ? '围棋' : '五子棋' }}</p>
          <p><strong>对局时间：</strong> {{ new Date(record.timestamp).toLocaleString() }}</p>
          <p><strong>最终结果：</strong> {{ record.result }}</p>
        </div>
        
        <div class="chart-panel">
          <h3>黑方胜率走势</h3>
          <div ref="chartRef" class="echarts-container"></div>
        </div>
      </div>
    </div>
  </div>
</template>

<script setup lang="ts">
import { ref, computed, onMounted, onUnmounted, watch } from 'vue';
import { useRoute, useRouter } from 'vue-router';
import * as echarts from 'echarts';
import Board from '../components/Board.vue';

const route = useRoute();
const router = useRouter();

interface Move {
  x: number;
  y: number;
  color: number; // 1黑, 2白
  winRate: number;
}

interface Record {
  id: string;
  mode: string;
  boardSize?: number;
  timestamp: number;
  result: string;
  moves: Move[];
}

const record = ref<Record | null>(null);
const currentStep = ref(0);
const boardSize = computed(() => record.value?.boardSize || (record.value?.mode === 'go' ? 19 : 15));
const totalSteps = computed(() => record.value?.moves.length || 0);

const chartRef = ref<HTMLElement | null>(null);
let chartInstance: echarts.ECharts | null = null;

const goBack = () => {
  router.push('/records');
};

const loadRecord = () => {
  const id = route.params.id;
  const stored = localStorage.getItem('alphago_records');
  if (stored) {
    const records: Record[] = JSON.parse(stored);
    const found = records.find(r => r.id === id);
    if (found) {
      record.value = found;
      currentStep.value = found.moves.length; // 默认跳到最后一步
      setTimeout(() => {
        initChart();
      }, 100);
    }
  }
};

onMounted(() => {
  loadRecord();
});

onUnmounted(() => {
  if (chartInstance) {
    chartInstance.dispose();
  }
});

// 计算当前的棋盘状态
const currentBoardState = computed(() => {
  const state = new Array(boardSize.value * boardSize.value).fill(0);
  if (!record.value) return state;
  for (let i = 0; i < currentStep.value; i++) {
    const move = record.value.moves[i];
    state[move.y * boardSize.value + move.x] = move.color;
  }
  return state;
});

const currentLastMove = computed(() => {
  if (currentStep.value === 0 || !record.value) return null;
  const move = record.value.moves[currentStep.value - 1];
  return { x: move.x, y: move.y };
});

const prevStep = () => {
  if (currentStep.value > 0) currentStep.value--;
};

const nextStep = () => {
  if (currentStep.value < totalSteps.value) currentStep.value++;
};

// 监听步数变化，更新图表的 cursor
watch(currentStep, (newStep) => {
  if (chartInstance) {
    chartInstance.dispatchAction({
      type: 'showTip',
      seriesIndex: 0,
      dataIndex: newStep > 0 ? newStep - 1 : 0
    });
  }
});

const initChart = () => {
  if (!chartRef.value || !record.value) return;
  chartInstance = echarts.init(chartRef.value);

  const moves = record.value.moves;
  // 横坐标是手数
  const xAxisData = moves.map((_, i) => i + 1);
  // 纵坐标是胜率 (0-100)
  const yAxisData = moves.map(m => (m.winRate * 100).toFixed(1));

  const option = {
    tooltip: {
      trigger: 'axis',
      formatter: '第 {b} 手<br/>黑方胜率: {c}%'
    },
    xAxis: {
      type: 'category',
      data: xAxisData,
      name: '手数'
    },
    yAxis: {
      type: 'value',
      name: '胜率(%)',
      min: 0,
      max: 100
    },
    series: [
      {
        data: yAxisData,
        type: 'line',
        smooth: true,
        itemStyle: {
          color: '#2196F3'
        },
        areaStyle: {
          color: new echarts.graphic.LinearGradient(0, 0, 0, 1, [
            { offset: 0, color: 'rgba(33, 150, 243, 0.5)' },
            { offset: 1, color: 'rgba(33, 150, 243, 0)' }
          ])
        }
      }
    ]
  };

  chartInstance.setOption(option);

  // 图表点击事件，跳转到对应步数
  chartInstance.on('click', (params) => {
    if (params.dataIndex !== undefined) {
      currentStep.value = params.dataIndex + 1;
    }
  });
};
</script>

<style scoped>
.replay-container {
  max-width: 1200px;
  margin: 0 auto;
}
.header {
  display: flex;
  justify-content: space-between;
  align-items: center;
  margin-bottom: 20px;
  border-bottom: 1px solid #eaeaea;
  padding-bottom: 10px;
}
.back-btn {
  padding: 8px 16px;
  background-color: #2c3e50;
  color: white;
  border: none;
  border-radius: 4px;
  cursor: pointer;
}
.content-area {
  display: flex;
  gap: 30px;
  align-items: flex-start;
}
.board-area {
  flex: 1;
  display: flex;
  flex-direction: column;
  align-items: center;
}
.controls {
  margin-top: 20px;
  display: flex;
  gap: 15px;
  align-items: center;
}
.controls button {
  padding: 8px 16px;
  border-radius: 4px;
  border: 1px solid #ccc;
  background-color: #fff;
  cursor: pointer;
}
.controls button:disabled {
  background-color: #f5f5f5;
  color: #aaa;
  cursor: not-allowed;
}
.analysis-area {
  flex: 1;
  display: flex;
  flex-direction: column;
  gap: 20px;
}
.info-panel, .chart-panel {
  background: white;
  padding: 20px;
  border-radius: 8px;
  box-shadow: 0 2px 8px rgba(0,0,0,0.05);
}
.echarts-container {
  width: 100%;
  height: 350px;
}
</style>
