// ReportPanel triggers PDF/CSV export for the currently selected study via REST API.

import { useExportReport, useExportCsv } from '@/api/endpoints'
import { useStudyStore } from '@/stores/studyStore'

export function ReportPanel() {
  const selectedStudyUid = useStudyStore((s) => s.selectedStudyUid)
  const exportReport = useExportReport()
  const exportCsv = useExportCsv()

  const downloadBlob = (blob: Blob, filename: string) => {
    const url = URL.createObjectURL(blob)
    const a = document.createElement('a')
    a.href = url
    a.download = filename
    a.click()
    URL.revokeObjectURL(url)
  }

  const handleExportPdf = () => {
    if (selectedStudyUid === null) return
    exportReport.mutate(
      { studyInstanceUid: selectedStudyUid, format: 'pdf', includeImages: true },
      {
        onSuccess: (blob) => downloadBlob(blob, `report-${selectedStudyUid}.pdf`),
      }
    )
  }

  const handleExportDocx = () => {
    if (selectedStudyUid === null) return
    exportReport.mutate(
      { studyInstanceUid: selectedStudyUid, format: 'docx', includeImages: false },
      {
        onSuccess: (blob) => downloadBlob(blob, `report-${selectedStudyUid}.docx`),
      }
    )
  }

  const handleExportCsv = () => {
    if (selectedStudyUid === null) return
    exportCsv.mutate(
      { studyInstanceUid: selectedStudyUid, metrics: ['flow', 'measurements'] },
      {
        onSuccess: (blob) => downloadBlob(blob, `metrics-${selectedStudyUid}.csv`),
      }
    )
  }

  if (selectedStudyUid === null) {
    return (
      <div style={{ color: '#555', fontSize: '12px', textAlign: 'center', marginTop: '8px' }}>
        Select a study to export
      </div>
    )
  }

  return (
    <div style={{ display: 'flex', flexDirection: 'column', gap: '8px' }}>
      <div style={sectionLabel}>Export</div>
      <button
        onClick={handleExportPdf}
        disabled={exportReport.isPending}
        style={buttonStyle}
      >
        {exportReport.isPending ? 'Generating…' : 'Download PDF Report'}
      </button>
      <button
        onClick={handleExportDocx}
        disabled={exportReport.isPending}
        style={buttonStyle}
      >
        Download DOCX Report
      </button>
      <button
        onClick={handleExportCsv}
        disabled={exportCsv.isPending}
        style={buttonStyle}
      >
        {exportCsv.isPending ? 'Exporting…' : 'Export Metrics CSV'}
      </button>
      {exportReport.isError && (
        <div style={{ color: '#ef9a9a', fontSize: '12px' }}>
          Export failed: {exportReport.error instanceof Error ? exportReport.error.message : 'Unknown error'}
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

const buttonStyle: React.CSSProperties = {
  background: '#2a2a2a',
  border: '1px solid #444',
  borderRadius: '3px',
  color: '#ccc',
  cursor: 'pointer',
  fontSize: '12px',
  padding: '6px 8px',
  textAlign: 'left',
  width: '100%',
}
