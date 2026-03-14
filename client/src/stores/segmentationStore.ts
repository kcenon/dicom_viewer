import { create } from 'zustand'
import type { SegmentationLabel, SegmentationTool } from '@/types/models'

const MAX_UNDO_DEPTH = 20

interface SegmentationState {
  labels: SegmentationLabel[]
  activeLabelId: number | null
  activeTool: SegmentationTool
  undoDepth: number
  brushSize: number
  addLabel: (label: SegmentationLabel) => void
  removeLabel: (id: number) => void
  updateLabel: (id: number, partial: Partial<SegmentationLabel>) => void
  setActiveLabelId: (id: number | null) => void
  setActiveTool: (tool: SegmentationTool) => void
  pushUndo: () => void
  undo: () => void
  setBrushSize: (size: number) => void
}

export const useSegmentationStore = create<SegmentationState>((set) => ({
  labels: [],
  activeLabelId: null,
  activeTool: 'none',
  undoDepth: 0,
  brushSize: 10,

  addLabel: (label) =>
    set((state) => ({ labels: [...state.labels, label] })),

  removeLabel: (id) =>
    set((state) => ({
      labels: state.labels.filter((l) => l.id !== id),
      activeLabelId: state.activeLabelId === id ? null : state.activeLabelId,
    })),

  updateLabel: (id, partial) =>
    set((state) => ({
      labels: state.labels.map((l) => (l.id === id ? { ...l, ...partial } : l)),
    })),

  setActiveLabelId: (activeLabelId) => set({ activeLabelId }),

  setActiveTool: (activeTool) => set({ activeTool }),

  pushUndo: () =>
    set((state) => ({
      undoDepth: Math.min(state.undoDepth + 1, MAX_UNDO_DEPTH),
    })),

  undo: () =>
    set((state) => ({
      undoDepth: Math.max(state.undoDepth - 1, 0),
    })),

  setBrushSize: (brushSize) => set({ brushSize }),
}))
