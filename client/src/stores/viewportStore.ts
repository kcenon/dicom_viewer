import { create } from 'zustand'
import type { ViewportLayout, ViewportChannel } from '@/types/models'

interface FrameResolution {
  width: number
  height: number
}

interface ViewportState {
  layout: ViewportLayout
  activeViewportIndex: number
  channels: ViewportChannel[]
  frameResolutions: Record<number, FrameResolution>
  setLayout: (layout: ViewportLayout) => void
  setActiveViewport: (index: number) => void
  assignChannel: (viewportIndex: number, channelId: number) => void
  setChannelSeries: (
    channelId: number,
    studyInstanceUid: string,
    seriesInstanceUid: string
  ) => void
  clearChannel: (viewportIndex: number) => void
  setFrameResolution: (channelId: number, width: number, height: number) => void
}

function layoutToViewportCount(layout: ViewportLayout): number {
  switch (layout) {
    case '1x1': return 1
    case '1x2': return 2
    case '2x2': return 4
    case '2x3': return 6
    case '3x3': return 9
  }
}

function initChannels(count: number): ViewportChannel[] {
  return Array.from({ length: count }, (_, i) => ({
    channelId: i,
    viewportIndex: i,
  }))
}

export const useViewportStore = create<ViewportState>((set) => ({
  layout: '1x1',
  activeViewportIndex: 0,
  channels: initChannels(1),
  frameResolutions: {},

  setLayout: (layout) =>
    set({
      layout,
      channels: initChannels(layoutToViewportCount(layout)),
      activeViewportIndex: 0,
    }),

  setActiveViewport: (activeViewportIndex) => set({ activeViewportIndex }),

  assignChannel: (viewportIndex, channelId) =>
    set((state) => ({
      channels: state.channels.map((ch) =>
        ch.viewportIndex === viewportIndex ? { ...ch, channelId } : ch
      ),
    })),

  setChannelSeries: (channelId, studyInstanceUid, seriesInstanceUid) =>
    set((state) => ({
      channels: state.channels.map((ch) =>
        ch.channelId === channelId
          ? { ...ch, studyInstanceUid, seriesInstanceUid }
          : ch
      ),
    })),

  clearChannel: (viewportIndex) =>
    set((state) => ({
      channels: state.channels.map((ch) =>
        ch.viewportIndex === viewportIndex
          ? { channelId: ch.channelId, viewportIndex }
          : ch
      ),
    })),

  setFrameResolution: (channelId, width, height) =>
    set((state) => {
      const prev = state.frameResolutions[channelId]
      if (prev && prev.width === width && prev.height === height) return state
      return {
        frameResolutions: { ...state.frameResolutions, [channelId]: { width, height } },
      }
    }),
}))
