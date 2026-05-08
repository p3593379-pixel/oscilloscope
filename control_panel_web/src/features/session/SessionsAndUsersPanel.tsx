// FILE: control_panel_web/src/features/session/SessionsAndUsersPanel.tsx
import { useEffect, useState, useCallback } from 'react';
import { configClient, type SessionConfig, type UserInfo, type UserRole } from 'api/configClient';

// ── shared styles ─────────────────────────────────────────────────────────
const inp: React.CSSProperties = {
    background: '#313244', border: '1px solid #45475a', color: '#cdd6f4',
    borderRadius: 4, padding: '4px 8px', fontSize: 14,
};
const saveBtn: React.CSSProperties = {
    background: '#89b4fa', color: '#1e1e2e', border: 'none',
    padding: '6px 20px', borderRadius: 4, cursor: 'pointer', fontSize: 14, fontWeight: 600,
};
const outlineBtn: React.CSSProperties = {
    background: '#313244', color: '#cdd6f4', border: '1px solid #45475a',
    padding: '4px 12px', borderRadius: 4, cursor: 'pointer', fontSize: 13,
};
const dangerBtn: React.CSSProperties = { ...outlineBtn, color: '#f38ba8', borderColor: '#f38ba8' };
const secLabel: React.CSSProperties = {
    fontSize: 13, textTransform: 'uppercase' as const,
    letterSpacing: 1, color: '#6c7086', marginBottom: 12,
};
const th: React.CSSProperties = { padding: '6px 8px', fontWeight: 600, fontSize: 12, color: '#6c7086' };
const td: React.CSSProperties = { padding: '7px 8px', color: '#cdd6f4' };

function Row({ label, hint, children }: { label: string; hint?: string; children: React.ReactNode }) {
    return (
        <div style={{ marginBottom: 14 }}>
            <label style={{ display: 'block', marginBottom: 4, fontSize: 14, color: '#cdd6f4' }}>
                {label}
                {hint && <span style={{ marginLeft: 8, fontSize: 12, color: '#6c7086' }}>{hint}</span>}
            </label>
            {children}
        </div>
    );
}

function Modal({ title, children }: { title: string; onClose: () => void; children: React.ReactNode }) {
    return (
        <div style={{ position: 'fixed', inset: 0, background: 'rgba(0,0,0,.6)',
            display: 'flex', alignItems: 'center', justifyContent: 'center', zIndex: 100 }}>
            <div style={{ background: '#1e1e2e', borderRadius: 8, padding: 24, minWidth: 320, maxWidth: 440 }}>
                <h3 style={{ color: '#cdd6f4', marginBottom: 16 }}>{title}</h3>
                {children}
            </div>
        </div>
    );
}

const ROLE_LABELS: Record<UserRole, string> = { admin: 'Admin', engineer: 'Engineer' };
function formatTs(ts: number) { return ts ? new Date(ts * 1000).toLocaleString() : '—'; }

// ── Session settings sub-section ──────────────────────────────────────────
function SessionSection() {
    const [cfg, setCfg]       = useState<SessionConfig | null>(null);
    const [saving, setSaving] = useState(false);
    const [msg, setMsg]       = useState('');

    useEffect(() => { configClient.getSession().then(r => r.data && setCfg(r.data)); }, []);

    const set   = (patch: Partial<SessionConfig>) => setCfg(c => c ? { ...c, ...patch } : c);
    const save  = async () => {
        if (!cfg) return;
        setSaving(true); setMsg('');
        const r = await configClient.putSession(cfg);
        setSaving(false);
        setMsg(r.error ? `Error: ${r.error}` : 'Saved.');
    };

    if (!cfg) return <p style={{ color: '#a6adc8', fontSize: 13 }}>Loading…</p>;

    return (
        <div style={{display: "flex", flexDirection: "column"}}>
            <h3 style={{ ...secLabel, marginTop: 0 }}>Concurrency</h3>
            <Row label="Admin conflict timeout" hint="seconds">
                <input type="number" style={{ ...inp, width: 120 }} value={cfg.admin_conflict_timeout_seconds}
                       onChange={e => set({ admin_conflict_timeout_seconds: Number(e.target.value) })} />
            </Row>
            <Row label="Snooze duration" hint="seconds — pause before kicking idle admin">
                <input type="number" style={{ ...inp, width: 120 }} value={cfg.snooze_duration_seconds}
                       onChange={e => set({ snooze_duration_seconds: Number(e.target.value) })} />
            </Row>
            <Row label="Max concurrent sessions" hint="0 = unlimited">
                <input type="number" style={{ ...inp, width: 120 }} value={cfg.max_concurrent_sessions}
                       onChange={e => set({ max_concurrent_sessions: Number(e.target.value) })} />
            </Row>

            <h3 style={{ ...secLabel, marginTop: 20 }}>Token &amp; Grace Periods</h3>
            <Row label="Call token renew period" hint="seconds">
                <input type="number" style={{ ...inp, width: 120 }} value={cfg.call_token_renew_period}
                       onChange={e => set({ call_token_renew_period: Number(e.target.value) })} />
            </Row>
            <Row label="Grace period — admin" hint="seconds">
                <input type="number" style={{ ...inp, width: 120 }} value={cfg.grace_period_admin_seconds}
                       onChange={e => set({ grace_period_admin_seconds: Number(e.target.value) })} />
            </Row>
            <Row label="Grace period — engineer" hint="seconds">
                <input type="number" style={{ ...inp, width: 120 }} value={cfg.grace_period_engineer_seconds}
                       onChange={e => set({ grace_period_engineer_seconds: Number(e.target.value) })} />
            </Row>

            <div style={{ display: 'flex', gap: 12, alignItems: 'center', marginTop: 4, marginBottom: 36 }}>
                <button onClick={save} disabled={saving} style={saveBtn}>
                    {saving ? 'Saving…' : 'Save Session Settings'}
                </button>
                {msg && <span style={{ fontSize: 13, color: msg.startsWith('Error') ? '#f38ba8' : '#a6e3a1' }}>{msg}</span>}
            </div>
        </div>
    );
}

