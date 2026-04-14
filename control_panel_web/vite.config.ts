// FILE: control_panel_web/vite.config.ts
import { defineConfig } from 'vite';
import react from '@vitejs/plugin-react';
import tsconfigPaths from 'vite-tsconfig-paths'

export default defineConfig({
    plugins: [
        react(),
        tsconfigPaths()
    ],
    build: {
        outDir: '../cmake-build-debug/oscilloscope_backend/static/control_panel_web',
        emptyOutDir: true,
    },
    server: {
        proxy: {
            '/api': {
                target: 'http://oscilloscope.local',
                changeOrigin: true,
            },
        },
    },
});
