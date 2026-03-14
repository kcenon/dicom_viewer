import { useSessionStore } from '@/stores/sessionStore'
import { AuthProvider } from '@/components/auth/AuthProvider'

function App() {
  const connectionStatus = useSessionStore((s) => s.connectionStatus)

  return (
    <AuthProvider>
      <div data-testid="app-root" data-status={connectionStatus}>
        {/* Layout components will be added in issue #520 */}
      </div>
    </AuthProvider>
  )
}

export default App
