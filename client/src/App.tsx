import { useSessionStore } from '@/stores/sessionStore'

function App() {
  const connectionStatus = useSessionStore((s) => s.connectionStatus)

  return (
    <div data-testid="app-root" data-status={connectionStatus}>
      {/* UI components will be added in issue #506 */}
    </div>
  )
}

export default App
