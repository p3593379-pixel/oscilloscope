// FILE: control_panel_web/src/hooks/useConfig.ts
import { useState, useEffect, useCallback } from 'react';
import { configClient, type ServerConfig } from 'api/configClient';

export function useConfig() {
    const [config, setConfig] = useState<ServerConfig | null>(null);
    const [loading, setLoading] = useState(true);
    const [error, setError] = useState<string | null>(null);
    const [saving, setSaving] = useState(false);

    const reload = useCallback(async () => {
        setLoading(true);
        setError(null);
        const r = await configClient.getAll();
        if (r.error) setError(r.error);
        else if (r.data) setConfig(r.data);
        setLoading(false);
    }, []);

    useEffect(() => { reload(); }, [reload]);

    const save = useCallback(async (updated: ServerConfig): Promise<boolean> => {
        setSaving(true);
        const r = await configClient.putAll(updated);
        setSaving(false);
        if (r.error) { setError(r.error); return false; }
        if (r.data) setConfig(r.data);
        return true;
    }, []);

    return { config, loading, error, saving, save, reload };
}
