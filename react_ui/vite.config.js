import { defineConfig } from 'vite'
import react from '@vitejs/plugin-react'
import { viteSingleFile } from 'vite-plugin-singlefile'
import path from 'path'
import dns from 'node:dns/promises'

async function resolveKiwiTarget() {
  if (process.env.KIWI_TARGET) return process.env.KIWI_TARGET
  try {
    const res = await Promise.race([
      dns.lookup('kiwi.local', { family: 4 }),
      new Promise((_, rej) => setTimeout(() => rej(new Error('timeout')), 1500))
    ])
    if (res?.address) return `http://${res.address}`
  } catch {}
  return 'http://kiwi.local'
}

// https://vite.dev/config/
export default defineConfig(async ({ command }) => {
  const isBuild = command === 'build'
  const kiwiTarget = isBuild ? 'http://kiwi.local' : await resolveKiwiTarget()

  return {
    plugins: isBuild ? [react(), viteSingleFile()] : [react()],
    resolve: isBuild
      ? {
          alias: [
            { find: '@', replacement: path.resolve(__dirname, './src') },
            { find: /^react-dom\/client$/, replacement: 'preact/compat/client' },
            { find: /^react-dom\/test-utils$/, replacement: 'preact/test-utils' },
            { find: /^react-dom$/, replacement: 'preact/compat' },
            { find: /^react\/jsx-runtime$/, replacement: 'preact/compat/jsx-runtime' },
            { find: /^react\/jsx-dev-runtime$/, replacement: 'preact/compat/jsx-dev-runtime' },
            { find: /^react$/, replacement: 'preact/compat' },
          ],
        }
      : {
          alias: [{ find: '@', replacement: path.resolve(__dirname, './src') }],
        },
    server: {
      host: true,
      port: 5173,
      proxy: {
        '/status': { target: kiwiTarget, changeOrigin: true },
        '/logs': { target: kiwiTarget, changeOrigin: true },
        '/config': { target: kiwiTarget, changeOrigin: true },
        '/do': { target: kiwiTarget, changeOrigin: true },
        '/save': { target: kiwiTarget, changeOrigin: true },
        '/animations': { target: kiwiTarget, changeOrigin: true },
        '/scan': { target: kiwiTarget, changeOrigin: true },
        '/ota': { target: kiwiTarget, changeOrigin: true },
        '/api': { target: kiwiTarget, changeOrigin: true },
        '/apple-touch-icon.png': { target: kiwiTarget, changeOrigin: true },
        '/manifest.json': { target: kiwiTarget, changeOrigin: true },
      },
    },
    build: {
      target: 'esnext',
      assetsInlineLimit: 100000000,
      chunkSizeWarningLimit: 100000000,
      cssCodeSplit: false,
    },
  }
})
