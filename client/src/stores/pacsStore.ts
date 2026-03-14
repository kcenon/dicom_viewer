import { create } from 'zustand'
import type { PacsServer, StudySummary } from '@/types/api'

interface PacsState {
  servers: PacsServer[]
  queryResults: StudySummary[]
  isQuerying: boolean
  lastQueryError: string | null
  setServers: (servers: PacsServer[]) => void
  setQueryResults: (results: StudySummary[]) => void
  setQuerying: (querying: boolean) => void
  setQueryError: (error: string | null) => void
  clearQueryResults: () => void
}

export const usePacsStore = create<PacsState>((set) => ({
  servers: [],
  queryResults: [],
  isQuerying: false,
  lastQueryError: null,

  setServers: (servers) => set({ servers }),

  setQueryResults: (queryResults) =>
    set({ queryResults, lastQueryError: null }),

  setQuerying: (isQuerying) => set({ isQuerying }),

  setQueryError: (lastQueryError) => set({ lastQueryError, isQuerying: false }),

  clearQueryResults: () => set({ queryResults: [], lastQueryError: null }),
}))
