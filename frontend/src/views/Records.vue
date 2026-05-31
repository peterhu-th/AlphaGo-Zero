<template>
  <div class="records-container">
    <h2>历史对局棋谱</h2>
    <div v-if="records.length === 0" class="empty-state">
      <p>暂无历史棋谱，去下两盘吧！</p>
    </div>
    <div class="record-grid">
      <div v-for="record in records" :key="record.id" class="record-card" @click="goToReplay(record.id)">
        <div class="record-header">
          <span class="mode-badge">{{ record.mode === 'go' ? '围棋' : '五子棋' }}</span>
          <span class="date">{{ formatDate(record.timestamp) }}</span>
        </div>
        <div class="record-body">
          <p><strong>结果:</strong> {{ record.result }}</p>
          <p><strong>总手数:</strong> {{ record.moves.length }}手</p>
        </div>
        <div class="record-footer">
          <button class="replay-btn">查看复盘</button>
        </div>
      </div>
    </div>
  </div>
</template>

<script setup lang="ts">
import { ref, onMounted } from 'vue';
import { useRouter } from 'vue-router';

const router = useRouter();

interface Record {
  id: string;
  mode: string;
  timestamp: number;
  result: string;
  moves: {x: number, y: number, color: number, winRate: number}[];
}

const records = ref<Record[]>([]);

onMounted(() => {
  // 从 localStorage 获取历史棋谱
  const stored = localStorage.getItem('alphago_records');
  if (stored) {
    try {
      records.value = JSON.parse(stored).sort((a: Record, b: Record) => b.timestamp - a.timestamp);
    } catch(e) {
      console.error('解析棋谱失败', e);
    }
  }
});

const formatDate = (timestamp: number) => {
  return new Date(timestamp).toLocaleString();
};

const goToReplay = (id: string) => {
  router.push(`/replay/${id}`);
};
</script>

<style scoped>
.records-container {
  max-width: 1000px;
  margin: 0 auto;
}
.empty-state {
  text-align: center;
  padding: 50px;
  color: #888;
}
.record-grid {
  display: grid;
  grid-template-columns: repeat(auto-fill, minmax(280px, 1fr));
  gap: 20px;
  margin-top: 20px;
}
.record-card {
  background: white;
  border-radius: 8px;
  padding: 15px;
  box-shadow: 0 2px 10px rgba(0,0,0,0.05);
  cursor: pointer;
  transition: transform 0.2s, box-shadow 0.2s;
  border: 1px solid #eaeaea;
}
.record-card:hover {
  transform: translateY(-5px);
  box-shadow: 0 5px 15px rgba(0,0,0,0.1);
}
.record-header {
  display: flex;
  justify-content: space-between;
  align-items: center;
  margin-bottom: 15px;
}
.mode-badge {
  background: #2c3e50;
  color: white;
  padding: 4px 8px;
  border-radius: 4px;
  font-size: 0.8rem;
}
.date {
  font-size: 0.85rem;
  color: #888;
}
.record-body p {
  margin: 8px 0;
  color: #444;
}
.record-footer {
  margin-top: 15px;
  border-top: 1px solid #eee;
  padding-top: 10px;
  text-align: right;
}
.replay-btn {
  background: none;
  border: none;
  color: #2196F3;
  cursor: pointer;
  font-weight: bold;
}
.replay-btn:hover {
  text-decoration: underline;
}
</style>
