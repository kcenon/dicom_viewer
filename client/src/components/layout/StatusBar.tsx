// StatusBar displays real-time connection status and quality metrics from sessionStore.

import { useSessionStore } from '@/stores/sessionStore'

const STATUS_COLOR: Record<string, string> = {
  connected: '#4caf50',
  connecting: '#ff9800',
  disconnected: '#9e9e9e',
  error: '#f44336',
}

export function StatusBar() {
  const connectionStatus = useSessionStore((s) => s.connectionStatus)
  const metrics = useSessionStore((s) => s.metrics)

  const color = STATUS_COLOR[connectionStatus] ?? '#9e9e9e'

  return (
    <div
      style={{
        display: 'flex',
        alignItems: 'center',
        gap: '16px',
        height: '24px',
        padding: '0 12px',
        background: '#1a1a1a',
        borderTop: '1px solid #333',
        fontSize: '12px',
        color: '#aaa',
        userSelect: 'none',
        flexShrink: 0,
      }}
    >
      <span style={{ display: 'flex', alignItems: 'center', gap: '4px' }}>
        <span
          style={{
            display: 'inline-block',
            width: '8px',
            height: '8px',
            borderRadius: '50%',
            background: color,
          }}
        />
        <span style={{ color }}>{connectionStatus}</span>
      </span>
      <span>{metrics.fps.toFixed(0)} FPS</span>
      <span>{metrics.latencyMs.toFixed(0)} ms</span>
      <span>{(metrics.bandwidthKbps / 1024).toFixed(1)} Mbps</span>
      {metrics.droppedFrames > 0 && (
        <span style={{ color: '#f44336' }}>{metrics.droppedFrames} dropped</span>
      )}
    </div>
  )
}
