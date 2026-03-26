// ViewportGrid renders a CSS grid of RemoteViewport + MeasurementOverlay pairs.
// Layout is driven by useViewportStore; each viewport slot maps to a channel.

import { useViewportStore } from '@/stores/viewportStore'
import { RemoteViewport } from './RemoteViewport'
import { MeasurementOverlay } from './MeasurementOverlay'
import type { ViewportLayout } from '@/types/models'

function layoutToColumns(layout: ViewportLayout): number {
  switch (layout) {
    case '1x1': return 1
    case '1x2': return 2
    case '2x2': return 2
    case '2x3': return 3
    case '3x3': return 3
  }
}

export function ViewportGrid() {
  const layout = useViewportStore((s) => s.layout)
  const channels = useViewportStore((s) => s.channels)
  const activeViewportIndex = useViewportStore((s) => s.activeViewportIndex)
  const setActiveViewport = useViewportStore((s) => s.setActiveViewport)
  const frameResolutions = useViewportStore((s) => s.frameResolutions)

  const cols = layoutToColumns(layout)

  return (
    <div
      style={{
        display: 'grid',
        gridTemplateColumns: `repeat(${cols}, 1fr)`,
        gridAutoRows: '1fr',
        width: '100%',
        height: '100%',
        gap: '2px',
        background: '#111',
        boxSizing: 'border-box',
      }}
    >
      {channels.map((ch) => (
        <div
          key={ch.channelId}
          style={{ position: 'relative', minWidth: 0, minHeight: 0 }}
        >
          <RemoteViewport
            channelId={ch.channelId}
            isActive={ch.viewportIndex === activeViewportIndex}
            onClick={() => setActiveViewport(ch.viewportIndex)}
          />
          {frameResolutions[ch.channelId] && (
            <MeasurementOverlay
              channelId={ch.channelId}
              width={frameResolutions[ch.channelId].width}
              height={frameResolutions[ch.channelId].height}
            />
          )}
        </div>
      ))}
    </div>
  )
}
