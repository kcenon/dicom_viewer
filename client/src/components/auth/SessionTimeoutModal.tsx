import { useEffect, useState } from 'react'

const COUNTDOWN_SECONDS = 60 // time between 14-min warning and 15-min forced logout

const styles = {
  overlay: {
    position: 'fixed' as const,
    inset: 0,
    display: 'flex',
    alignItems: 'center',
    justifyContent: 'center',
    background: 'rgba(0,0,0,0.7)',
    backdropFilter: 'blur(4px)',
    zIndex: 9999,
  },
  modal: {
    width: 400,
    padding: '32px 28px',
    background: '#161b22',
    border: '1px solid #f0883e',
    borderRadius: 8,
    boxShadow: '0 8px 32px rgba(0,0,0,0.5)',
    textAlign: 'center' as const,
  },
  icon: {
    fontSize: 40,
    marginBottom: 12,
  },
  title: {
    margin: '0 0 8px',
    fontSize: 18,
    fontWeight: 600,
    color: '#f0883e',
  },
  message: {
    margin: '0 0 8px',
    fontSize: 14,
    color: '#c9d1d9',
    lineHeight: 1.5,
  },
  countdown: {
    margin: '0 0 24px',
    fontSize: 13,
    color: '#7d8590',
  },
  countdownNumber: {
    fontWeight: 700,
    color: '#f85149',
  },
  actions: {
    display: 'flex',
    gap: 12,
    justifyContent: 'center',
  },
  stayButton: {
    padding: '9px 20px',
    background: '#238636',
    color: '#fff',
    border: 'none',
    borderRadius: 6,
    fontSize: 14,
    fontWeight: 600,
    cursor: 'pointer',
  } as const,
  logoutButton: {
    padding: '9px 20px',
    background: 'transparent',
    color: '#7d8590',
    border: '1px solid #30363d',
    borderRadius: 6,
    fontSize: 14,
    cursor: 'pointer',
  } as const,
}

interface Props {
  onStayLoggedIn: () => void
  onLogout: () => void
}

export function SessionTimeoutModal({ onStayLoggedIn, onLogout }: Props) {
  const [secondsLeft, setSecondsLeft] = useState(COUNTDOWN_SECONDS)

  useEffect(() => {
    if (secondsLeft === 0) {
      onLogout()
      return
    }
    const timer = setTimeout(() => setSecondsLeft((s) => s - 1), 1000)
    return () => clearTimeout(timer)
  }, [secondsLeft, onLogout])

  return (
    <div style={styles.overlay} role="dialog" aria-modal="true" aria-labelledby="timeout-title">
      <div style={styles.modal}>
        <div style={styles.icon} aria-hidden="true">⚠️</div>
        <h2 id="timeout-title" style={styles.title}>Session Timeout Warning</h2>
        <p style={styles.message}>
          You have been inactive for 14 minutes.
        </p>
        <p style={styles.countdown}>
          Auto-logout in{' '}
          <span style={styles.countdownNumber}>{secondsLeft}</span>
          {' '}second{secondsLeft !== 1 ? 's' : ''}
        </p>
        <div style={styles.actions}>
          <button style={styles.stayButton} onClick={onStayLoggedIn}>
            Stay Logged In
          </button>
          <button style={styles.logoutButton} onClick={onLogout}>
            Logout Now
          </button>
        </div>
      </div>
    </div>
  )
}
