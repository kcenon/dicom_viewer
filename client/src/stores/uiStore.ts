import { create } from 'zustand'

type Theme = 'dark' | 'light'
type ActivePanel =
  | 'patientBrowser'
  | 'segmentation'
  | 'measurement'
  | 'flow'
  | 'overlay'
  | 'report'
  | null

interface UiState {
  theme: Theme
  isSidePanelOpen: boolean
  activePanel: ActivePanel
  isStatusBarVisible: boolean
  isToolBarVisible: boolean
  isPacsConfigOpen: boolean
  isSettingsOpen: boolean
  setTheme: (theme: Theme) => void
  setSidePanelOpen: (open: boolean) => void
  setActivePanel: (panel: ActivePanel) => void
  toggleSidePanel: () => void
  setStatusBarVisible: (visible: boolean) => void
  setToolBarVisible: (visible: boolean) => void
  openPacsConfig: () => void
  closePacsConfig: () => void
  openSettings: () => void
  closeSettings: () => void
}

export const useUiStore = create<UiState>((set) => ({
  theme: 'dark',
  isSidePanelOpen: true,
  activePanel: 'patientBrowser',
  isStatusBarVisible: true,
  isToolBarVisible: true,
  isPacsConfigOpen: false,
  isSettingsOpen: false,

  setTheme: (theme) => set({ theme }),

  setSidePanelOpen: (isSidePanelOpen) => set({ isSidePanelOpen }),

  setActivePanel: (activePanel) =>
    set({ activePanel, isSidePanelOpen: activePanel !== null }),

  toggleSidePanel: () =>
    set((state) => ({ isSidePanelOpen: !state.isSidePanelOpen })),

  setStatusBarVisible: (isStatusBarVisible) => set({ isStatusBarVisible }),

  setToolBarVisible: (isToolBarVisible) => set({ isToolBarVisible }),

  openPacsConfig: () => set({ isPacsConfigOpen: true }),
  closePacsConfig: () => set({ isPacsConfigOpen: false }),
  openSettings: () => set({ isSettingsOpen: true }),
  closeSettings: () => set({ isSettingsOpen: false }),
}))