// ── Users sub-section ─────────────────────────────────────────────────────
interface AddForm  { username: string; password: string; role: UserRole; }
interface ResetForm { username: string; new_password: string; }

function UsersSection() {
    const [users,     setUsers]     = useState<UserInfo[]>([]);
    const [err,       setErr]       = useState('');
    const [loading,   setLoading]   = useState(true);
    const [showAdd,   setShowAdd]   = useState(false);
    const [showReset, setShowReset] = useState<ResetForm | null>(null);
    const [showDel,   setShowDel]   = useState<UserInfo | null>(null);
    const [addForm,   setAddForm]   = useState<AddForm>({ username: '', password: '', role: 'engineer' });
    const [formErr,   setFormErr]   = useState('');

    const [dbPath,       setDbPath]       = useState('');

    const loadUsers = useCallback(async () => {
        setLoading(true); setErr('');
        const r = await configClient.listUsers();
        if (r.error) setErr(r.error); else setUsers(r.data?.users ?? []);
        setLoading(false);
    }, []);

    useEffect(() => {
        loadUsers();
        configClient.getUserDb().then(r => {
            if (r.data) { setDbPath(r.data.user_db_path); setDbPath(r.data.user_db_path); }
        });
    }, [loadUsers]);


    const createUser = async () => {
        if (!addForm.username || !addForm.password) { setFormErr('All fields required.'); return; }
        const r = await configClient.createUser(addForm);
        if (r.error) { setFormErr(r.error); return; }
        setShowAdd(false); setAddForm({ username: '', password: '', role: 'engineer' }); setFormErr('');
        loadUsers();
    };

    const resetPwd = async () => {
        if (!showReset?.new_password) { setFormErr('Password required.'); return; }
        const r = await configClient.resetPassword(showReset.username, showReset.new_password);
        if (r.error) { setFormErr(r.error); return; }
        setShowReset(null); setFormErr('');
    };

    const deleteUser = async () => {
        if (!showDel) return;
        const r = await configClient.deleteUser(showDel.user_id.toString());
        if (r.error) { setErr(r.error); return; }
        setShowDel(null); loadUsers();
    };

    return (
        <>
            {/* DB path — read-only */}
            <section style={{ background: '#313244', borderRadius: 6, padding: '12px 16px', marginBottom: 20,
                display: 'flex', alignItems: 'center', gap: 14 }}>
                <span style={{ fontSize: 12, color: '#6c7086', whiteSpace: 'nowrap',
                textTransform: 'uppercase', letterSpacing: 0.5 }}>Database</span>
                {dbPath === null
                    ? <span style={{ fontSize: 13, color: '#6c7086' }}>Loading…</span>
                    : <code style={{ fontSize: 13, color: '#a6e3a1', background: '#1e1e2e',
                        padding: '2px 8px', borderRadius: 4 }}>{dbPath}</code>
                }
            </section>

            {/* User list header */}
            <div style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'center', marginBottom: 10 }}>
                <h3 style={{ ...secLabel, marginBottom: 0 }}>Users</h3>
                <button onClick={() => { setShowAdd(true); setFormErr(''); }} style={outlineBtn}>+ Add User</button>
            </div>

            {loading ? <p style={{ color: '#a6adc8', fontSize: 13 }}>Loading…</p>
                : err   ? <p style={{ color: '#f38ba8', fontSize: 13 }}>Error: {err}</p>
                    : (
                        <table style={{ width: '100%', borderCollapse: 'collapse', fontSize: 13 }}>
                            <thead>
                            <tr style={{ textAlign: 'left' }}>
                                <th style={th}>Username</th>
                                <th style={th}>Role</th>
                                <th style={th}>Created</th>
                                <th style={th}>Last Login</th>
                                <th style={th}></th>
                            </tr>
                            </thead>
                            <tbody>
                            {users.map(u => (
                                <tr key={u.user_id} style={{ borderTop: '1px solid #313244' }}>
                                    <td style={td}>{u.username}</td>
                                    <td style={td}>
                                    <span style={{
                                        background: u.role === 'admin' ? '#313244' : '#1e1e2e',
                                        border: `1px solid ${u.role === 'admin' ? '#89b4fa' : '#45475a'}`,
                                        color: u.role === 'admin' ? '#89b4fa' : '#a6adc8',
                                        borderRadius: 10, padding: '1px 8px', fontSize: 12,
                                    }}>{ROLE_LABELS[u.role]}</span>
                                    </td>
                                    <td style={{ ...td, color: '#6c7086' }}>{formatTs(u.created_at)}</td>
                                    <td style={{ ...td, color: '#6c7086' }}>{formatTs(u.last_login)}</td>
                                    <td style={{ ...td, display: 'flex', gap: 6 }}>
                                        <button style={outlineBtn}
                                                onClick={() => { setShowReset({ username: u.username, new_password: '' }); setFormErr(''); }}>
                                            Reset password
                                        </button>
                                        <button style={dangerBtn} onClick={() => setShowDel(u)}>Delete</button>
                                    </td>
                                </tr>
                            ))}
                            {users.length === 0 && (
                                <tr><td colSpan={5} style={{ padding: 20, textAlign: 'center', color: '#6c7086' }}>
                                    No users yet.
                                </td></tr>
                            )}
                            </tbody>
                        </table>
                    )}

            {showAdd && (
                <Modal title="Add User" onClose={() => setShowAdd(false)}>
                    {(['username', 'password'] as const).map(f => (
                        <label key={f} style={{ display: 'block', marginBottom: 12 }}>
                            <span style={{ display: 'block', fontSize: 13, color: '#6c7086', marginBottom: 4 }}>
                                {f.charAt(0).toUpperCase() + f.slice(1)}
                            </span>
                            <input type={f === 'password' ? 'password' : 'text'}
                                   value={addForm[f]}
                                   onChange={e => setAddForm({ ...addForm, [f]: e.target.value })}
                                   style={{ ...inp, width: '100%' }} />
                        </label>
                    ))}
                    <label style={{ display: 'block', marginBottom: 16 }}>
                        <span style={{ display: 'block', fontSize: 13, color: '#6c7086', marginBottom: 4 }}>Role</span>
                        <select value={addForm.role}
                                onChange={e => setAddForm({ ...addForm, role: e.target.value as UserRole })}
                                style={{ ...inp, width: '100%' }}>
                            <option value="engineer">Engineer</option>
                            <option value="admin">Admin</option>
                        </select>
                    </label>
                    {formErr && <p style={{ color: '#f38ba8', fontSize: 13, marginBottom: 10 }}>{formErr}</p>}
                    <div style={{ display: 'flex', gap: 8, justifyContent: 'flex-end' }}>
                        <button style={outlineBtn} onClick={() => setShowAdd(false)}>Cancel</button>
                        <button style={saveBtn} onClick={createUser}>Create</button>
                    </div>
                </Modal>
            )}
            {showReset && (
                <Modal title={`Reset password — ${showReset.username}`} onClose={() => setShowReset(null)}>
                    <label style={{ display: 'block', marginBottom: 16 }}>
                        <span style={{ display: 'block', fontSize: 13, color: '#6c7086', marginBottom: 4 }}>New password</span>
                        <input type="password" value={showReset.new_password}
                               onChange={e => setShowReset({ ...showReset, new_password: e.target.value })}
                               style={{ ...inp, width: '100%' }} />
                    </label>
                    {formErr && <p style={{ color: '#f38ba8', fontSize: 13, marginBottom: 10 }}>{formErr}</p>}
                    <div style={{ display: 'flex', gap: 8, justifyContent: 'flex-end' }}>
                        <button style={outlineBtn} onClick={() => setShowReset(null)}>Cancel</button>
                        <button style={saveBtn} onClick={resetPwd}>Save</button>
                    </div>
                </Modal>
            )}
            {showDel && (
                <Modal title="Delete user?" onClose={() => setShowDel(null)}>
                    <p style={{ fontSize: 13, color: '#6c7086', marginBottom: 20 }}>
                        "{showDel.username}" will be permanently removed.
                    </p>
                    <div style={{ display: 'flex', gap: 8, justifyContent: 'flex-end' }}>
                        <button style={outlineBtn} onClick={() => setShowDel(null)}>Cancel</button>
                        <button style={dangerBtn} onClick={deleteUser}>Delete</button>
                    </div>
                </Modal>
            )}
        </>
    );
}

// ── Root panel ────────────────────────────────────────────────────────────
export default function SessionsAndUsersPanel() {
    return (
        <div>
            <h2 style={{ marginBottom: 4, color: '#89b4fa' }}>Sessions &amp; Users</h2>
            <p style={{ fontSize: 13, color: '#6c7086', marginBottom: 28 }}>
                Session lifecycle and concurrency controls, user accounts, and the user database path.
            </p>
            <div style={{display: "flex", flexDirection: "row", gap: 28}}>
                <SessionSection />
                <div style={{display: "flex", flexDirection: "column", minWidth: 800}}>
                    <UsersSection/>
                </div>
            </div>
        </div>
    );
}
