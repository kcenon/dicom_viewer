import { create } from 'zustand'
import type { StudyDetail, StudySummary } from '@/types/api'

interface StudyState {
  loadedStudy: StudyDetail | null
  studyList: StudySummary[]
  selectedStudyUid: string | null
  selectedSeriesUid: string | null
  expandedStudyUid: string | null
  isStudyBrowserOpen: boolean
  setStudyList: (studies: StudySummary[]) => void
  setLoadedStudy: (study: StudyDetail | null) => void
  selectStudy: (uid: string | null) => void
  selectSeries: (uid: string | null) => void
  toggleStudyExpansion: (uid: string) => void
  setStudyBrowserOpen: (open: boolean) => void
}

export const useStudyStore = create<StudyState>((set) => ({
  loadedStudy: null,
  studyList: [],
  selectedStudyUid: null,
  selectedSeriesUid: null,
  expandedStudyUid: null,
  isStudyBrowserOpen: false,

  setStudyList: (studyList) => set({ studyList }),

  setLoadedStudy: (loadedStudy) =>
    set({ loadedStudy, selectedSeriesUid: null }),

  selectStudy: (selectedStudyUid) =>
    set({ selectedStudyUid, selectedSeriesUid: null }),

  selectSeries: (selectedSeriesUid) => set({ selectedSeriesUid }),

  toggleStudyExpansion: (uid) =>
    set((state) => ({
      expandedStudyUid: state.expandedStudyUid === uid ? null : uid,
    })),

  setStudyBrowserOpen: (isStudyBrowserOpen) => set({ isStudyBrowserOpen }),
}))
