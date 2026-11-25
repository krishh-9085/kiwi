import React, { useState, useEffect, useCallback } from 'react'
import { flushSync } from 'react-dom'
import Sidebar from './components/Sidebar'
import ContentHeader from './components/ContentHeader'
import LiveConsoleView from './components/LiveConsoleView'
import FacesStudioView from './components/FacesStudioView'
import SettingsView from './components/SettingsView'
import OtaUpdateView from './components/OtaUpdateView'
import RebootOverlay from './components/RebootOverlay'
import MobileBottomDock from './components/MobileBottomDock'
import Toast from './components/Toast'
import KiwiLogo from './components/KiwiLogo'
import ThemeToggleButton4 from './components/ThemeToggleButton4'
import { RotateCw } from 'lucide-react'

export default function App() {
  const [theme, setTheme] = useState(() => {
    const saved = localStorage.getItem('kiwi_theme')
    if (saved === 'dark' || saved === 'light') return saved
    return (typeof window !== 'undefined' && window.matchMedia && window.matchMedia('(prefers-color-scheme: dark)').matches) ? 'dark' : 'light'
  })

  useEffect(() => {
    localStorage.setItem('kiwi_theme', theme)
    const metaTheme = document.querySelector('meta[name="theme-color"]')
    const color = theme === 'dark' ? '#0D0F14' : '#F5F6F9'
    if (metaTheme) {
      metaTheme.setAttribute('content', color)
    }
    if (theme === 'dark') {
      document.documentElement.classList.add('dark')
      document.documentElement.setAttribute('data-theme', 'dark')
    } else {
      document.documentElement.classList.remove('dark')
      document.documentElement.setAttribute('data-theme', 'light')
    }
  }, [theme])

  const toggleTheme = useCallback(() => {
    if (typeof window === 'undefined') return

    const switchTheme = () => {
      setTheme(prev => {
        const next = prev === 'dark' ? 'light' : 'dark'
        if (next === 'dark') {
          document.documentElement.classList.add('dark')
          document.documentElement.setAttribute('data-theme', 'dark')
        } else {
          document.documentElement.classList.remove('dark')
          document.documentElement.setAttribute('data-theme', 'light')
        }
        localStorage.setItem('kiwi_theme', next)
        return next
      })
    }

    if (document.startViewTransition) {
      document.startViewTransition(() => {
        flushSync(() => {
          switchTheme()
        })
      })
    } else {
      switchTheme()
    }
  }, [])

  const [robotHost, setRobotHost] = useState(() => {
    const saved = localStorage.getItem('kiwi_robot_host')
    if (saved) return saved
    if (typeof window !== 'undefined' && (window.location.hostname === 'localhost' || window.location.hostname === '127.0.0.1')) {
      return 'http://192.168.1.16'
    }
    return ''
  })
  const [activeTab, setActiveTab] = useState('live')
  const [status, setStatus] = useState(null)
  const [config, setConfig] = useState(null)
  const [connected, setConnected] = useState(false)
  const [toastMessage, setToastMessage] = useState(null)
  const [rebooting, setRebooting] = useState(false)
  const [rebootMessage, setRebootMessage] = useState('')
  const [localRecording, setLocalRecording] = useState(false)
  const [refreshing, setRefreshing] = useState(false)

  const isMicRecording = localRecording || Boolean(
    status?.busy && (
      status?.emotion === 'listening' ||
      status?.state?.toLowerCase().includes('listen') ||
      status?.state?.toLowerCase().includes('transcrib')
    )
  )

  useEffect(() => {
    const isStillBusyListening = Boolean(
      status?.busy && (
        status?.emotion === 'listening' ||
        status?.state?.toLowerCase().includes('listen') ||
        status?.state?.toLowerCase().includes('transcrib')
      )
    )
    if (!isStillBusyListening) {
      setLocalRecording(false)
    }
  }, [status?.busy, status?.emotion, status?.state])

  const showToast = (msg) => {
    setToastMessage(msg)
    setTimeout(() => setToastMessage(null), 3000)
  }

  const saveRobotHost = (newHost) => {
    setRobotHost(newHost)
    localStorage.setItem('kiwi_robot_host', newHost)
    showToast(`Target set to: ${newHost || 'Local Host'}`)
  }

  const getBaseUrl = useCallback(() => {
    if (!robotHost) return ''
    return robotHost.replace(/\/$/, '')
  }, [robotHost])

  const fetchStatus = useCallback(async () => {
    try {
      const url = getBaseUrl() ? `${getBaseUrl()}/status` : '/status'
      const res = await fetch(url, { signal: AbortSignal.timeout(2500) })
      if (!res.ok) throw new Error('status failed')
      const newStatus = await res.json()
      setStatus(prev => ({
        ...(prev || {}),
        ...newStatus,
        logs: newStatus.logs ?? prev?.logs ?? []
      }))
      setConnected(true)
    } catch {
      if (getBaseUrl()) {
        try {
          const res2 = await fetch('/status', { signal: AbortSignal.timeout(2000) })
          if (res2.ok) {
            const newStatus = await res2.json()
            setStatus(prev => ({ ...(prev || {}), ...newStatus, logs: newStatus.logs ?? prev?.logs ?? [] }))
            setConnected(true)
            return
          }
        } catch {}
      }
      setConnected(false)
    }
  }, [getBaseUrl])

  const fetchLogs = useCallback(async () => {
    try {
      const url = getBaseUrl() ? `${getBaseUrl()}/logs` : '/logs'
      const res = await fetch(url, { signal: AbortSignal.timeout(3000) })
      if (!res.ok) return
      const logData = await res.json()
      if (Array.isArray(logData)) {
        setStatus(prev => prev ? { ...prev, logs: logData } : { logs: logData })
      }
    } catch {
      if (getBaseUrl()) {
        try {
          const res2 = await fetch('/logs', { signal: AbortSignal.timeout(2000) })
          if (res2.ok) {
            const logData = await res2.json()
            if (Array.isArray(logData)) {
              setStatus(prev => prev ? { ...prev, logs: logData } : { logs: logData })
            }
          }
        } catch {}
      }
    }
  }, [getBaseUrl])

  const fetchConfig = useCallback(async () => {
    try {
      const url = getBaseUrl() ? `${getBaseUrl()}/config` : '/config'
      const res = await fetch(url, { signal: AbortSignal.timeout(3000) })
      if (res.ok) setConfig(await res.json())
    } catch (err) {
      if (getBaseUrl()) {
        try {
          const res2 = await fetch('/config', { signal: AbortSignal.timeout(2000) })
          if (res2.ok) setConfig(await res2.json())
        } catch {}
      }
    }
  }, [getBaseUrl])

  useEffect(() => {
    fetchStatus()
    fetchConfig()
    if (activeTab === 'live') {
      fetchLogs()
    }
    const statusTimer = setInterval(fetchStatus, 1500)
    let logsTimer = null
    if (activeTab === 'live') {
      logsTimer = setInterval(fetchLogs, 4000)
    }
    return () => {
      clearInterval(statusTimer)
      if (logsTimer) clearInterval(logsTimer)
    }
  }, [fetchStatus, fetchConfig, fetchLogs, activeTab])

  const triggerAction = async (endpointAndQuery) => {
    try {
      const res = await fetch(`${getBaseUrl()}${endpointAndQuery}`, { method: 'POST' })
      const data = await res.json().catch(() => ({}))
      setTimeout(() => {
        fetchStatus()
        fetchConfig()
        if (activeTab === 'live') fetchLogs()
      }, 150)
      return data
    } catch (err) {
      console.warn('Action failed:', err)
      return null
    }
  }

  const handleMicClick = async () => {
    if (isMicRecording) {
      setLocalRecording(false)
      setStatus(prev => prev ? { ...prev, busy: false, emotion: 'idle', state: 'Ready' } : prev)
      await triggerAction('/do?record=0')
      showToast('Listening cancelled')
    } else {
      setLocalRecording(true)
      setStatus(prev => prev ? { ...prev, busy: true, emotion: 'listening', state: 'Listening...' } : prev)
      await triggerAction('/do?record=1')
      showToast('Kiwi is listening...')
    }
  }

  const handleSaveConfig = async (formDataParams) => {
    try {
      const res = await fetch(`${getBaseUrl()}/save`, {
        method: 'POST',
        headers: { 'Content-Type': 'application/x-www-form-urlencoded;charset=UTF-8' },
        body: formDataParams.toString()
      })
      const data = await res.json().catch(() => ({}))
      if (data?.reboot) {
        setRebootMessage(data.message || 'Settings saved. Kiwi is restarting...')
        setRebooting(true)
      } else {
        showToast('Settings saved successfully!')
        fetchConfig()
      }
    } catch {
      setRebootMessage('Settings sent! Kiwi is restarting to apply changes...')
      setRebooting(true)
    }
  }

  const handleRefreshTelemetry = async () => {
    setRefreshing(true)
    showToast('Telemetry refreshed!')
    try {
      const tasks = [fetchStatus(), fetchConfig()]
      if (activeTab === 'live') tasks.push(fetchLogs())
      await Promise.allSettled(tasks)
    } finally {
      setTimeout(() => setRefreshing(false), 600)
    }
  }

  return (
    <>
      <div className={`flux-app-window ${theme}`} data-theme={theme}>
        {/* Left Dark Charcoal Sidebar matching reference */}
        <Sidebar
          activeTab={activeTab}
          onTabChange={setActiveTab}
          status={status}
          connected={connected}
          onTriggerAction={triggerAction}
        />

        {/* Right Workspace Canvas */}
        <div className="flux-workspace">
          {/* Top Header matching reference */}
          <ContentHeader
            status={status}
            config={config}
            connected={connected}
            robotHost={robotHost}
            onSetRobotHost={saveRobotHost}
            onRefresh={handleRefreshTelemetry}
            theme={theme}
            onToggleTheme={toggleTheme}
          />

          {/* Mobile Top Profile Bar (Global across all tabs: Live, Faces, Settings, OTA) */}
          <div className="mobile-top-bar">
            <div className="mobile-user-pill">
              <div className="mobile-avatar">
                <KiwiLogo size={28} />
              </div>
              <span className="mobile-user-name">Kiwi</span>
            </div>

            <div style={{ display: 'flex', alignItems: 'center', gap: '8px' }}>
              {/* Theme Toggle Button 4 (Lightbulb animation) just left of refresh button on Mobile */}
              <ThemeToggleButton4
                isDark={theme === 'dark'}
                onToggle={toggleTheme}
                mobile
              />

              <button
                onClick={handleRefreshTelemetry}
                className="mobile-icon-circle-btn"
                aria-label="Refresh Telemetry"
              >
                <RotateCw size={17} color="var(--text-dark)" className={refreshing ? 'spin-anim' : ''} />
              </button>
            </div>
          </div>

          {/* Active View Switcher */}
          <div
            key={activeTab}
            className="flux-tab-enter"
            style={{ width: '100%', flex: 1, display: 'flex', flexDirection: 'column' }}
          >
              {activeTab === 'live' && (
                <LiveConsoleView
                  status={status}
                  config={config}
                  connected={connected}
                  onTriggerAction={triggerAction}
                  onToast={showToast}
                  onTabChange={setActiveTab}
                  onRefresh={() => { fetchStatus(); fetchConfig(); fetchLogs(); showToast('Telemetry refreshed!') }}
                />
              )}

              {activeTab === 'faces' && (
                <FacesStudioView
                  status={status}
                  onTriggerAction={triggerAction}
                  onToast={showToast}
                />
              )}

              {activeTab === 'settings' && (
                <SettingsView
                  config={config}
                  onSaveConfig={handleSaveConfig}
                  onTriggerAction={triggerAction}
                  onToast={showToast}
                  getBaseUrl={getBaseUrl}
                />
              )}

              {activeTab === 'ota' && (
                <OtaUpdateView
                  status={status}
                  getBaseUrl={getBaseUrl}
                  onToast={showToast}
                />
              )}
          </div>
        </div>
      </div>

      <MobileBottomDock
        activeTab={activeTab}
        onTabChange={setActiveTab}
        isListening={isMicRecording}
        onMicClick={handleMicClick}
      />

      <RebootOverlay open={rebooting} message={rebootMessage} getBaseUrl={getBaseUrl} />
      <Toast toast={toastMessage} onClose={() => setToastMessage(null)} />
    </>
  )
}
