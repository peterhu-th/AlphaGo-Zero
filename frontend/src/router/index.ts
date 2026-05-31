import { createRouter, createWebHistory } from 'vue-router';
import Play from '../views/Play.vue';

const router = createRouter({
  history: createWebHistory(),
  routes: [
    {
      path: '/',
      redirect: '/play'
    },
    {
      path: '/play',
      name: 'Play',
      component: Play
    },
    {
      path: '/records',
      name: 'Records',
      component: () => import('../views/Records.vue') // 懒加载
    },
    {
      path: '/replay/:id',
      name: 'Replay',
      component: () => import('../views/Replay.vue') // 懒加载
    }
  ]
});

export default router;
