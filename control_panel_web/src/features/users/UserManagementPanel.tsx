// FILE: control_panel_web/src/features/users/UserManagementPanel.tsx
import { useEffect, useState, useCallback } from 'react';
import { configClient, type UserInfo, type UserRole } from 'api/configClient';

interface AddUserForm  { username: string; password: string; role: UserRole; }
interface ResetForm    { username: string; new_password: string; }

const ROLE_LABELS: Record<UserRole, string> = { admin: 'Admin', engineer: 'Engineer' };

function formatTs(ts: number) {
  if (!ts) return '—';
  return new Date(ts * 1000).toLocaleString();
}

export default function UserManagementPanel() {
  const [users,   setUsers]   = useState<UserInfo[]>([]);
  const [error,   setError]   = useState('');
  const [loading, setLoading] = useState(true);
  const [showAdd,   setShowAdd]   = useState(false);
  const [showReset, setShowReset] = useState<ResetForm | null>(null);
  const [showDel,   setShowDel]   = useState<UserInfo | null>(null);
  const [addForm,  setAddForm]  = useState<AddUserForm>({ username: '', password: '', role: 'engineer' });
  const [formErr,  setFormErr]  = useState('');

  const loadUsers = useCallback(async () => {
    setLoading(true); setError('');
    const r = await configClient.listUsers();
    if (r.error) setError(r.error);
    else         setUsers(r.data?.users ?? []);
    setLoading(false);
  }, []);

  useEffect(() => { loadUsers(); }, [loadUsers]);

  const createUser = async () => {
    if (!addForm.username || !addForm.password) { setFormErr('All fields required.'); return; }
    const r = await configClient.createUser(addForm);
    if (r.error) { setFormErr(r.error); return; }
    setShowAdd(false);
    setAddForm({ username: '', password: '', role: 'engineer' });
    setFormErr('');
    loadUsers();
  };

  const resetPassword = async () => {
    if (!showReset?.new_password) {
      setFormErr('Password required.');
      return;
    }
    const r = await configClient.resetPassword(showReset.username.toString(), showReset.new_password);
    if (r.error) { setFormErr(r.error); return; }
    setShowReset(null); setFormErr('');
  };

  const deleteUser = async () => {
    if (!showDel) return;
    const r = await configClient.deleteUser(showDel.user_id.toString());
    if (r.error) { setError(r.error); return; }
    setShowDel(null); loadUsers();
  };

  if (loading) return <p style={{ color: '#cdd6f4' }}>Loading…</p>;
  if (error)   return <p style={{ color: '#f38ba8' }}>Error: {error}</p>;

  return (
      <div>
        <h2 style={{ color: '#cdd6f4', marginBottom: 16 }}>User Management</h2>
        <button onClick={() => { setShowAdd(true); setFormErr(''); }}
                style={{ marginBottom: 14, padding: '6px 14px', cursor: 'pointer',
                  background: '#313244', color: '#cdd6f4', border: '1px solid #45475a', borderRadius: 4 }}>
          + Add User
        </button>

        {users.length === 0
            ? <p style={{ color: '#a6adc8', fontSize: 14 }}>No users yet.</p>
            : (
                <table style={{ width: '100%', borderCollapse: 'collapse', fontSize: 14 }}>
                  <thead>
                  <tr style={{ background: '#313244' }}>
                    {['Username', 'Role', 'Created', 'Last Login', 'Actions'].map(h => (
                        <th key={h} style={{ textAlign: 'left', padding: '8px 10px',
                          borderBottom: '1px solid #45475a', color: '#a6adc8' }}>{h}</th>
                    ))}
                  </tr>
                  </thead>
                  <tbody>
                  {users.map(u => (
                      <tr key={u.user_id} style={{ borderBottom: '1px solid #313244' }}>
                        <td style={{ padding: '8px 10px', color: '#cdd6f4' }}>{u.username}</td>
                        <td style={{ padding: '8px 10px' }}>
                                        <span style={{
                                          background: u.role === 'admin' ? '#1e3a5f' : '#1a3a2a',
                                          color:      u.role === 'admin' ? '#89b4fa' : '#a6e3a1',
                                          padding: '2px 8px', borderRadius: 12, fontSize: 12
                                        }}>
                                            {ROLE_LABELS[u.role] ?? u.role}
                                        </span>
                        </td>
                        <td style={{ padding: '8px 10px', color: '#6c7086', fontSize: 12 }}>{formatTs(u.created_at)}</td>
                        <td style={{ padding: '8px 10px', color: '#6c7086', fontSize: 12 }}>{formatTs(u.last_login)}</td>
                        <td style={{ padding: '8px 10px' }}>
                          <button onClick={() => { setShowReset({ username: u.username, new_password: '' }); setFormErr(''); }}
                                  style={{ marginRight: 8, fontSize: 12, cursor: 'pointer', padding: '3px 10px',
                                    background: '#313244', color: '#cdd6f4', border: '1px solid #45475a', borderRadius: 4 }}>
                            Reset Password
                          </button>
                          <button onClick={() => setShowDel(u)}
                                  style={{ fontSize: 12, cursor: 'pointer', padding: '3px 10px',
                                    background: '#3b1a1a', color: '#f38ba8', border: '1px solid #7f3030', borderRadius: 4 }}>
                            Delete
                          </button>
                        </td>
                      </tr>
                  ))}
                  </tbody>
                </table>
            )
        }

        {showAdd && (
            <Modal title="Add User" onClose={() => setShowAdd(false)}>
              <Field label="Username">
                <input value={addForm.username}
                       onChange={e => setAddForm({ ...addForm, username: e.target.value })}
                       style={inputStyle} />
              </Field>
              <Field label="Password">
                <input type="password" value={addForm.password}
                       onChange={e => setAddForm({ ...addForm, password: e.target.value })}
                       style={inputStyle} />
              </Field>
              <Field label="Role">
                <select value={addForm.role}
                        onChange={e => setAddForm({ ...addForm, role: e.target.value as UserRole })}
                        style={inputStyle}>
                  <option value="engineer">Engineer</option>
                  <option value="admin">Admin</option>
                </select>
              </Field>
              {formErr && <p style={errStyle}>{formErr}</p>}
              <ModalActions>
                <PrimaryBtn onClick={createUser}>Create</PrimaryBtn>
                <SecondaryBtn onClick={() => setShowAdd(false)}>Cancel</SecondaryBtn>
              </ModalActions>
            </Modal>
        )}

        {showReset && (
            <Modal title={`Reset Password — ${showReset.username}`} onClose={() => setShowReset(null)}>
              <Field label="New Password">
                <input type="password" value={showReset.new_password}
                       onChange={e => setShowReset({ ...showReset, new_password: e.target.value })}
                       style={inputStyle} />
              </Field>
              {formErr && <p style={errStyle}>{formErr}</p>}
              <ModalActions>
                <PrimaryBtn onClick={resetPassword}>Reset</PrimaryBtn>
                <SecondaryBtn onClick={() => setShowReset(null)}>Cancel</SecondaryBtn>
              </ModalActions>
            </Modal>
        )}

        {showDel && (
            <Modal title="Delete User" onClose={() => setShowDel(null)}>
              <p style={{ color: '#cdd6f4', marginBottom: 16 }}>
                Delete <strong>{showDel.username}</strong>? This cannot be undone.
              </p>
              <ModalActions>
                <DangerBtn onClick={deleteUser}>Delete</DangerBtn>
                <SecondaryBtn onClick={() => setShowDel(null)}>Cancel</SecondaryBtn>
              </ModalActions>
            </Modal>
        )}
      </div>
  );
}

