// REST API request and response types

export interface SessionCreateRequest {
  width: number
  height: number
  quality?: number
}

export interface SessionCreateResponse {
  sessionId: string
  wsUrl: string
  channelId: number
}

export interface StudySummary {
  studyInstanceUid: string
  patientName: string
  patientId: string
  studyDate: string
  studyDescription: string
  modalities: string[]
  seriesCount: number
}

export interface SeriesSummary {
  seriesInstanceUid: string
  seriesNumber: number
  seriesDescription: string
  modality: string
  frameCount: number
  rows: number
  columns: number
}

export interface StudyDetail extends StudySummary {
  series: SeriesSummary[]
}

export interface PacsServer {
  id: string
  name: string
  aet: string
  host: string
  port: number
  tlsEnabled: boolean
}

export interface PacsQueryParams {
  serverId: string
  patientId?: string
  patientName?: string
  studyDate?: string
  modality?: string
  accessionNumber?: string
}

export interface PacsQueryResult {
  studies: StudySummary[]
}

export interface ExportReportRequest {
  studyInstanceUid: string
  format: 'pdf' | 'docx'
  includeImages: boolean
}

export interface ExportCsvRequest {
  studyInstanceUid: string
  metrics: string[]
}

export interface ApiError {
  code: string
  message: string
  details?: Record<string, unknown>
}
