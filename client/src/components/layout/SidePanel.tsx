// SidePanel is a collapsible dock housing tabbed tool panels.

import { useUiStore } from '@/stores/uiStore'
import { PatientBrowser } from '@/components/panels/PatientBrowser'
import { SegmentationPanel } from '@/components/panels/SegmentationPanel'
import { FlowToolPanel } from '@/components/panels/FlowToolPanel'
import { OverlayControlPanel } from '@/components/panels/OverlayControlPanel'
import { ReportPanel } from '@/components/panels/ReportPanel'

type ActivePanel = NonNullable<ReturnType<typeof useUiStore.getState>['activePanel']>

const PANEL_TABS: { id: ActivePanel; label: string }[] = [
  { id: 'patientBrowser', label: 'Patients' },
  { id: 'segmentation', label: 'Seg' },
  { id: 'measurement', label: 'Measure' },
  { id: 'flow', label: 'Flow' },
  { id: 'overlay', label: 'Overlay' },
  { id: 'report', label: 'Report' },
]

function ActivePanelContent({ activePanel }: { activePanel: ActivePanel }) {
  switch (activePanel) {
    case 'patientBrowser': return <PatientBrowser />
    case 'segmentation': return <SegmentationPanel />
    case 'flow': return <FlowToolPanel />
    case 'overlay': return <OverlayControlPanel />
    case 'report': return <ReportPanel />
    case 'measurement':
      return (
        <div style={{ color: '#555', fontSize: '12px', textAlign: 'center', marginTop: '8px' }}>
          Use the viewport tools to add measurements
        </div>
      )
  }
}

export function SidePanel() {
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
        {activePanel !== null ? (
          <ActivePanelContent activePanel={activePanel} />
        ) : (
          <div style={{ textAlign: 'center', marginTop: '24px', color: '#555' }}>
            Select a panel
          </div>
        )}
      </div>
    </div>
  )
}
