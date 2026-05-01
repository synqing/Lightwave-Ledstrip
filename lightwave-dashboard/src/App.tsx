import DashboardShell from './components/DashboardShell'
import { V2Provider } from './state/v2'

function App() {
  return (
    <V2Provider>
      <DashboardShell />
    </V2Provider>
  )
}

export default App
