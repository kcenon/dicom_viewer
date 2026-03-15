// FlowToolPanel controls 4D Flow MRI phase playback and displays quantification data.

import { useFlowStore } from '@/stores/flowStore'

export function FlowToolPanel() {
  const phaseIndex = useFlowStore((s) => s.phaseIndex)
  const totalPhases = useFlowStore((s) => s.totalPhases)
  const isPlaying = useFlowStore((s) => s.isPlaying)
  const playbackFps = useFlowStore((s) => s.playbackFps)
  const quantificationData = useFlowStore((s) => s.quantificationData)
  const setPhaseIndex = useFlowStore((s) => s.setPhaseIndex)
  const setPlaying = useFlowStore((s) => s.setPlaying)
  const setPlaybackFps = useFlowStore((s) => s.setPlaybackFps)
  const nextPhase = useFlowStore((s) => s.nextPhase)
  const prevPhase = useFlowStore((s) => s.prevPhase)

  if (totalPhases === 0) {
    return (
      <div style={{ color: '#555', fontSize: '12px', textAlign: 'center', marginTop: '8px' }}>
        No 4D Flow study loaded
      </div>
    )
  }

  return (
    <div style={{ display: 'flex', flexDirection: 'column', gap: '10px' }}>
      {/* Phase scrubber */}
      <div>
        <div style={sectionLabel}>
          Phase {phaseIndex + 1} / {totalPhases}
        </div>
        <input
          type="range"
          min={0}
          max={totalPhases - 1}
          value={phaseIndex}
          onChange={(e) => setPhaseIndex(Number(e.target.value))}
          style={{ width: '100%', marginTop: '4px' }}
        />
      </div>

      {/* Playback controls */}
      <div style={{ display: 'flex', gap: '4px', justifyContent: 'center' }}>
        <button onClick={prevPhase} style={controlButtonStyle}>◀</button>
        <button
          onClick={() => setPlaying(!isPlaying)}
          style={{ ...controlButtonStyle, minWidth: '64px', background: isPlaying ? '#1565c0' : '#2a2a2a' }}
        >
          {isPlaying ? '⏸ Pause' : '▶ Play'}
        </button>
        <button onClick={nextPhase} style={controlButtonStyle}>▶</button>
      </div>

      {/* FPS control */}
      <div>
        <div style={sectionLabel}>Playback speed: {playbackFps} FPS</div>
        <input
          type="range"
          min={1}
          max={30}
          value={playbackFps}
          onChange={(e) => setPlaybackFps(Number(e.target.value))}
          style={{ width: '100%', marginTop: '4px' }}
        />
      </div>

      {/* Quantification data */}
      {quantificationData !== null && (
        <div style={{ display: 'flex', flexDirection: 'column', gap: '4px' }}>
          <div style={sectionLabel}>Flow Metrics</div>
          <table style={{ width: '100%', borderCollapse: 'collapse', fontSize: '12px' }}>
            <tbody>
              {([
                ['Forward', `${quantificationData.forwardFlowMl.toFixed(1)} mL`],
                ['Reverse', `${quantificationData.reverseFlowMl.toFixed(1)} mL`],
                ['Net flow', `${quantificationData.netFlowMl.toFixed(1)} mL`],
                ['Peak vel.', `${(quantificationData.peakVelocityMs * 100).toFixed(1)} cm/s`],
              ] as [string, string][]).map(([key, val]) => (
                <tr key={key}>
                  <td style={{ color: '#666', paddingRight: '8px', paddingBottom: '2px' }}>{key}</td>
                  <td style={{ color: '#ccc' }}>{val}</td>
                </tr>
              ))}
            </tbody>
          </table>
        </div>
      )}
    </div>
  )
}

const sectionLabel: React.CSSProperties = {
  fontSize: '11px',
  color: '#666',
  textTransform: 'uppercase',
  letterSpacing: '0.05em',
}

const controlButtonStyle: React.CSSProperties = {
  background: '#2a2a2a',
  border: '1px solid #444',
  borderRadius: '3px',
  color: '#ccc',
  cursor: 'pointer',
  fontSize: '13px',
  padding: '4px 10px',
}
