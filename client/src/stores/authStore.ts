import { create } from 'zustand'
import { setCsrfToken, clearCsrfToken } from '@/api/httpClient'
import type { AuthUser } from '@/types/api'

interface AuthState {
  user: AuthUser | null
  refreshToken: string | null
  isAuthenticated: boolean
  login: (refreshToken: string, csrfToken: string, user: AuthUser) => void
  logout: () => void
  updateAfterRefresh: (csrfToken: string, expiresAt: number) => void
  setUser: (user: AuthUser) => void
}

export const useAuthStore = create<AuthState>((set) => ({
  user: null,
  refreshToken: null,
  isAuthenticated: false,

  login: (refreshToken, csrfToken, user) => {
    setCsrfToken(csrfToken)
    set({ user, refreshToken, isAuthenticated: true })
  },

  logout: () => {
    clearCsrfToken()
    set({ user: null, refreshToken: null, isAuthenticated: false })
  },

  updateAfterRefresh: (csrfToken, expiresAt) => {
    setCsrfToken(csrfToken)
    set((state) => ({
      user: state.user ? { ...state.user, expiresAt } : null,
    }))
  },

  setUser: (user) => {
    set({ user, isAuthenticated: true })
  },
}))
