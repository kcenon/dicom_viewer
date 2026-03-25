// HTTP client with httpOnly cookie auth and CSRF protection

const BASE_URL = '/api'
const CSRF_STORAGE_KEY = 'dicom_viewer_csrf'

export function getCsrfToken(): string | null {
  return sessionStorage.getItem(CSRF_STORAGE_KEY)
}

export function setCsrfToken(token: string): void {
  sessionStorage.setItem(CSRF_STORAGE_KEY, token)
}

export function clearCsrfToken(): void {
  sessionStorage.removeItem(CSRF_STORAGE_KEY)
}

function redirectToLogin(): void {
  clearCsrfToken()
  window.location.href = '/login'
}

function isStateChangingMethod(method: string): boolean {
  return ['POST', 'PUT', 'DELETE', 'PATCH'].includes(method.toUpperCase())
}

async function request<T>(
  path: string,
  options: RequestInit = {}
): Promise<T> {
  const headers = new Headers(options.headers)
  headers.set('Content-Type', 'application/json')

  // Attach CSRF token for state-changing requests
  const method = (options.method ?? 'GET').toUpperCase()
  if (isStateChangingMethod(method)) {
    const csrfToken = getCsrfToken()
    if (csrfToken) {
      headers.set('X-CSRF-Token', csrfToken)
    }
  }

  const response = await fetch(`${BASE_URL}${path}`, {
    ...options,
    headers,
    credentials: 'include',
  })

  if (response.status === 401) {
    redirectToLogin()
    throw new Error('Unauthorized')
  }

  if (!response.ok) {
    const errorBody = await response.json().catch(() => ({}))
    const error = new Error(
      (errorBody as { message?: string }).message ?? `HTTP ${response.status}`
    )
    throw error
  }

  if (response.status === 204) {
    return undefined as T
  }

  const contentType = response.headers.get('Content-Type') ?? ''
  if (contentType.includes('application/json')) {
    return response.json() as Promise<T>
  }

  return response.blob() as Promise<T>
}

export const http = {
  get: <T>(path: string) => request<T>(path, { method: 'GET' }),
  post: <T>(path: string, body?: unknown) =>
    request<T>(path, { method: 'POST', body: JSON.stringify(body) }),
  put: <T>(path: string, body?: unknown) =>
    request<T>(path, { method: 'PUT', body: JSON.stringify(body) }),
  delete: <T>(path: string) => request<T>(path, { method: 'DELETE' }),
}