function Modal({ title, children, onClose }: { title: string; children: React.ReactNode; onClose: () => void }) {
  return (
      <div style={{ position: 'fixed', inset: 0, background: 'rgba(0,0,0,0.6)',
        display: 'flex', alignItems: 'center', justifyContent: 'center', zIndex: 100 }}>
        <div style={{ background: '#1e1e2e', border: '1px solid #45475a',
          padding: 24, borderRadius: 8, width: 400 }}>
          <div style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'center', marginBottom: 16 }}>
            <h3 style={{ margin: 0, color: '#cdd6f4' }}>{title}</h3>
            <button onClick={onClose}
                    style={{ background: 'none', border: 'none', color: '#6c7086',
                      cursor: 'pointer', fontSize: 18, lineHeight: 1 }}>✕</button>
          </div>
          {children}
        </div>
      </div>
  );
}

function Field({ label, children }: { label: string; children: React.ReactNode }) {
  return (
      <label style={{ display: 'block', marginBottom: 12, color: '#a6adc8', fontSize: 14 }}>
        {label}<div style={{ marginTop: 4 }}>{children}</div>
      </label>
  );
}
function ModalActions({ children }: { children: React.ReactNode }) {
  return <div style={{ display: 'flex', gap: 8, marginTop: 16 }}>{children}</div>;
}
function PrimaryBtn({ onClick, children }: { onClick: () => void; children: React.ReactNode }) {
  return <button onClick={onClick}
                 style={{ padding: '5px 14px', background: '#1d4ed8', color: '#fff',
                   border: 'none', borderRadius: 4, cursor: 'pointer', fontSize: 13 }}>{children}</button>;
}
function DangerBtn({ onClick, children }: { onClick: () => void; children: React.ReactNode }) {
  return <button onClick={onClick}
                 style={{ padding: '5px 14px', background: '#7f1d1d', color: '#fca5a5',
                   border: '1px solid #7f3030', borderRadius: 4, cursor: 'pointer', fontSize: 13 }}>{children}</button>;
}
function SecondaryBtn({ onClick, children }: { onClick: () => void; children: React.ReactNode }) {
  return <button onClick={onClick}
                 style={{ padding: '5px 12px', background: '#313244', color: '#cdd6f4',
                   border: '1px solid #45475a', borderRadius: 4, cursor: 'pointer', fontSize: 13 }}>{children}</button>;
}

const inputStyle: React.CSSProperties = {
  width: '100%', padding: '5px 8px', background: '#313244',
  color: '#cdd6f4', border: '1px solid #45475a', borderRadius: 4, fontSize: 14,
};
const errStyle: React.CSSProperties = { color: '#f38ba8', fontSize: 13, margin: '4px 0 0' };