// AppLayout is the main application shell: TopBar + (SidePanel | ViewportGrid) + StatusBar.
// Modal dialogs are rendered here so they overlay the entire UI.

import { useUiStore } from '@/stores/uiStore'
import { TopBar } from './TopBar'
import { SidePanel } from './SidePanel'
import { StatusBar } from './StatusBar'
import { ViewportGrid } from '@/components/viewport/ViewportGrid'
import { PacsConfigDialog } from '@/components/dialogs/PacsConfigDialog'
import { SettingsDialog } from '@/components/dialogs/SettingsDialog'

export function AppLayout() {
  const isStatusBarVisible = useUiStore((s) => s.isStatusBarVisible)

  return (
    <div
      style={{
        display: 'flex',
        flexDirection: 'column',
        width: '100vw',
        height: '100vh',
        background: '#111',
        overflow: 'hidden',
      }}
    >
      <TopBar />

      {/* Main content: sidebar + viewport area */}
      <div style={{ display: 'flex', flex: 1, overflow: 'hidden', minHeight: 0 }}>
        <SidePanel />
        <div style={{ flex: 1, overflow: 'hidden', minWidth: 0 }}>
          <ViewportGrid />
        </div>
      </div>

      {isStatusBarVisible && <StatusBar />}

      {/* Modal dialogs */}
      <PacsConfigDialog />
      <SettingsDialog />
    </div>
  )
}
