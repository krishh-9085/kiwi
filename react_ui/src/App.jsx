import React, { useState, useEffect, useCallback, useRef } from 'react'
import { flushSync } from 'react-dom'
import Sidebar from './components/Sidebar'
import ContentHeader from './components/ContentHeader'
import HomeView from './components/HomeView'
import FacesStudioView from './components/FacesStudioView'
import SettingsView from './components/SettingsView'
import OtaUpdateView from './components/OtaUpdateView'
import RebootOverlay from './components/RebootOverlay'
import MobileBottomDock from './components/MobileBottomDock'
import Toast from './components/Toast'
import KiwiLogo from './components/KiwiLogo'
import ThemeToggle from './components/ThemeToggle'
import { RotateCw } from 'lucide-react'

class TabErrorBoundary extends React.Component {
  constructor(props) {
    super(props)
    this.state = { error: null }
  }

  static getDerivedStateFromError(error) {
    return { error }
  }

  componentDidCatch(error, info) {
    if (typeof console !== 'undefined' && console.error) {
      console.error('[Kiwi UI] Tab render error:', error, info)
    }
  }

  componentDidUpdate(prevProps) {
    if (prevProps.resetKey !== this.props.resetKey && this.state.error) {
      this.setState({ error: null })
    }
  }

  render() {
    if (this.state.error) {
      return (
        <div
          style={{
            padding: '24px',
            margin: '16px 0',
            borderRadius: '16px',
            border: '1px solid rgba(239, 68, 68, 0.45)',
            background: 'rgba(239, 68, 68, 0.10)',
            color: 'var(--text-primary, #f8fafc)',
          }}
        >
          <div style={{ fontWeight: 700, fontSize: '15px', marginBottom: '6px' }}>
            Something went wrong rendering this tab
          </div>
          <div style={{ fontSize: '13px', opacity: 0.85, marginBottom: '12px', fontFamily: 'monospace' }}>
            {String(this.state.error?.message || this.state.error)}
          </div>
          <button
            type="button"
            className="btn-secondary"
            onClick={() => this.setState({ error: null })}
          >
            Retry
          </button>
        </div>
      )
    }
    return this.props.children
  }
}

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
    if (saved && saved !== 'http://192.168.1.16') return saved
    if (saved === 'http://192.168.1.16') {
      localStorage.removeItem('kiwi_robot_host')
    }
    return ''
  })
  const [activeTab, setActiveTab] = useState('live')
  const [activeOledScreen, setActiveOledScreen] = useState(null) // 'clock' | 'weather' | null
  const [status, setStatus] = useState(null)
  const [config, setConfig] = useState(null)
  const [connected, setConnected] = useState(false)
  const [toastMessage, setToastMessage] = useState(null)
  const [rebooting, setRebooting] = useState(false)
  const [rebootMessage, setRebootMessage] = useState('')
  const [localRecording, setLocalRecording] = useState(false)
  const [refreshing, setRefreshing] = useState(false)
  const toastTimerRef = useRef(null)
  const statusInFlightRef = useRef(false)
  const fastPollUntilRef = useRef(0)
  const pollFastRef = useRef(false)
  const triggerNextPollRef = useRef(null)
  const localRecordingUntilRef = useRef(0)

  const isMicRecording = localRecording || Boolean(
    status?.busy && (
      status?.emotion === 'listening' ||
      status?.state?.toLowerCase().includes('listen') ||
      status?.state?.toLowerCase().includes('transcrib')
    )
  )

  const isFastPoll = Boolean(
    isMicRecording ||
    status?.busy ||
    status?.emotion === 'listening' ||
    status?.emotion === 'speaking' ||
    status?.timer_active
  )

  useEffect(() => {
    pollFastRef.current = isFastPoll
  }, [isFastPoll])

  useEffect(() => () => {
    if (toastTimerRef.current) clearTimeout(toastTimerRef.current)
  }, [])

  useEffect(() => {
    if (Date.now() < localRecordingUntilRef.current) return
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

  const showToast = useCallback((msg) => {
    if (toastTimerRef.current) clearTimeout(toastTimerRef.current)
    setToastMessage(msg)
    toastTimerRef.current = setTimeout(() => {
      setToastMessage(null)
      toastTimerRef.current = null
    }, 3000)
  }, [])

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
    if (statusInFlightRef.current) return
    statusInFlightRef.current = true
    try {
      const url = getBaseUrl() ? `${getBaseUrl()}/status` : '/status'
      const res = await fetch(url, { signal: AbortSignal.timeout(2500) })
      if (!res.ok) throw new Error('status failed')
      const newStatus = await res.json()
      setStatus(prev => ({
        ...(prev || {}),
        ...newStatus
      }))
      setConnected(true)
    } catch {
      if (getBaseUrl()) {
        try {
          const res2 = await fetch('/status', { signal: AbortSignal.timeout(2000) })
          if (res2.ok) {
            const newStatus = await res2.json()
            setStatus(prev => ({ ...(prev || {}), ...newStatus }))
            setConnected(true)
            return
          }
        } catch { }
      }
      setConnected(false)
    } finally {
      statusInFlightRef.current = false
    }
  }, [getBaseUrl])

  const fetchConfig = useCallback(async () => {
    try {
      const url = getBaseUrl() ? `${getBaseUrl()}/config` : '/config'
      const res = await fetch(url, { signal: AbortSignal.timeout(3000) })
      if (res.ok) setConfig(await res.json())
    } catch {
      if (getBaseUrl()) {
        try {
          const res2 = await fetch('/config', { signal: AbortSignal.timeout(2000) })
          if (res2.ok) setConfig(await res2.json())
        } catch { }
      }
    }
  }, [getBaseUrl])

  useEffect(() => {
    if (activeTab === 'settings') {
      fetchConfig()
    }
  }, [activeTab, fetchConfig])

  useEffect(() => {
    fetchConfig()
    let timerId = null
    let cancelled = false

    const scheduleNext = (delayMs) => {
      if (cancelled || document.hidden) return
      if (timerId) clearTimeout(timerId)
      const ms = delayMs ?? ((pollFastRef.current || Date.now() < fastPollUntilRef.current) ? 450 : 2000)
      timerId = setTimeout(tickPoll, ms)
    }

    const tickPoll = async () => {
      if (cancelled || document.hidden) return
      await fetchStatus()
      scheduleNext()
    }

    triggerNextPollRef.current = (delayMs = 35) => scheduleNext(delayMs)

    const handleVisibility = () => {
      if (document.hidden) {
        if (timerId) {
          clearTimeout(timerId)
          timerId = null
        }
      } else {
        scheduleNext(0)
      }
    }

    if (!document.hidden) scheduleNext(0)
    document.addEventListener('visibilitychange', handleVisibility)
    return () => {
      cancelled = true
      triggerNextPollRef.current = null
      if (timerId) clearTimeout(timerId)
      document.removeEventListener('visibilitychange', handleVisibility)
    }
  }, [fetchStatus, fetchConfig])

  const triggerAction = useCallback(async (endpointAndQuery) => {
    try {
      fastPollUntilRef.current = Date.now() + 2500
      const res = await fetch(`${getBaseUrl()}${endpointAndQuery}`, { method: 'POST' })
      const data = await res.json().catch(() => ({}))
      if (data && data.ok !== false) {
        const qIdx = endpointAndQuery.indexOf('?')
        const params = new URLSearchParams(qIdx >= 0 ? endpointAndQuery.slice(qIdx + 1) : '')
        setStatus(prev => {
          const next = { ...(prev || {}) }
          if (data.volume != null) next.volume = data.volume
          if (data.brightness != null) next.brightness = data.brightness
          if (data.night_mode != null) next.nightMode = data.night_mode
          if (data.night_bright != null) next.nightBrightness = data.night_bright
          if (data.mouth != null) next.mouth = data.mouth
          if (data.skin != null) next.skin = data.skin
          if (data.sleeping != null) next.sleeping = data.sleeping
          if (data.boredom != null) next.boredom = data.boredom
          if (data.boredom_mins != null) next.boredom_mins = data.boredom_mins
          if (data.eco_sleep != null) {
            next.power = { ...(next.power || {}), ecoSleep: data.eco_sleep }
          }
          if (data.tz_min != null || data.localTime != null) {
            next.time = {
              ...(next.time || {}),
              ...(data.tz_min != null ? { tzMin: data.tz_min } : {}),
              ...(data.localTime != null ? { localTime: data.localTime } : {})
            }
          }
          if (data.rgb_en != null || data.rgb_bright != null) {
            next.rgbLed = {
              ...(next.rgbLed || {}),
              ...(data.rgb_en != null ? { enabled: data.rgb_en } : {}),
              ...(data.rgb_bright != null ? { brightness: data.rgb_bright } : {})
            }
          }
          if (data.greet_en != null) {
            next.greetings = { ...(next.greetings || {}), enabled: data.greet_en }
          }
          if (data.hunger_en != null) {
            next.hunger_en = data.hunger_en
            if (!data.hunger_en) {
              next.hunger = 'satisfied'
              next.hunger_sec = 0
              next.meal = 'generic'
            }
          }
          if (data.fed === true || data.reconciled === true) {
            next.hunger = 'satisfied'
            next.hunger_sec = 0
            next.meal = 'generic'
            next.emotion = data.fed ? 'hearts' : 'laughing'
          }
          if (data.hunger === true) {
            next.hunger = 'hungry'
            next.hunger_sec = 120
            const hParam = params.get('hunger')
            next.meal = (hParam === 'breakfast' || hParam === 'lunch' || hParam === 'dinner') ? hParam : 'generic'
            next.emotion = 'cute'
          }
          if (data.snd_react != null) {
            next.soundReactive = { ...(next.soundReactive || {}), enabled: data.snd_react }
          }
          if (data.ww_en != null || data.ww_model != null || data.ww_cutoff != null) {
            next.wakeWord = {
              ...(next.wakeWord || {}),
              ...(data.ww_en != null ? { enabled: data.ww_en } : {}),
              ...(data.ww_model != null ? { model: data.ww_model } : {}),
              ...(data.ww_cutoff != null ? { cutoff: data.ww_cutoff } : {})
            }
          }
          if (data.circ_en != null || data.circ_start != null || data.circ_smin != null ||
            data.circ_end != null || data.circ_emin != null || data.startStr != null ||
            data.endStr != null || data.eco_sleep != null || data.skip_tonight != null ||
            data.tempWakeSec != null || data.active != null) {
            next.circadian = {
              ...(next.circadian || {}),
              ...(data.circ_en != null ? { enabled: data.circ_en } : {}),
              ...(data.circ_start != null ? { start: data.circ_start } : {}),
              ...(data.circ_smin != null ? { startMin: data.circ_smin } : {}),
              ...(data.circ_end != null ? { end: data.circ_end } : {}),
              ...(data.circ_emin != null ? { endMin: data.circ_emin } : {}),
              ...(data.startStr != null ? { startStr: data.startStr } : {}),
              ...(data.endStr != null ? { endStr: data.endStr } : {}),
              ...(data.eco_sleep != null ? { ecoSleep: data.eco_sleep } : {}),
              ...(data.skip_tonight != null ? { skipTonight: data.skip_tonight } : {}),
              ...(data.tempWakeSec != null ? { tempWakeSec: data.tempWakeSec } : {}),
              ...(data.active != null ? { active: data.active } : {})
            }
          }
          const eParam = params.get('e')
          if (eParam && eParam !== 'clock' && eParam !== 'weather' && eParam !== 'clear' && eParam !== 'forget') {
            next.emotion = eParam === 'wake' ? 'idle' : eParam
          }
          return next
        })
        if (params.get('banner') != null || params.get('skin') != null) {
          setActiveOledScreen(null)
        } else {
          const eParam = params.get('e')
          if (eParam === 'clock' || eParam === 'weather') {
            setActiveOledScreen(eParam)
          } else if (eParam) {
            setActiveOledScreen(null)
          }
        }
      }
      if (triggerNextPollRef.current) {
        triggerNextPollRef.current(35)
      }
      return data
    } catch (err) {
      console.warn('Action failed:', err)
      return null
    }
  }, [getBaseUrl])

  const handleMicClick = async () => {
    if (isMicRecording) {
      localRecordingUntilRef.current = 0
      setLocalRecording(false)
      setStatus(prev => prev ? { ...prev, busy: false, emotion: 'idle', state: 'Ready' } : prev)
      const res = await triggerAction('/do?record=0')
      if (!res || res.ok === false) {
        showToast('Failed to stop listening — check connection')
        return
      }
      showToast('Listening cancelled')
    } else {
      if (!connected) {
        showToast('Kiwi is offline — cannot start listening')
        return
      }
      localRecordingUntilRef.current = Date.now() + 1800
      setLocalRecording(true)
      setStatus(prev => prev ? { ...prev, busy: true, emotion: 'listening', state: 'Listening...' } : prev)
      const res = await triggerAction('/do?record=1')
      if (!res || res.ok === false) {
        localRecordingUntilRef.current = 0
        setLocalRecording(false)
        setStatus(prev => prev ? { ...prev, busy: false, emotion: 'idle', state: 'Ready' } : prev)
        showToast(res?.reason === 'ota_in_progress' ? 'OTA update in progress' : 'Failed to reach Kiwi')
        return
      }
      showToast('Kiwi is listening...')
    }
  }

  const handleSaveConfig = async (formDataParams, expectsReboot = false) => {
    try {
      const res = await fetch(`${getBaseUrl()}/save`, {
        method: 'POST',
        headers: { 'Content-Type': 'application/x-www-form-urlencoded;charset=UTF-8' },
        body: formDataParams.toString()
      })
      if (!res.ok) {
        throw new Error(`HTTP ${res.status}`)
      }
      const data = await res.json().catch(() => ({}))
      if (data?.reboot) {
        setRebootMessage(data.message || 'Settings saved. Kiwi is restarting...')
        setRebooting(true)
      } else {
        fetchConfig()
      }
    } catch (err) {
      if (expectsReboot) {
        setRebootMessage('Network settings sent! Kiwi is restarting to apply changes...')
        setRebooting(true)
        return
      }
      throw err
    }
  }

  const handleRefreshTelemetry = async () => {
    setRefreshing(true)
    showToast('Telemetry refreshed!')
    try {
      await Promise.allSettled([fetchStatus(), fetchConfig()])
    } finally {
      setTimeout(() => setRefreshing(false), 600)
    }
  }

  useEffect(() => {
    try {
      const url = new URL(window.location.href)
      if (url.searchParams.has('_r')) {
        url.searchParams.delete('_r')
        const clean = url.pathname + url.search + url.hash
        window.history.replaceState(null, '', clean || '/')
      }
    } catch { }
  }, [])

  const handleHardRefresh = useCallback(async () => {
    try {
      if (typeof window !== 'undefined' && 'caches' in window) {
        const keys = await window.caches.keys()
        await Promise.all(keys.map((k) => window.caches.delete(k)))
      }
    } catch { }
    try {
      const url = new URL(window.location.href)
      url.searchParams.set('_r', String(Date.now()))
      window.location.replace(url.toString())
    } catch {
      window.location.reload()
    }
  }, [])

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

          {/* Mobile Top Bar (Global across all tabs: Live, Faces, Settings, OTA) */}
          <div className="mobile-top-bar">
            <button
              type="button"
              onClick={handleHardRefresh}
              className="mobile-user-pill"
              aria-label="Hard Refresh Page"
              title="Tap to hard refresh page"
            >
              <div className="mobile-avatar">
                <KiwiLogo size={26} />
              </div>
              <span className="mobile-user-name">Kiwi</span>
            </button>

            <div className="mobile-header-actions">
              <ThemeToggle
                isDark={theme === 'dark'}
                onToggle={toggleTheme}
                mobile
              />

              <button
                type="button"
                onClick={handleRefreshTelemetry}
                className="mobile-icon-circle-btn"
                aria-label="Refresh Telemetry"
                title="Refresh telemetry"
              >
                <RotateCw size={15} className={refreshing ? 'spin-anim' : ''} />
              </button>
            </div>
          </div>

          {/* Active View Switcher */}
          <div
            key={activeTab}
            className="flux-tab-enter"
            style={{ width: '100%', flex: 1, display: 'flex', flexDirection: 'column' }}
          >
            <TabErrorBoundary resetKey={activeTab}>
              {activeTab === 'live' && (
                <HomeView
                  status={status}
                  config={config}
                  connected={connected}
                  isListening={isMicRecording}
                  onMicToggle={handleMicClick}
                  onTriggerAction={triggerAction}
                  onToast={showToast}
                  onRefresh={handleRefreshTelemetry}
                />
              )}

              {activeTab === 'faces' && (
                <FacesStudioView
                  status={status}
                  activeScreen={activeOledScreen}
                  onSetActiveScreen={setActiveOledScreen}
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
            </TabErrorBoundary>
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
