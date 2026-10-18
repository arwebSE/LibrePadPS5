import { defineConfig } from 'vite';
import vue from '@vitejs/plugin-vue';
import tailwindcss from '@tailwindcss/vite';

export default defineConfig({
  plugins: [vue(), tailwindcss()],
  build: {
    target: 'es2020',
    cssCodeSplit: false,
    rolldownOptions: { output: {
      entryFileNames: 'dashboard.js',
      codeSplitting: false,
      assetFileNames: 'dashboard.[ext]',
    } },
  },
  server: { proxy: { '/api': process.env.LIBREPAD_PS5_URL || 'http://127.0.0.1:8096' } },
  test: { environment: 'jsdom', include: ['tests/**/*.test.js'], restoreMocks: true },
});
