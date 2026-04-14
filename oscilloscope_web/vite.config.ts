import { defineConfig } from 'vite';
import react from '@vitejs/plugin-react';
import path from 'path';

export default defineConfig({
  build: {
    // Specify the output directory (relative to project root)
    outDir: '../cmake-build-debug/oscilloscope_backend/static/oscilloscope_web',

    // Optional: Empty the output directory before building
    // If outDir is outside root, you MUST set this to true
    emptyOutDir: false,
  },
  plugins: [react()],
  resolve: {
    alias: {
      '@/app':       path.resolve(__dirname, 'src/app'),
      '@/pages':     path.resolve(__dirname, 'src/pages'),
      '@/widgets':   path.resolve(__dirname, 'src/widgets'),
      '@/features':  path.resolve(__dirname, 'src/features'),
      '@/entities':  path.resolve(__dirname, 'src/entities'),
      '@/shared':    path.resolve(__dirname, 'src/shared'),
      '@/generated': path.resolve(__dirname, 'src/generated'),
    },
  },
  server: {
    proxy: {
      // ── buf_connect_server built-in services (auth / session / admin) ──
      '/buf_connect_server.v2': {
        target:       'http://oscilloscope.local',
        changeOrigin: true,
      },
      // ── oscilloscope device-specific services ──────────────────────────
      '/oscilloscope_interface.v2': {
        target:       'http://oscilloscope.local',
        changeOrigin: true,
      },
    },
  },
});
