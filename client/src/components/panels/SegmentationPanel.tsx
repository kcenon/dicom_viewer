// SegmentationPanel provides brush/eraser tool controls and segmentation label management.

import { useSegmentationStore } from '@/stores/segmentationStore'
import type { SegmentationTool } from '@/types/models'

const TOOLS: { id: SegmentationTool; label: string }[] = [
  { id: 'brush', label: 'Brush' },
  { id: 'eraser', label: 'Eraser' },
  { id: 'levelTracing', label: 'Trace' },
  { id: 'scissors', label: 'Scissors' },
  { id: 'hollow', label: 'Hollow' },
]

const LABEL_COLORS: [number, number, number, number][] = [
  [255, 82, 82, 255],
  [82, 255, 82, 255],
  [82, 82, 255, 255],
  [255, 255, 82, 255],
  [255, 82, 255, 255],
]

function colorToCss(rgba: [number, number, number, number]): string {
  return `rgba(${rgba[0]},${rgba[1]},${rgba[2]},${rgba[3] / 255})`
}

export function SegmentationPanel() {
  const activeTool = useSegmentationStore((s) => s.activeTool)
  const setActiveTool = useSegmentationStore((s) => s.setActiveTool)
  const brushSize = useSegmentationStore((s) => s.brushSize)
  const setBrushSize = useSegmentationStore((s) => s.setBrushSize)
  const undoDepth = useSegmentationStore((s) => s.undoDepth)
  const undo = useSegmentationStore((s) => s.undo)
  const labels = useSegmentationStore((s) => s.labels)
  const activeLabelId = useSegmentationStore((s) => s.activeLabelId)
  const setActiveLabelId = useSegmentationStore((s) => s.setActiveLabelId)
  const addLabel = useSegmentationStore((s) => s.addLabel)
  const removeLabel = useSegmentationStore((s) => s.removeLabel)
  const updateLabel = useSegmentationStore((s) => s.updateLabel)

  const handleAddLabel = () => {
    const colorEntry = LABEL_COLORS[labels.length % LABEL_COLORS.length]
    if (colorEntry === undefined) return
    addLabel({
      id: Date.now(),
      name: `Label ${labels.length + 1}`,
      color: colorEntry,
      visible: true,
      opacity: 0.5,
    })
  }

  return (
    <div style={{ display: 'flex', flexDirection: 'column', gap: '10px' }}>
      {/* Tool selector */}
      <div>
        <div style={sectionLabel}>Tool</div>
        <div style={{ display: 'flex', flexWrap: 'wrap', gap: '4px', marginTop: '4px' }}>
          {TOOLS.map(({ id, label }) => (
            <button
              key={id}
              onClick={() => setActiveTool(id)}
              style={{
                ...chipStyle,
                background: activeTool === id ? '#1565c0' : '#2a2a2a',
                color: activeTool === id ? '#fff' : '#aaa',
              }}
            >
              {label}
            </button>
          ))}
          <button
            onClick={() => setActiveTool('none')}
            style={{
              ...chipStyle,
              background: activeTool === 'none' ? '#424242' : '#2a2a2a',
              color: '#aaa',
            }}
          >
            None
          </button>
        </div>
      </div>

      {/* Brush size */}
      <div>
        <div style={sectionLabel}>Brush size: {brushSize}px</div>
        <input
          type="range"
          min={1}
          max={100}
          value={brushSize}
          onChange={(e) => setBrushSize(Number(e.target.value))}
          style={{ width: '100%', marginTop: '4px' }}
        />
      </div>

      {/* Undo */}
      <button
        onClick={undo}
        disabled={undoDepth === 0}
        style={{ ...actionButtonStyle, opacity: undoDepth === 0 ? 0.4 : 1 }}
      >
        Undo ({undoDepth})
      </button>

      {/* Labels */}
      <div>
        <div
          style={{
            display: 'flex',
            justifyContent: 'space-between',
            alignItems: 'center',
            marginBottom: '4px',
          }}
        >
          <span style={sectionLabel}>Labels</span>
          <button onClick={handleAddLabel} style={chipStyle}>
            + Add
          </button>
        </div>
        {labels.map((label) => (
          <div
            key={label.id}
            style={{
              display: 'flex',
              alignItems: 'center',
              gap: '6px',
              padding: '4px 0',
              borderBottom: '1px solid #2a2a2a',
            }}
          >
            {/* Color swatch / active indicator */}
            <div
              onClick={() => setActiveLabelId(label.id)}
              style={{
                width: '16px',
                height: '16px',
                borderRadius: '3px',
                background: colorToCss(label.color),
                border: activeLabelId === label.id ? '2px solid #fff' : '2px solid transparent',
                cursor: 'pointer',
                flexShrink: 0,
              }}
            />
            {/* Visibility toggle */}
            <input
              type="checkbox"
              checked={label.visible}
              onChange={(e) => updateLabel(label.id, { visible: e.target.checked })}
              style={{ flexShrink: 0 }}
            />
            {/* Label name */}
            <span style={{ flex: 1, fontSize: '12px', color: '#ccc', overflow: 'hidden', textOverflow: 'ellipsis', whiteSpace: 'nowrap' }}>
              {label.name}
            </span>
            {/* Remove */}
            <button
              onClick={() => removeLabel(label.id)}
              style={{ ...chipStyle, padding: '1px 5px', color: '#ef9a9a' }}
            >
              ✕
            </button>
          </div>
        ))}
        {labels.length === 0 && (
          <div style={{ color: '#555', fontSize: '12px', textAlign: 'center', marginTop: '4px' }}>
            No labels
          </div>
        )}
      </div>
    </div>
  )
}

const sectionLabel: React.CSSProperties = {
  fontSize: '11px',
  color: '#666',
  textTransform: 'uppercase',
  letterSpacing: '0.05em',
}

const chipStyle: React.CSSProperties = {
  background: '#2a2a2a',
  border: '1px solid #444',
  borderRadius: '3px',
  color: '#aaa',
  cursor: 'pointer',
  fontSize: '12px',
  padding: '3px 8px',
}

const actionButtonStyle: React.CSSProperties = {
  background: '#2a2a2a',
  border: '1px solid #444',
  borderRadius: '3px',
  color: '#ccc',
  cursor: 'pointer',
  fontSize: '12px',
  padding: '4px 8px',
  width: '100%',
}
