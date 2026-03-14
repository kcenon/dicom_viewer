import { create } from 'zustand'
import { setToken, clearToken } from '@/api/httpClient'
import type { AuthUser } from '@/types/api'

interface AuthState {
  user: AuthUser | null
  refreshToken: string | null
  isAuthenticated: boolean
  login: (token: string, refreshToken: string, user: AuthUser) => void
  logout: () => void
  updateToken: (token: string, user: AuthUser) => void
}

export const useAuthStore = create<AuthState>((set) => ({
  user: null,
  refreshToken: null,
  isAuthenticated: false,

  login: (token, refreshToken, user) => {
    setToken(token)
    set({ user, refreshToken, isAuthenticated: true })
  },

  logout: () => {
    clearToken()
    set({ user: null, refreshToken: null, isAuthenticated: false })
  },

  updateToken: (token, user) => {
    setToken(token)
    set({ user })
  },
}))
