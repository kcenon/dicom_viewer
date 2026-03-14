import { create } from 'zustand'
import type { Measurement, MeasurementTool } from '@/types/models'

interface MeasurementState {
  measurements: Measurement[]
  activeTool: MeasurementTool
  selectedMeasurementId: string | null
  addMeasurement: (measurement: Measurement) => void
  removeMeasurement: (id: string) => void
  updateMeasurement: (id: string, partial: Partial<Measurement>) => void
  setActiveTool: (tool: MeasurementTool) => void
  selectMeasurement: (id: string | null) => void
  clearAll: (channelId?: number) => void
}

export const useMeasurementStore = create<MeasurementState>((set) => ({
  measurements: [],
  activeTool: 'none',
  selectedMeasurementId: null,

  addMeasurement: (measurement) =>
    set((state) => ({ measurements: [...state.measurements, measurement] })),

  removeMeasurement: (id) =>
    set((state) => ({
      measurements: state.measurements.filter((m) => m.id !== id),
      selectedMeasurementId:
        state.selectedMeasurementId === id ? null : state.selectedMeasurementId,
    })),

  updateMeasurement: (id, partial) =>
    set((state) => ({
      measurements: state.measurements.map((m) =>
        m.id === id ? { ...m, ...partial } : m
      ),
    })),

  setActiveTool: (activeTool) => set({ activeTool }),

  selectMeasurement: (selectedMeasurementId) => set({ selectedMeasurementId }),

  clearAll: (channelId) =>
    set((state) => ({
      measurements:
        channelId !== undefined
          ? state.measurements.filter((m) => m.channelId !== channelId)
          : [],
      selectedMeasurementId: null,
    })),
}))
