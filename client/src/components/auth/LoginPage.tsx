import { useState } from 'react'
import type { FormEvent } from 'react'
import { useAuthStore } from '@/stores/authStore'
import { http } from '@/api/httpClient'
import type { LoginResponse } from '@/types/api'

const styles = {
  root: {
    display: 'flex',
    alignItems: 'center',
    justifyContent: 'center',
    minHeight: '100vh',
    background: '#0d1117',
  } as const,
  card: {
    width: 360,
    padding: '40px 32px',
    background: '#161b22',
    border: '1px solid #30363d',
    borderRadius: 8,
    boxShadow: '0 8px 32px rgba(0,0,0,0.4)',
  } as const,
  title: {
    margin: '0 0 4px',
    fontSize: 22,
    fontWeight: 600,
    color: '#e6edf3',
    textAlign: 'center' as const,
  },
  subtitle: {
    margin: '0 0 28px',
    fontSize: 13,
    color: '#7d8590',
    textAlign: 'center' as const,
  },
  label: {
    display: 'block',
    marginBottom: 6,
    fontSize: 13,
    fontWeight: 500,
    color: '#c9d1d9',
  } as const,
  input: {
    display: 'block',
    width: '100%',
    padding: '8px 12px',
    marginBottom: 16,
    background: '#0d1117',
    border: '1px solid #30363d',
    borderRadius: 6,
    color: '#e6edf3',
    fontSize: 14,
    boxSizing: 'border-box' as const,
    outline: 'none',
  },
  button: {
    display: 'block',
    width: '100%',
    padding: '10px 0',
    marginTop: 8,
    background: '#238636',
    color: '#fff',
    border: 'none',
    borderRadius: 6,
    fontSize: 14,
    fontWeight: 600,
    cursor: 'pointer',
  } as const,
  buttonDisabled: {
    opacity: 0.6,
    cursor: 'not-allowed',
  } as const,
  error: {
    marginBottom: 12,
    padding: '8px 12px',
    background: 'rgba(248,81,73,0.15)',
    border: '1px solid rgba(248,81,73,0.4)',
    borderRadius: 6,
    color: '#f85149',
    fontSize: 13,
  } as const,
}

export function LoginPage() {
  const login = useAuthStore((s) => s.login)
  const [username, setUsername] = useState('')
  const [password, setPassword] = useState('')
  const [error, setError] = useState<string | null>(null)
  const [loading, setLoading] = useState(false)

  const handleSubmit = async (e: FormEvent) => {
    e.preventDefault()
    setError(null)
    setLoading(true)
    try {
      const res = await http.post<LoginResponse>('/auth/login', { username, password })
      login(res.token, res.refreshToken, res.user)
    } catch (err) {
      setError(err instanceof Error ? err.message : 'Login failed')
    } finally {
      setLoading(false)
    }
  }

  return (
    <div style={styles.root}>
      <div style={styles.card}>
        <h1 style={styles.title}>DICOM Viewer</h1>
        <p style={styles.subtitle}>Sign in to access the viewer</p>
        <form onSubmit={handleSubmit} noValidate>
          {error && <div style={styles.error}>{error}</div>}
          <label style={styles.label} htmlFor="username">Username</label>
          <input
            id="username"
            type="text"
            autoComplete="username"
            required
            style={styles.input}
            value={username}
            onChange={(e) => setUsername(e.target.value)}
          />
          <label style={styles.label} htmlFor="password">Password</label>
          <input
            id="password"
            type="password"
            autoComplete="current-password"
            required
            style={styles.input}
            value={password}
            onChange={(e) => setPassword(e.target.value)}
          />
          <button
            type="submit"
            disabled={loading}
            style={{ ...styles.button, ...(loading ? styles.buttonDisabled : {}) }}
          >
            {loading ? 'Signing in…' : 'Sign In'}
          </button>
        </form>
      </div>
    </div>
  )
}
