import { useCallback, useEffect, useRef, useState } from 'react'
import type { ReactNode } from 'react'
import { useAuthStore } from '@/stores/authStore'
import { http, setCsrfToken } from '@/api/httpClient'
import type { AuthMeResponse, CsrfTokenResponse, RefreshTokenResponse } from '@/types/api'
import { LoginPage } from './LoginPage'
import { SessionTimeoutModal } from './SessionTimeoutModal'

const IDLE_WARNING_MS = 14 * 60 * 1000  // 14 minutes
const IDLE_LOGOUT_MS = 15 * 60 * 1000   // 15 minutes (HIPAA)
const ACTIVITY_THROTTLE_MS = 60 * 1000  // debounce: reset at most once per minute
const TOKEN_REFRESH_BUFFER_MS = 60_000  // refresh 1 minute before expiry

interface Props {
  children: ReactNode
}

export function AuthProvider({ children }: Props) {
  const isAuthenticated = useAuthStore((s) => s.isAuthenticated)
  const user = useAuthStore((s) => s.user)
  const refreshToken = useAuthStore((s) => s.refreshToken)
  const storeLogout = useAuthStore((s) => s.logout)
  const updateAfterRefresh = useAuthStore((s) => s.updateAfterRefresh)
  const setUser = useAuthStore((s) => s.setUser)

  // Server-side logout (revoke token + clear cookie) then clear client state
  const logout = useCallback(() => {
    http.post('/auth/logout').catch(() => {})
    storeLogout()
  }, [storeLogout])

  const [showTimeoutModal, setShowTimeoutModal] = useState(false)
  const [checkingSession, setCheckingSession] = useState(true)

  const idleTimerRef = useRef<ReturnType<typeof setTimeout> | null>(null)
  const logoutTimerRef = useRef<ReturnType<typeof setTimeout> | null>(null)
  const lastActivityRef = useRef<number>(0)

  // Check existing session on mount via /auth/me (cookie-based)
  useEffect(() => {
    let cancelled = false
    http.get<AuthMeResponse>('/auth/me')
      .then(async (res) => {
        if (cancelled) return
        setUser({ id: res.id, username: res.id, role: res.role, expiresAt: 0 })
        // Fetch CSRF token for this tab (sessionStorage is per-tab)
        const csrf = await http.get<CsrfTokenResponse>('/auth/csrf-token')
        if (!cancelled) setCsrfToken(csrf.csrfToken)
      })
      .catch(() => {
        // No valid session — stay on login page
      })
      .finally(() => {
        if (!cancelled) setCheckingSession(false)
      })
    return () => { cancelled = true }
  }, [setUser])

  // Reset idle timers on user activity (throttled to once per minute)
  const resetIdleTimers = useCallback(() => {
    const now = Date.now()
    if (now - lastActivityRef.current < ACTIVITY_THROTTLE_MS) return
    lastActivityRef.current = now

    setShowTimeoutModal(false)

    if (idleTimerRef.current !== null) clearTimeout(idleTimerRef.current)
    if (logoutTimerRef.current !== null) clearTimeout(logoutTimerRef.current)

    idleTimerRef.current = setTimeout(() => setShowTimeoutModal(true), IDLE_WARNING_MS)
    logoutTimerRef.current = setTimeout(logout, IDLE_LOGOUT_MS)
  }, [logout])

  // Attach idle detection event listeners when authenticated
  useEffect(() => {
    if (!isAuthenticated) return

    resetIdleTimers()
    window.addEventListener('mousemove', resetIdleTimers)
    window.addEventListener('keydown', resetIdleTimers)

    return () => {
      window.removeEventListener('mousemove', resetIdleTimers)
      window.removeEventListener('keydown', resetIdleTimers)
      if (idleTimerRef.current !== null) clearTimeout(idleTimerRef.current)
      if (logoutTimerRef.current !== null) clearTimeout(logoutTimerRef.current)
    }
  }, [isAuthenticated, resetIdleTimers])

  // Auto-refresh token before it expires
  const expiresAt = user?.expiresAt ?? null
  useEffect(() => {
    if (!isAuthenticated || expiresAt === null || expiresAt === 0 || refreshToken === null) return

    const msUntilExpiry = expiresAt - Date.now()
    if (msUntilExpiry <= 0) {
      logout()
      return
    }

    const refreshAt = Math.max(0, msUntilExpiry - TOKEN_REFRESH_BUFFER_MS)
    const timer = setTimeout(async () => {
      try {
        const res = await http.post<RefreshTokenResponse>('/auth/refresh', { refreshToken })
        updateAfterRefresh(res.csrfToken, res.expiresAt)
      } catch {
        logout()
      }
    }, refreshAt)

    return () => clearTimeout(timer)
  }, [isAuthenticated, expiresAt, refreshToken, logout, updateAfterRefresh])

  // Bypass throttle when user explicitly chooses to stay logged in
  const handleStayLoggedIn = useCallback(() => {
    lastActivityRef.current = 0
    resetIdleTimers()
  }, [resetIdleTimers])

  if (checkingSession) {
    return null
  }

  if (!isAuthenticated) {
    return <LoginPage />
  }

  return (
    <>
      {children}
      {showTimeoutModal && (
        <SessionTimeoutModal
          onStayLoggedIn={handleStayLoggedIn}
          onLogout={logout}
        />
      )}
    </>
  )
}
