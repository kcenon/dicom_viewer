// SettingsDialog allows toggling theme, status bar, and toolbar visibility.

import { useUiStore } from '@/stores/uiStore'

export function SettingsDialog() {
  const isSettingsOpen = useUiStore((s) => s.isSettingsOpen)
  const closeSettings = useUiStore((s) => s.closeSettings)
  const theme = useUiStore((s) => s.theme)
  const setTheme = useUiStore((s) => s.setTheme)
  const isStatusBarVisible = useUiStore((s) => s.isStatusBarVisible)
  const setStatusBarVisible = useUiStore((s) => s.setStatusBarVisible)
  const isToolBarVisible = useUiStore((s) => s.isToolBarVisible)
  const setToolBarVisible = useUiStore((s) => s.setToolBarVisible)

  if (!isSettingsOpen) return null

  return (
    <div style={overlayStyle} onClick={closeSettings}>
      <div style={dialogStyle} onClick={(e) => e.stopPropagation()}>
        <div style={titleStyle}>Settings</div>

        <div style={{ display: 'flex', flexDirection: 'column', gap: '12px' }}>
          {/* Theme */}
          <div style={rowStyle}>
            <span style={rowLabelStyle}>Theme</span>
            <div style={{ display: 'flex', gap: '4px' }}>
              <button
                onClick={() => setTheme('dark')}
                style={{
                  ...chipStyle,
                  background: theme === 'dark' ? '#1565c0' : '#2a2a2a',
                  color: theme === 'dark' ? '#fff' : '#aaa',
                }}
              >
                Dark
              </button>
              <button
                onClick={() => setTheme('light')}
                style={{
                  ...chipStyle,
                  background: theme === 'light' ? '#1565c0' : '#2a2a2a',
                  color: theme === 'light' ? '#fff' : '#aaa',
                }}
              >
                Light
              </button>
            </div>
          </div>

          {/* Status bar */}
          <div style={rowStyle}>
            <label htmlFor="settings-statusbar" style={rowLabelStyle}>
              Status bar
            </label>
            <input
              id="settings-statusbar"
              type="checkbox"
              checked={isStatusBarVisible}
              onChange={(e) => setStatusBarVisible(e.target.checked)}
            />
          </div>

          {/* Toolbar */}
          <div style={rowStyle}>
            <label htmlFor="settings-toolbar" style={rowLabelStyle}>
              Toolbar
            </label>
            <input
              id="settings-toolbar"
              type="checkbox"
              checked={isToolBarVisible}
              onChange={(e) => setToolBarVisible(e.target.checked)}
            />
          </div>
        </div>

        <div style={{ display: 'flex', justifyContent: 'flex-end', marginTop: '20px' }}>
          <button onClick={closeSettings} style={closeButtonStyle}>
            Close
          </button>
        </div>
      </div>
    </div>
  )
}

const overlayStyle: React.CSSProperties = {
  position: 'fixed',
  inset: 0,
  background: 'rgba(0,0,0,0.6)',
  display: 'flex',
  alignItems: 'center',
  justifyContent: 'center',
  zIndex: 1000,
}

const dialogStyle: React.CSSProperties = {
  background: '#1e1e1e',
  border: '1px solid #333',
  borderRadius: '6px',
  padding: '20px',
  width: '360px',
  maxWidth: '90vw',
}

const titleStyle: React.CSSProperties = {
  fontSize: '16px',
  fontWeight: 600,
  color: '#e0e0e0',
  marginBottom: '16px',
}

const rowStyle: React.CSSProperties = {
  display: 'flex',
  justifyContent: 'space-between',
  alignItems: 'center',
}

const rowLabelStyle: React.CSSProperties = {
  fontSize: '13px',
  color: '#ccc',
}

const chipStyle: React.CSSProperties = {
  border: '1px solid #444',
  borderRadius: '3px',
  cursor: 'pointer',
  fontSize: '12px',
  padding: '4px 12px',
}

const closeButtonStyle: React.CSSProperties = {
  background: '#2a2a2a',
  border: '1px solid #444',
  borderRadius: '3px',
  color: '#ccc',
  cursor: 'pointer',
  fontSize: '13px',
  padding: '6px 16px',
}
