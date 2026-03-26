// MeasurementPanel provides tool selection, measurement list, and lifecycle controls.

import { useState } from 'react'
import { useMeasurementStore } from '@/stores/measurementStore'
import type { MeasurementTool, Measurement } from '@/types/models'

const TOOLS: { id: Exclude<MeasurementTool, 'none'>; label: string; color: string }[] = [
  { id: 'length', label: 'Length', color: '#ffeb3b' },
  { id: 'angle', label: 'Angle', color: '#4fc3f7' },
  { id: 'ellipse', label: 'Ellipse', color: '#a5d6a7' },
  { id: 'rectangle', label: 'Rect', color: '#ce93d8' },
  { id: 'arrow', label: 'Arrow', color: '#ff8a65' },
]

const TOOL_COLORS: Record<string, string> = {
  length: '#ffeb3b',
  angle: '#4fc3f7',
  ellipse: '#a5d6a7',
  rectangle: '#ce93d8',
  arrow: '#ff8a65',
}

function formatValue(m: Measurement): string {
  if (m.type === 'arrow') return m.label ?? ''
  if (m.value === undefined) return ''
  if (m.type === 'angle') return `${m.value.toFixed(1)}\u00B0`
  return `${m.value.toFixed(2)} ${m.unit ?? ''}`
}

function MeasurementRow({ m }: { m: Measurement }) {
  const selectedId = useMeasurementStore((s) => s.selectedMeasurementId)
  const selectMeasurement = useMeasurementStore((s) => s.selectMeasurement)
  const updateMeasurement = useMeasurementStore((s) => s.updateMeasurement)
  const removeMeasurement = useMeasurementStore((s) => s.removeMeasurement)
  const [editing, setEditing] = useState(false)
  const [draft, setDraft] = useState(m.label ?? '')

  const isSelected = selectedId === m.id
  const color = TOOL_COLORS[m.type] ?? '#aaa'

  const commitLabel = () => {
    updateMeasurement(m.id, { label: draft || undefined })
    setEditing(false)
  }

  return (
    <div
      onClick={() => selectMeasurement(isSelected ? null : m.id)}
      style={{
        display: 'flex',
        alignItems: 'center',
        gap: '6px',
        padding: '4px 4px',
        borderBottom: '1px solid #2a2a2a',
        background: isSelected ? '#2a3a4a' : 'transparent',
        cursor: 'pointer',
        borderRadius: '2px',
      }}
    >
      {/* Type color indicator */}
      <div
        style={{
          width: '10px',
          height: '10px',
          borderRadius: '2px',
          background: color,
          flexShrink: 0,
        }}
      />

      {/* Value */}
      <span
        style={{
          flex: 1,
          fontSize: '12px',
          color: '#ccc',
          overflow: 'hidden',
          textOverflow: 'ellipsis',
          whiteSpace: 'nowrap',
        }}
      >
        {formatValue(m) || m.type}
      </span>

      {/* Editable label */}
      {editing ? (
        <input
          autoFocus
          value={draft}
          onChange={(e) => setDraft(e.target.value)}
          onBlur={commitLabel}
          onKeyDown={(e) => {
            if (e.key === 'Enter') commitLabel()
            if (e.key === 'Escape') setEditing(false)
          }}
          onClick={(e) => e.stopPropagation()}
          style={{
            width: '60px',
            fontSize: '11px',
            background: '#1a1a1a',
            border: '1px solid #555',
            borderRadius: '2px',
            color: '#ccc',
            padding: '1px 3px',
          }}
        />
      ) : (
        <span
          onDoubleClick={(e) => {
            e.stopPropagation()
            setDraft(m.label ?? '')
            setEditing(true)
          }}
          style={{
            fontSize: '11px',
            color: '#777',
            maxWidth: '60px',
            overflow: 'hidden',
            textOverflow: 'ellipsis',
            whiteSpace: 'nowrap',
          }}
          title="Double-click to edit label"
        >
          {m.label || '—'}
        </span>
      )}

      {/* Delete */}
      <button
        onClick={(e) => {
          e.stopPropagation()
          removeMeasurement(m.id)
        }}
        style={{ ...chipStyle, padding: '1px 5px', color: '#ef9a9a' }}
      >
        ✕
      </button>
    </div>
  )
}

export function MeasurementPanel() {
  const activeTool = useMeasurementStore((s) => s.activeTool)
  const setActiveTool = useMeasurementStore((s) => s.setActiveTool)
  const measurements = useMeasurementStore((s) => s.measurements)
  const clearAll = useMeasurementStore((s) => s.clearAll)
  const [confirmClear, setConfirmClear] = useState(false)

  return (
    <div style={{ display: 'flex', flexDirection: 'column', gap: '10px' }}>
      {/* Tool selector */}
      <div>
        <div style={sectionLabel}>Tool</div>
        <div style={{ display: 'flex', flexWrap: 'wrap', gap: '4px', marginTop: '4px' }}>
          {TOOLS.map(({ id, label, color }) => (
            <button
              key={id}
              onClick={() => setActiveTool(activeTool === id ? 'none' : id)}
              style={{
                ...chipStyle,
                background: activeTool === id ? color : '#2a2a2a',
                color: activeTool === id ? '#000' : '#aaa',
                borderColor: activeTool === id ? color : '#444',
              }}
            >
              {label}
            </button>
          ))}
        </div>
      </div>

      {/* Measurement list */}
      <div>
        <div style={sectionLabel}>Measurements ({measurements.length})</div>
        <div style={{ marginTop: '4px' }}>
          {measurements.map((m) => (
            <MeasurementRow key={m.id} m={m} />
          ))}
          {measurements.length === 0 && (
            <div style={{ color: '#555', fontSize: '12px', textAlign: 'center', marginTop: '8px' }}>
              No measurements. Select a tool and click on the viewport.
            </div>
          )}
        </div>
      </div>

      {/* Clear All */}
      {measurements.length > 0 && (
        confirmClear ? (
          <div style={{ display: 'flex', gap: '4px' }}>
            <button
              onClick={() => {
                clearAll()
                setConfirmClear(false)
              }}
              style={{ ...actionButtonStyle, color: '#ef9a9a', flex: 1 }}
            >
              Confirm
            </button>
            <button
              onClick={() => setConfirmClear(false)}
              style={{ ...actionButtonStyle, flex: 1 }}
            >
              Cancel
            </button>
          </div>
        ) : (
          <button
            onClick={() => setConfirmClear(true)}
            style={actionButtonStyle}
          >
            Clear All
          </button>
        )
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
