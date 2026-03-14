// TanStack Query hooks for REST API endpoints

import { useMutation, useQuery, useQueryClient } from '@tanstack/react-query'
import { http } from './httpClient'
import type {
  SessionCreateRequest,
  SessionCreateResponse,
  StudyDetail,
  StudySummary,
  PacsServer,
  PacsQueryParams,
  PacsQueryResult,
  ExportReportRequest,
  ExportCsvRequest,
  LoginRequest,
  LoginResponse,
  RefreshTokenRequest,
  RefreshTokenResponse,
} from '@/types/api'

// --- Session ---

export function useCreateSession() {
  return useMutation({
    mutationFn: (req: SessionCreateRequest) =>
      http.post<SessionCreateResponse>('/sessions', req),
  })
}

export function useDestroySession() {
  const queryClient = useQueryClient()
  return useMutation({
    mutationFn: (sessionId: string) =>
      http.delete<void>(`/sessions/${sessionId}`),
    onSuccess: () => {
      queryClient.invalidateQueries({ queryKey: ['sessions'] })
    },
  })
}

// --- Studies ---

export function useStudies() {
  return useQuery({
    queryKey: ['studies'],
    queryFn: () => http.get<StudySummary[]>('/studies'),
  })
}

export function useLoadStudy(studyInstanceUid: string) {
  return useQuery({
    queryKey: ['studies', studyInstanceUid],
    queryFn: () => http.get<StudyDetail>(`/studies/${studyInstanceUid}`),
    enabled: studyInstanceUid.length > 0,
  })
}

// --- PACS ---

export function usePacsServers() {
  return useQuery({
    queryKey: ['pacs', 'servers'],
    queryFn: () => http.get<PacsServer[]>('/pacs/servers'),
  })
}

export function usePacsQuery() {
  return useMutation({
    mutationFn: (params: PacsQueryParams) =>
      http.post<PacsQueryResult>('/pacs/query', params),
  })
}

// --- Export ---

export function useExportReport() {
  return useMutation({
    mutationFn: (req: ExportReportRequest) =>
      http.post<Blob>('/export/report', req),
  })
}

export function useExportCsv() {
  return useMutation({
    mutationFn: (req: ExportCsvRequest) =>
      http.post<Blob>('/export/csv', req),
  })
}

// --- Auth ---

export function useLogin() {
  return useMutation({
    mutationFn: (req: LoginRequest) =>
      http.post<LoginResponse>('/auth/login', req),
  })
}

export function useLogout() {
  return useMutation({
    mutationFn: () => http.post<void>('/auth/logout'),
  })
}

export function useRefreshToken() {
  return useMutation({
    mutationFn: (req: RefreshTokenRequest) =>
      http.post<RefreshTokenResponse>('/auth/refresh', req),
  })
}
