// REST API request and response types

export interface SessionCreateRequest {
  width: number
  height: number
  quality?: number
}

export interface SessionCreateResponse {
  sessionId: string
  wsUrl: string
  token: string
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

// Auth types

export type UserRole = 'admin' | 'radiologist' | 'technician' | 'viewer'

export interface AuthUser {
  id: string
  username: string
  role: UserRole
  expiresAt: number // Unix timestamp in ms
}

export interface LoginRequest {
  username: string
  password: string
}

export interface LoginResponse {
  refreshToken: string
  csrfToken: string
  expiresAt: number
  user: AuthUser
}

export interface RefreshTokenRequest {
  refreshToken: string
}

export interface RefreshTokenResponse {
  csrfToken: string
  expiresAt: number
}

export interface AuthMeResponse {
  id: string
  role: UserRole
}

export interface CsrfTokenResponse {
  csrfToken: string
}
