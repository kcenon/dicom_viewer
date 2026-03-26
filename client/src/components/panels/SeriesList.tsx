// SeriesList displays the series within an expanded study in PatientBrowser.

import { useLoadStudy } from '@/api/endpoints'
import { useViewportStore } from '@/stores/viewportStore'

interface SeriesListProps {
  studyInstanceUid: string
}

export function SeriesList({ studyInstanceUid }: SeriesListProps) {
  const { data: study, isLoading } = useLoadStudy(studyInstanceUid)
  const setChannelSeries = useViewportStore((s) => s.setChannelSeries)
  const activeViewportIndex = useViewportStore((s) => s.activeViewportIndex)
  const channels = useViewportStore((s) => s.channels)

  const handleSeriesClick = (seriesInstanceUid: string) => {
    const channel = channels[activeViewportIndex]
    if (channel) {
      setChannelSeries(channel.channelId, studyInstanceUid, seriesInstanceUid)
    }
  }

  if (isLoading) {
    return (
      <div style={loadingStyle}>Loading series...</div>
    )
  }

  const series = study?.series ?? []

  if (series.length === 0) {
    return (
      <div style={emptyStyle}>No series in this study</div>
    )
  }

  return (
    <div style={containerStyle}>
      {series.map((s) => (
        <div
          key={s.seriesInstanceUid}
          onClick={() => handleSeriesClick(s.seriesInstanceUid)}
          style={seriesRowStyle}
        >
          <div style={{ color: '#ccc', fontWeight: 500 }}>
            #{s.seriesNumber} &mdash; {s.seriesDescription || 'No description'}
          </div>
          <div style={{ color: '#888', marginTop: '1px' }}>
            {s.modality} &bull; {s.frameCount} images
          </div>
        </div>
      ))}
    </div>
  )
}

const containerStyle: React.CSSProperties = {
  display: 'flex',
  flexDirection: 'column',
  gap: '2px',
  paddingLeft: '12px',
  borderLeft: '2px solid #444',
  marginLeft: '6px',
}

const seriesRowStyle: React.CSSProperties = {
  padding: '4px 6px',
  borderRadius: '3px',
  cursor: 'pointer',
  fontSize: '11px',
  userSelect: 'none',
  background: '#1e1e1e',
}

const loadingStyle: React.CSSProperties = {
  color: '#666',
  fontSize: '11px',
  textAlign: 'center',
  padding: '4px 0',
}

const emptyStyle: React.CSSProperties = {
  color: '#555',
  fontSize: '11px',
  textAlign: 'center',
  padding: '4px 0',
}
