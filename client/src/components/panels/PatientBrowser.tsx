// PatientBrowser shows a local study list and PACS query interface.
// Local studies are fetched via useStudies(); PACS results are stored in pacsStore.

import { useState } from 'react'
import { useStudies, usePacsQuery } from '@/api/endpoints'
import { useStudyStore } from '@/stores/studyStore'
import { usePacsStore } from '@/stores/pacsStore'
import { SeriesList } from './SeriesList'
import type { PacsQueryParams } from '@/types/api'

const rowStyle: React.CSSProperties = {
  padding: '6px 8px',
  borderRadius: '3px',
  cursor: 'pointer',
  fontSize: '12px',
  userSelect: 'none',
}

export function PatientBrowser() {
  const [patientName, setPatientName] = useState('')
  const [patientId, setPatientId] = useState('')

  const { data: studies, isLoading } = useStudies()

  const pacsQueryMutation = usePacsQuery()
  const pacsServers = usePacsStore((s) => s.servers)
  const queryResults = usePacsStore((s) => s.queryResults)
  const isQuerying = usePacsStore((s) => s.isQuerying)
  const setQueryResults = usePacsStore((s) => s.setQueryResults)
  const setQuerying = usePacsStore((s) => s.setQuerying)
  const setQueryError = usePacsStore((s) => s.setQueryError)
  const clearQueryResults = usePacsStore((s) => s.clearQueryResults)

  const selectedStudyUid = useStudyStore((s) => s.selectedStudyUid)
  const selectStudy = useStudyStore((s) => s.selectStudy)
  const expandedStudyUid = useStudyStore((s) => s.expandedStudyUid)
  const toggleStudyExpansion = useStudyStore((s) => s.toggleStudyExpansion)

  const firstServer = pacsServers[0]

  const handleSearch = () => {
    if (firstServer === undefined) return
    setQuerying(true)

    const params: PacsQueryParams = { serverId: firstServer.id }
    if (patientName.trim()) params.patientName = patientName.trim()
    if (patientId.trim()) params.patientId = patientId.trim()

    pacsQueryMutation.mutate(params, {
      onSuccess: (result) => {
        setQueryResults(result.studies)
      },
      onError: (err) => {
        setQueryError(err instanceof Error ? err.message : 'PACS query failed')
      },
    })
  }

  const displayStudies = queryResults.length > 0 ? queryResults : (studies ?? [])

  return (
    <div style={{ display: 'flex', flexDirection: 'column', gap: '6px' }}>
      {/* PACS search form — only when a server is configured */}
      {firstServer !== undefined && (
        <div style={{ display: 'flex', flexDirection: 'column', gap: '4px' }}>
          <input
            placeholder="Patient name"
            value={patientName}
            onChange={(e) => setPatientName(e.target.value)}
            style={inputStyle}
          />
          <input
            placeholder="Patient ID"
            value={patientId}
            onChange={(e) => setPatientId(e.target.value)}
            style={inputStyle}
          />
          <div style={{ display: 'flex', gap: '4px' }}>
            <button
              onClick={handleSearch}
              disabled={isQuerying}
              style={{ ...actionButtonStyle, flex: 1 }}
            >
              {isQuerying ? 'Searching…' : 'Search PACS'}
            </button>
            {queryResults.length > 0 && (
              <button onClick={clearQueryResults} style={actionButtonStyle}>
                Clear
              </button>
            )}
          </div>
        </div>
      )}

      {/* Study list */}
      {isLoading && (
        <div style={{ color: '#666', fontSize: '12px', textAlign: 'center' }}>
          Loading…
        </div>
      )}

      {displayStudies.map((study) => {
        const isActive = study.studyInstanceUid === selectedStudyUid
        const isExpanded = study.studyInstanceUid === expandedStudyUid
        return (
          <div key={study.studyInstanceUid}>
            <div
              onClick={() => {
                selectStudy(study.studyInstanceUid)
                toggleStudyExpansion(study.studyInstanceUid)
              }}
              style={{
                ...rowStyle,
                background: isActive ? '#1565c0' : '#2a2a2a',
              }}
            >
              <div style={{ color: '#e0e0e0', fontWeight: 500 }}>
                {study.patientName || '—'}
              </div>
              <div style={{ color: '#aaa', marginTop: '2px' }}>
                {study.studyDate} &mdash; {study.studyDescription || 'No description'}
              </div>
              <div style={{ color: '#666', marginTop: '2px' }}>
                {study.modalities.join(', ')} &bull; {study.seriesCount} series
              </div>
            </div>
            {isExpanded && (
              <SeriesList studyInstanceUid={study.studyInstanceUid} />
            )}
          </div>
        )
      })}

      {displayStudies.length === 0 && !isLoading && (
        <div style={{ color: '#555', fontSize: '12px', textAlign: 'center', marginTop: '8px' }}>
          No studies
        </div>
      )}
    </div>
  )
}

const inputStyle: React.CSSProperties = {
  background: '#2a2a2a',
  border: '1px solid #444',
  borderRadius: '3px',
  color: '#ccc',
  fontSize: '12px',
  padding: '4px 6px',
  outline: 'none',
  width: '100%',
  boxSizing: 'border-box',
}

const actionButtonStyle: React.CSSProperties = {
  background: '#2a2a2a',
  border: '1px solid #444',
  borderRadius: '3px',
  color: '#ccc',
  cursor: 'pointer',
  fontSize: '12px',
  padding: '4px 8px',
}
