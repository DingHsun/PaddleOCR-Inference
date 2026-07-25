import { defineConfig } from 'vite'
import vue from '@vitejs/plugin-vue'

// https://vite.dev/config/
export default defineConfig({
  plugins: [vue()],
  base: './',
  server: {
    // During `npm run dev`, forward API calls to api_server.exe so the
    // browser sees same-origin requests (no CORS setup needed).
    proxy: {
      '/health': 'http://127.0.0.1:8080',
      '/ocr_detect': 'http://127.0.0.1:8080',
      '/ocr_recognize': 'http://127.0.0.1:8080',
    },
  },
})
