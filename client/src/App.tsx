import { useSessionStore } from '@/stores/sessionStore'
import { AuthProvider } from '@/components/auth/AuthProvider'
import { AppLayout } from '@/components/layout/AppLayout'

function App() {
  const connectionStatus = useSessionStore((s) => s.connectionStatus)

  return (
    <AuthProvider>
      <div data-testid="app-root" data-status={connectionStatus}>
        <AppLayout />
      </div>
    </AuthProvider>
  )
}

export default App
