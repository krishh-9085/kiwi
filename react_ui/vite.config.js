import { defineConfig } from 'vite'
import react from '@vitejs/plugin-react'
import { viteSingleFile } from 'vite-plugin-singlefile'
import path from 'path'

// https://vite.dev/config/
export default defineConfig({
  plugins: [react(), viteSingleFile()],
  resolve: {
    alias: {
      '@': path.resolve(__dirname, './src'),
    },
  },
  server: {
    port: 5173,
    proxy: {
      '/status': 'http://localhost:8080',
      '/logs': 'http://localhost:8080',
      '/config': 'http://localhost:8080',
      '/do': 'http://localhost:8080',
      '/save': 'http://localhost:8080',
      '/animations': 'http://localhost:8080',
      '/scan': 'http://localhost:8080',
      '/ota': 'http://localhost:8080',
      '/api': 'http://localhost:8080',
    }
  },
  build: {
    target: 'esnext',
    assetsInlineLimit: 100000000,
    chunkSizeWarningLimit: 100000000,
    cssCodeSplit: false,
  },
})
