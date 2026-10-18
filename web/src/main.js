import { createApp } from 'vue';
import App from './App.vue';
import './styles.css';
async function start() {
  if (import.meta.env.DEV && new URLSearchParams(location.search).has('preview')) {
    const { enablePreview } = await import('./dev/preview');
    enablePreview();
  }
  createApp(App).mount('#app');
}
start();
