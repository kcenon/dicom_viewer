// OverlayControlPanel controls segmentation label overlay visibility and opacity.

import { useSegmentationStore } from '@/stores/segmentationStore'

function colorToCss(rgba: [number, number, number, number]): string {
  return `rgba(${rgba[0]},${rgba[1]},${rgba[2]},${rgba[3] / 255})`
}

export function OverlayControlPanel() {
  const labels = useSegmentationStore((s) => s.labels)
  const updateLabel = useSegmentationStore((s) => s.updateLabel)

  if (labels.length === 0) {
    return (
      <div style={{ color: '#555', fontSize: '12px', textAlign: 'center', marginTop: '8px' }}>
        No segmentation labels
      </div>
    )
  }

  return (
    <div style={{ display: 'flex', flexDirection: 'column', gap: '8px' }}>
      <div style={sectionLabel}>Overlay Visibility &amp; Opacity</div>
      {labels.map((label) => (
        <div key={label.id} style={{ display: 'flex', flexDirection: 'column', gap: '4px' }}>
          <div style={{ display: 'flex', alignItems: 'center', gap: '6px' }}>
            {/* Color swatch */}
            <div
              style={{
                width: '12px',
                height: '12px',
                borderRadius: '2px',
                background: colorToCss(label.color),
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
            <span
              style={{
                flex: 1,
                fontSize: '12px',
                color: label.visible ? '#ccc' : '#555',
                overflow: 'hidden',
                textOverflow: 'ellipsis',
                whiteSpace: 'nowrap',
              }}
            >
              {label.name}
            </span>
            <span style={{ fontSize: '11px', color: '#666', flexShrink: 0 }}>
              {Math.round(label.opacity * 100)}%
            </span>
          </div>
          {/* Opacity slider */}
          <input
            type="range"
            min={0}
            max={1}
            step={0.05}
            value={label.opacity}
            disabled={!label.visible}
            onChange={(e) => updateLabel(label.id, { opacity: Number(e.target.value) })}
            style={{ width: '100%', opacity: label.visible ? 1 : 0.3 }}
          />
        </div>
      ))}
    </div>
  )
}

const sectionLabel: React.CSSProperties = {
  fontSize: '11px',
  color: '#666',
  textTransform: 'uppercase',
  letterSpacing: '0.05em',
}
