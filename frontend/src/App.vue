<template>
  <div class="app-container">
    <nav class="navbar">
      <div class="logo">AlphaGo Zero Web</div>
      <div class="nav-links">
        <router-link to="/play">实时对弈</router-link>
        <router-link to="/records">棋谱记录</router-link>
      </div>
    </nav>
    <main class="main-content">
      <router-view></router-view>
    </main>
  </div>
</template>

<script setup lang="ts">
// 这里作为全局布局，不需要太多逻辑
</script>

<style>
/* 全局基础样式 */
body {
  margin: 0;
  font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, "Helvetica Neue", Arial, sans-serif;
  background-color: #f0f2f5;
  color: #333;
}
.app-container {
  min-height: 100vh;
  display: flex;
  flex-direction: column;
}
.navbar {
  background-color: #2c3e50;
  color: white;
  padding: 0 40px;
  height: 60px;
  display: flex;
  align-items: center;
  justify-content: space-between;
  box-shadow: 0 2px 4px rgba(0,0,0,0.1);
}
.logo {
  font-size: 1.5rem;
  font-weight: bold;
  letter-spacing: 1px;
}
.nav-links {
  display: flex;
  gap: 20px;
}
.nav-links a {
  color: #a8b2bd;
  text-decoration: none;
  font-size: 1.1rem;
  transition: color 0.3s;
}
.nav-links a:hover, .nav-links a.router-link-active {
  color: #ffffff;
}
.main-content {
  flex: 1;
  padding: 20px;
}
</style>
