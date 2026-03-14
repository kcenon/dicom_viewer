import { create } from 'zustand'
import type { QualityMetrics } from '@/types/models'

type ConnectionStatus = 'disconnected' | 'connecting' | 'connected' | 'error'

interface SessionState {
  sessionId: string | null
  wsUrl: string | null
  connectionStatus: ConnectionStatus
  metrics: QualityMetrics
  setSession: (sessionId: string, wsUrl: string) => void
  clearSession: () => void
  setConnectionStatus: (status: ConnectionStatus) => void
  updateMetrics: (metrics: Partial<QualityMetrics>) => void
}

const DEFAULT_METRICS: QualityMetrics = {
  fps: 0,
  latencyMs: 0,
  droppedFrames: 0,
  bandwidthKbps: 0,
}

export const useSessionStore = create<SessionState>((set) => ({
  sessionId: null,
  wsUrl: null,
  connectionStatus: 'disconnected',
  metrics: DEFAULT_METRICS,

  setSession: (sessionId, wsUrl) =>
    set({ sessionId, wsUrl }),

  clearSession: () =>
    set({ sessionId: null, wsUrl: null, connectionStatus: 'disconnected', metrics: DEFAULT_METRICS }),

  setConnectionStatus: (connectionStatus) =>
    set({ connectionStatus }),

  updateMetrics: (partial) =>
    set((state) => ({ metrics: { ...state.metrics, ...partial } })),
}))
