import { create } from 'zustand'
import type { FlowQuantificationData } from '@/types/models'

interface FlowState {
  phaseIndex: number
  totalPhases: number
  isPlaying: boolean
  playbackFps: number
  quantificationData: FlowQuantificationData | null
  setPhaseIndex: (index: number) => void
  setTotalPhases: (total: number) => void
  setPlaying: (playing: boolean) => void
  setPlaybackFps: (fps: number) => void
  setQuantificationData: (data: FlowQuantificationData | null) => void
  nextPhase: () => void
  prevPhase: () => void
}

export const useFlowStore = create<FlowState>((set) => ({
  phaseIndex: 0,
  totalPhases: 0,
  isPlaying: false,
  playbackFps: 10,
  quantificationData: null,

  setPhaseIndex: (phaseIndex) => set({ phaseIndex }),

  setTotalPhases: (totalPhases) => set({ totalPhases }),

  setPlaying: (isPlaying) => set({ isPlaying }),

  setPlaybackFps: (playbackFps) => set({ playbackFps }),

  setQuantificationData: (quantificationData) => set({ quantificationData }),

  nextPhase: () =>
    set((state) =>
      state.totalPhases > 0
        ? { phaseIndex: (state.phaseIndex + 1) % state.totalPhases }
        : {}
    ),

  prevPhase: () =>
    set((state) =>
      state.totalPhases > 0
        ? {
            phaseIndex:
              (state.phaseIndex - 1 + state.totalPhases) % state.totalPhases,
          }
        : {}
    ),
}))
