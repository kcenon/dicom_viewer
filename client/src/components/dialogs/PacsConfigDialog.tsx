// PacsConfigDialog manages PACS server configurations stored in pacsStore.
// Server configs are managed locally (no server-side persistence endpoint).

import { useState } from 'react'
import { usePacsStore } from '@/stores/pacsStore'
import { useUiStore } from '@/stores/uiStore'
import type { PacsServer } from '@/types/api'

interface ServerForm {
  name: string
  aet: string
  host: string
  port: string
  tlsEnabled: boolean
}

const EMPTY_FORM: ServerForm = {
  name: '',
  aet: '',
  host: '',
  port: '11112',
  tlsEnabled: false,
}

export function PacsConfigDialog() {
  const isPacsConfigOpen = useUiStore((s) => s.isPacsConfigOpen)
  const closePacsConfig = useUiStore((s) => s.closePacsConfig)
  const servers = usePacsStore((s) => s.servers)
  const setServers = usePacsStore((s) => s.setServers)

  const [form, setForm] = useState<ServerForm>(EMPTY_FORM)
  const [error, setError] = useState<string | null>(null)

  if (!isPacsConfigOpen) return null

  const handleAdd = () => {
    const portNum = parseInt(form.port, 10)
    if (!form.name.trim() || !form.aet.trim() || !form.host.trim()) {
      setError('Name, AET, and Host are required')
      return
    }
    if (isNaN(portNum) || portNum < 1 || portNum > 65535) {
      setError('Port must be 1–65535')
      return
    }

    const newServer: PacsServer = {
      id: `${Date.now()}`,
      name: form.name.trim(),
      aet: form.aet.trim(),
      host: form.host.trim(),
      port: portNum,
      tlsEnabled: form.tlsEnabled,
    }
    setServers([...servers, newServer])
    setForm(EMPTY_FORM)
    setError(null)
  }

  const handleRemove = (id: string) => {
    setServers(servers.filter((s) => s.id !== id))
  }

  return (
    <div style={overlayStyle} onClick={closePacsConfig}>
      <div style={dialogStyle} onClick={(e) => e.stopPropagation()}>
        <div style={titleStyle}>PACS Configuration</div>

        {/* Server list */}
        {servers.length > 0 && (
          <div style={{ marginBottom: '12px' }}>
            {servers.map((server) => (
              <div key={server.id} style={serverRowStyle}>
                <div style={{ flex: 1 }}>
                  <span style={{ color: '#e0e0e0', fontSize: '13px' }}>{server.name}</span>
                  <span style={{ color: '#666', fontSize: '12px', marginLeft: '8px' }}>
                    {server.aet} @ {server.host}:{server.port}
                    {server.tlsEnabled && ' (TLS)'}
                  </span>
                </div>
                <button
                  onClick={() => handleRemove(server.id)}
                  style={{ ...smallButtonStyle, color: '#ef9a9a' }}
                >
                  Remove
                </button>
              </div>
            ))}
          </div>
        )}

        {/* Add form */}
        <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr', gap: '8px' }}>
          <div style={fieldGroupStyle}>
            <label style={labelStyle}>Name</label>
            <input
              value={form.name}
              onChange={(e) => setForm({ ...form, name: e.target.value })}
              placeholder="Main PACS"
              style={inputStyle}
            />
          </div>
          <div style={fieldGroupStyle}>
            <label style={labelStyle}>AE Title</label>
            <input
              value={form.aet}
              onChange={(e) => setForm({ ...form, aet: e.target.value })}
              placeholder="PACS_AET"
              style={inputStyle}
            />
          </div>
          <div style={fieldGroupStyle}>
            <label style={labelStyle}>Host</label>
            <input
              value={form.host}
              onChange={(e) => setForm({ ...form, host: e.target.value })}
              placeholder="192.168.1.100"
              style={inputStyle}
            />
          </div>
          <div style={fieldGroupStyle}>
            <label style={labelStyle}>Port</label>
            <input
              type="number"
              value={form.port}
              onChange={(e) => setForm({ ...form, port: e.target.value })}
              placeholder="11112"
              style={inputStyle}
            />
          </div>
          <div style={{ ...fieldGroupStyle, flexDirection: 'row', alignItems: 'center', gap: '6px' }}>
            <input
              type="checkbox"
              id="pacs-tls"
              checked={form.tlsEnabled}
              onChange={(e) => setForm({ ...form, tlsEnabled: e.target.checked })}
            />
            <label htmlFor="pacs-tls" style={{ ...labelStyle, cursor: 'pointer' }}>
              TLS
            </label>
          </div>
        </div>

        {error !== null && (
          <div style={{ color: '#ef9a9a', fontSize: '12px', marginTop: '6px' }}>{error}</div>
        )}

        <div style={{ display: 'flex', gap: '8px', marginTop: '12px', justifyContent: 'flex-end' }}>
          <button onClick={handleAdd} style={primaryButtonStyle}>
            Add Server
          </button>
          <button onClick={closePacsConfig} style={secondaryButtonStyle}>
            Close
          </button>
        </div>
      </div>
    </div>
  )
}

const overlayStyle: React.CSSProperties = {
  position: 'fixed',
  inset: 0,
  background: 'rgba(0,0,0,0.6)',
  display: 'flex',
  alignItems: 'center',
  justifyContent: 'center',
  zIndex: 1000,
}

const dialogStyle: React.CSSProperties = {
  background: '#1e1e1e',
  border: '1px solid #333',
  borderRadius: '6px',
  padding: '20px',
  width: '480px',
  maxWidth: '90vw',
  maxHeight: '80vh',
  overflow: 'auto',
}

const titleStyle: React.CSSProperties = {
  fontSize: '16px',
  fontWeight: 600,
  color: '#e0e0e0',
  marginBottom: '16px',
}

const serverRowStyle: React.CSSProperties = {
  display: 'flex',
  alignItems: 'center',
  gap: '8px',
  padding: '6px 8px',
  background: '#2a2a2a',
  borderRadius: '3px',
  marginBottom: '4px',
}

const fieldGroupStyle: React.CSSProperties = {
  display: 'flex',
  flexDirection: 'column',
  gap: '4px',
}

const labelStyle: React.CSSProperties = {
  fontSize: '11px',
  color: '#666',
  textTransform: 'uppercase',
  letterSpacing: '0.05em',
}

const inputStyle: React.CSSProperties = {
  background: '#2a2a2a',
  border: '1px solid #444',
  borderRadius: '3px',
  color: '#ccc',
  fontSize: '13px',
  padding: '4px 8px',
  outline: 'none',
  width: '100%',
  boxSizing: 'border-box',
}

const smallButtonStyle: React.CSSProperties = {
  background: 'transparent',
  border: '1px solid #444',
  borderRadius: '3px',
  color: '#aaa',
  cursor: 'pointer',
  fontSize: '12px',
  padding: '2px 8px',
}

const primaryButtonStyle: React.CSSProperties = {
  background: '#1565c0',
  border: '1px solid #1976d2',
  borderRadius: '3px',
  color: '#fff',
  cursor: 'pointer',
  fontSize: '13px',
  padding: '6px 16px',
}

const secondaryButtonStyle: React.CSSProperties = {
  background: '#2a2a2a',
  border: '1px solid #444',
  borderRadius: '3px',
  color: '#ccc',
  cursor: 'pointer',
  fontSize: '13px',
  padding: '6px 16px',
}
