// MeasurementOverlay renders SVG annotations for measurements associated
// with a given channelId. Positioned absolutely over the canvas.

import { useMeasurementStore } from '@/stores/measurementStore'
import type { Measurement } from '@/types/models'

interface Props {
  channelId: number
  width: number
  height: number
}

function renderMeasurement(m: Measurement, isSelected: boolean) {
  const { points } = m
  const sw = isSelected ? 3 : 1.5

  switch (m.type) {
    case 'length': {
      if (points.length < 2) return null
      const [p0, p1] = points as [{ x: number; y: number }, { x: number; y: number }]
      const mx = (p0.x + p1.x) / 2
      const my = (p0.y + p1.y) / 2
      return (
        <g key={m.id} stroke="#ffeb3b" strokeWidth={sw} fill="none">
          <line x1={p0.x} y1={p0.y} x2={p1.x} y2={p1.y} />
          {m.value !== undefined && (
            <text x={mx} y={my - 4} fill="#ffeb3b" fontSize={12} stroke="none">
              {m.value.toFixed(2)} {m.unit ?? ''}
            </text>
          )}
        </g>
      )
    }

    case 'angle': {
      if (points.length < 3) return null
      const [a, b, c] = points as [
        { x: number; y: number },
        { x: number; y: number },
        { x: number; y: number }
      ]
      return (
        <g key={m.id} stroke="#4fc3f7" strokeWidth={sw} fill="none">
          <line x1={a.x} y1={a.y} x2={b.x} y2={b.y} />
          <line x1={b.x} y1={b.y} x2={c.x} y2={c.y} />
          {m.value !== undefined && (
            <text x={b.x + 4} y={b.y - 4} fill="#4fc3f7" fontSize={12} stroke="none">
              {m.value.toFixed(1)}°
            </text>
          )}
        </g>
      )
    }

    case 'ellipse': {
      if (points.length < 2) return null
      const [start, end] = points as [{ x: number; y: number }, { x: number; y: number }]
      const cx = (start.x + end.x) / 2
      const cy = (start.y + end.y) / 2
      const rx = Math.abs(end.x - start.x) / 2
      const ry = Math.abs(end.y - start.y) / 2
      return (
        <g key={m.id} stroke="#a5d6a7" strokeWidth={sw} fill="none">
          <ellipse cx={cx} cy={cy} rx={rx} ry={ry} />
          {m.value !== undefined && (
            <text x={cx} y={cy - ry - 4} fill="#a5d6a7" fontSize={12} stroke="none">
              {m.value.toFixed(2)} {m.unit ?? ''}
            </text>
          )}
        </g>
      )
    }

    case 'rectangle': {
      if (points.length < 2) return null
      const [tl, br] = points as [{ x: number; y: number }, { x: number; y: number }]
      const w = br.x - tl.x
      const h = br.y - tl.y
      return (
        <g key={m.id} stroke="#ce93d8" strokeWidth={sw} fill="none">
          <rect x={tl.x} y={tl.y} width={w} height={h} />
          {m.value !== undefined && (
            <text x={tl.x} y={tl.y - 4} fill="#ce93d8" fontSize={12} stroke="none">
              {m.value.toFixed(2)} {m.unit ?? ''}
            </text>
          )}
        </g>
      )
    }

    case 'arrow': {
      if (points.length < 2) return null
      const [from, to] = points as [{ x: number; y: number }, { x: number; y: number }]
      const angle = Math.atan2(to.y - from.y, to.x - from.x)
      const arrowLen = 10
      const arrowAngle = Math.PI / 6
      const ax1 = to.x - arrowLen * Math.cos(angle - arrowAngle)
      const ay1 = to.y - arrowLen * Math.sin(angle - arrowAngle)
      const ax2 = to.x - arrowLen * Math.cos(angle + arrowAngle)
      const ay2 = to.y - arrowLen * Math.sin(angle + arrowAngle)
      return (
        <g key={m.id} stroke="#ff8a65" strokeWidth={sw} fill="none">
          <line x1={from.x} y1={from.y} x2={to.x} y2={to.y} />
          <polyline points={`${ax1},${ay1} ${to.x},${to.y} ${ax2},${ay2}`} />
          {m.label !== undefined && (
            <text x={to.x + 4} y={to.y} fill="#ff8a65" fontSize={12} stroke="none">
              {m.label}
            </text>
          )}
        </g>
      )
    }

    default:
      return null
  }
}

export function MeasurementOverlay({ channelId, width, height }: Props) {
  const measurements = useMeasurementStore((s) =>
    s.measurements.filter((m) => m.channelId === channelId)
  )
  const selectedId = useMeasurementStore((s) => s.selectedMeasurementId)

  if (measurements.length === 0) return null

  return (
    <svg
      style={{
        position: 'absolute',
        top: 0,
        left: 0,
        width: '100%',
        height: '100%',
        pointerEvents: 'none',
        overflow: 'visible',
      }}
      viewBox={`0 0 ${width} ${height}`}
      preserveAspectRatio="none"
    >
      {measurements.map((m) => renderMeasurement(m, m.id === selectedId))}
    </svg>
  )
}
