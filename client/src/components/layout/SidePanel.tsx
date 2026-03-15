// SidePanel is a collapsible dock housing tabbed tool panels.
// Actual panel content (PatientBrowser, SegmentationPanel, etc.) will be
// added in issue #521. Placeholder tabs are rendered here.

import { useUiStore } from '@/stores/uiStore'
import type { ReactNode } from 'react'

type ActivePanel = NonNullable<ReturnType<typeof useUiStore.getState>['activePanel']>

const PANEL_TABS: { id: ActivePanel; label: string }[] = [
  { id: 'patientBrowser', label: 'Patients' },
  { id: 'segmentation', label: 'Seg' },
  { id: 'measurement', label: 'Measure' },
  { id: 'flow', label: 'Flow' },
  { id: 'overlay', label: 'Overlay' },
  { id: 'report', label: 'Report' },
]

interface Props {
  children?: ReactNode
}

export function SidePanel({ children }: Props) {
  const isSidePanelOpen = useUiStore((s) => s.isSidePanelOpen)
  const activePanel = useUiStore((s) => s.activePanel)
  const setActivePanel = useUiStore((s) => s.setActivePanel)

  if (!isSidePanelOpen) return null

  return (
    <div
      style={{
        display: 'flex',
        flexDirection: 'column',
        width: '240px',
        flexShrink: 0,
        background: '#1e1e1e',
        borderRight: '1px solid #333',
        overflow: 'hidden',
      }}
    >
      {/* Tab bar */}
      <div
        style={{
          display: 'flex',
          flexWrap: 'wrap',
          gap: '1px',
          padding: '4px',
          background: '#1a1a1a',
          borderBottom: '1px solid #333',
          flexShrink: 0,
        }}
      >
        {PANEL_TABS.map(({ id, label }) => (
          <button
            key={id}
            onClick={() => setActivePanel(id)}
            style={{
              background: activePanel === id ? '#1565c0' : '#2a2a2a',
              border: '1px solid #444',
              borderRadius: '3px',
              color: activePanel === id ? '#fff' : '#aaa',
              cursor: 'pointer',
              fontSize: '11px',
              padding: '3px 6px',
              lineHeight: 1,
            }}
          >
            {label}
          </button>
        ))}
      </div>

      {/* Panel content area */}
      <div
        style={{
          flex: 1,
          overflow: 'auto',
          padding: '8px',
          color: '#888',
          fontSize: '13px',
        }}
      >
        {children ?? (
          <div style={{ textAlign: 'center', marginTop: '24px', color: '#555' }}>
            {activePanel !== null ? activePanel : 'Select a panel'}
          </div>
        )}
      </div>
    </div>
  )
}
