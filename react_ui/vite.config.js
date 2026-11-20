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
    host: true,
    port: 5173,
    proxy: {
      '/status': { target: 'http://192.168.1.16', changeOrigin: true },
      '/logs': { target: 'http://192.168.1.16', changeOrigin: true },
      '/config': { target: 'http://192.168.1.16', changeOrigin: true },
      '/do': { target: 'http://192.168.1.16', changeOrigin: true },
      '/save': { target: 'http://192.168.1.16', changeOrigin: true },
      '/animations': { target: 'http://192.168.1.16', changeOrigin: true },
      '/scan': { target: 'http://192.168.1.16', changeOrigin: true },
      '/ota': { target: 'http://192.168.1.16', changeOrigin: true },
      '/api': { target: 'http://192.168.1.16', changeOrigin: true },
    }
  },
  build: {
    target: 'esnext',
    assetsInlineLimit: 100000000,
    chunkSizeWarningLimit: 100000000,
    cssCodeSplit: false,
  },
})
