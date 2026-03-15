// TopBar renders the application header: title, viewport layout selector,
// sidebar toggle, and user info with logout.

import { useUiStore } from '@/stores/uiStore'
import { useViewportStore } from '@/stores/viewportStore'
import { useAuthStore } from '@/stores/authStore'
import type { ViewportLayout } from '@/types/models'

const LAYOUTS: ViewportLayout[] = ['1x1', '1x2', '2x2', '2x3', '3x3']

export function TopBar() {
  const layout = useViewportStore((s) => s.layout)
  const setLayout = useViewportStore((s) => s.setLayout)
  const isSidePanelOpen = useUiStore((s) => s.isSidePanelOpen)
  const toggleSidePanel = useUiStore((s) => s.toggleSidePanel)
  const openPacsConfig = useUiStore((s) => s.openPacsConfig)
  const openSettings = useUiStore((s) => s.openSettings)
  const user = useAuthStore((s) => s.user)
  const logout = useAuthStore((s) => s.logout)

  return (
    <div
      style={{
        display: 'flex',
        alignItems: 'center',
        gap: '8px',
        height: '40px',
        padding: '0 12px',
        background: '#212121',
        borderBottom: '1px solid #333',
        flexShrink: 0,
      }}
    >
      {/* Sidebar toggle */}
      <button
        onClick={toggleSidePanel}
        title={isSidePanelOpen ? 'Collapse sidebar' : 'Expand sidebar'}
        style={buttonStyle}
        aria-pressed={isSidePanelOpen}
      >
        {isSidePanelOpen ? '◀' : '▶'}
      </button>

      {/* App title */}
      <span
        style={{ fontWeight: 600, fontSize: '14px', color: '#e0e0e0', marginRight: '8px' }}
      >
        DICOM Viewer
      </span>

      {/* Viewport layout selector */}
      <span style={{ fontSize: '12px', color: '#888', marginRight: '4px' }}>Layout:</span>
      {LAYOUTS.map((l) => (
        <button
          key={l}
          onClick={() => setLayout(l)}
          style={{
            ...buttonStyle,
            background: layout === l ? '#1565c0' : '#2a2a2a',
            color: layout === l ? '#fff' : '#aaa',
            minWidth: '36px',
          }}
        >
          {l}
        </button>
      ))}

      {/* Spacer */}
      <div style={{ flex: 1 }} />

      {/* Config / Settings */}
      <button onClick={openPacsConfig} style={buttonStyle} title="PACS Configuration">
        PACS
      </button>
      <button onClick={openSettings} style={buttonStyle} title="Settings">
        ⚙
      </button>

      {/* User info */}
      {user !== null && (
        <span style={{ fontSize: '12px', color: '#aaa' }}>
          {user.username}
          <span style={{ color: '#666', marginLeft: '4px' }}>({user.role})</span>
        </span>
      )}

      <button onClick={logout} style={{ ...buttonStyle, color: '#ef9a9a' }}>
        Logout
      </button>
    </div>
  )
}

const buttonStyle: React.CSSProperties = {
  background: '#2a2a2a',
  border: '1px solid #444',
  borderRadius: '3px',
  color: '#ccc',
  cursor: 'pointer',
  fontSize: '12px',
  padding: '2px 8px',
  height: '24px',
  lineHeight: 1,
}
