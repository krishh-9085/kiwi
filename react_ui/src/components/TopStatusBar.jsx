import React, { useState } from 'react'
import {
  Bot, Wifi, Battery, BatteryCharging, Cpu, Globe, RefreshCw,
  Sparkles, DownloadCloud, Terminal, Settings, Smile, UploadCloud
} from 'lucide-react'

export default function TopStatusBar({
  status,
  config,
  robotHost,
  onSetRobotHost,
  onRefresh,
  connected,
  activeTab,
  onTabChange,
  onCheckUpdate
}) {
  const [editingHost, setEditingHost] = useState(false)
  const [inputHost, setInputHost] = useState(robotHost)

  const handleHostSubmit = (e) => {
    e.preventDefault()
    let h = inputHost.trim()
    if (!h.startsWith('http://') && !h.startsWith('https://')) {
      h = 'http://' + h
    }
    onSetRobotHost(h)
    setEditingHost(false)
  }

  // Derive model / provider badges
  const llmBadge = config?.llmProvider ? `${config.llmProvider} / ${config.llmModel || 'default'}` : (status?.llm || 'Gemini Flash')
  const sttBadge = config?.sttProvider ? `${config.sttProvider} / ${config.sttLang || 'auto'}` : (status?.stt || 'Sarvam')
  const ttsBadge = config?.ttsProvider ? `${config.ttsProvider} / ${config.ttsVoice || 'voice'}` : (status?.tts || 'Bulbul')
  const apBadge = config?.apSSID || config?.ssid || status?.host || 'kiwi.local'

  const navTabs = [
    { id: 'live', label: 'Live Console', icon: Terminal },
    { id: 'faces', label: 'Faces & Expressions', icon: Smile },
    { id: 'settings', label: 'Settings & AI', icon: Settings },
    { id: 'ota', label: 'OTA Firmware', icon: UploadCloud }
  ]

  return (
    <div style={{ position: 'relative', zIndex: 10, marginBottom: '20px' }}>
      {/* Top Banner Bar */}
      <header className="glass-panel" style={{ padding: '12px 18px', display: 'flex', flexWrap: 'wrap', alignItems: 'center', justifyContent: 'space-between', gap: '14px' }}>
        {/* Brand */}
        <div style={{ display: 'flex', alignItems: 'center', gap: '12px' }}>
          <div style={{
            width: '38px', height: '38px', borderRadius: '10px',
            background: 'linear-gradient(135deg, #0284c7, #0f172a)',
            display: 'flex', alignItems: 'center', justifyContent: 'center',
            border: '1px solid rgba(56, 189, 248, 0.4)',
            boxShadow: '0 0 16px rgba(56, 189, 248, 0.3)'
          }}>
            <Bot size={22} color="#38bdf8" />
          </div>
          <div>
            <div style={{ display: 'flex', alignItems: 'center', gap: '8px' }}>
              <h1 style={{ fontSize: '16px', fontWeight: 700, letterSpacing: '-0.3px', margin: 0 }}>Kiwi Robot</h1>
              <span style={{ fontSize: '11px', color: '#64748b', background: 'rgba(255,255,255,0.06)', padding: '1px 6px', borderRadius: '4px' }}>
                v{status?.version || '2.0'}
              </span>
            </div>
            <p style={{ fontSize: '11px', color: 'var(--text-muted)', margin: 0 }}>
              Next-Gen Autonomous Companion
            </p>
          </div>
        </div>

        {/* System Info Chips from Classic Dashboard */}
        <div style={{ display: 'flex', alignItems: 'center', gap: '8px', flexWrap: 'wrap' }}>
          <div style={{ background: 'rgba(255,255,255,0.04)', padding: '4px 8px', borderRadius: '6px', fontSize: '11px', color: 'var(--text-muted)' }}>
            <span style={{ color: 'var(--text-dim)', marginRight: '4px' }}>AP:</span>
            <strong style={{ color: 'var(--text-main)' }}>{apBadge}</strong>
          </div>
          <div style={{ background: 'rgba(255,255,255,0.04)', padding: '4px 8px', borderRadius: '6px', fontSize: '11px', color: 'var(--text-muted)' }}>
            <span style={{ color: 'var(--text-dim)', marginRight: '4px' }}>LLM:</span>
            <strong style={{ color: 'var(--accent-cyan)' }}>{llmBadge}</strong>
          </div>
          <div style={{ background: 'rgba(255,255,255,0.04)', padding: '4px 8px', borderRadius: '6px', fontSize: '11px', color: 'var(--text-muted)' }}>
            <span style={{ color: 'var(--text-dim)', marginRight: '4px' }}>STT:</span>
            <strong style={{ color: '#34d399' }}>{sttBadge}</strong>
          </div>
          <div style={{ background: 'rgba(255,255,255,0.04)', padding: '4px 8px', borderRadius: '6px', fontSize: '11px', color: 'var(--text-muted)' }}>
            <span style={{ color: 'var(--text-dim)', marginRight: '4px' }}>TTS:</span>
            <strong style={{ color: '#c084fc' }}>{ttsBadge}</strong>
          </div>

          {/* Update Available Check Button */}
          <button
            onClick={onCheckUpdate}
            className="btn-action"
            style={{ padding: '3px 8px', fontSize: '11px', background: 'rgba(56, 189, 248, 0.08)' }}
            title="Check GitHub for OTA updates"
          >
            <DownloadCloud size={12} color="var(--accent-cyan)" />
            <span>Update?</span>
          </button>
        </div>

        {/* Host Switcher & Refresh */}
        <div style={{ display: 'flex', alignItems: 'center', gap: '8px' }}>
          {editingHost ? (
            <form onSubmit={handleHostSubmit} style={{ display: 'flex', gap: '6px' }}>
              <input
                type="text"
                value={inputHost}
                onChange={(e) => setInputHost(e.target.value)}
                placeholder="e.g. kiwi.local or 192.168.4.1"
                style={{
                  background: 'rgba(0,0,0,0.5)',
                  border: '1px solid var(--border-accent)',
                  borderRadius: '6px',
                  padding: '4px 8px',
                  color: '#fff',
                  fontSize: '11px',
                  outline: 'none',
                  width: '150px'
                }}
                autoFocus
              />
              <button type="submit" className="btn-action btn-primary" style={{ padding: '3px 8px', fontSize: '11px' }}>
                Save
              </button>
              <button type="button" onClick={() => setEditingHost(false)} className="btn-action" style={{ padding: '3px 6px', fontSize: '11px' }}>
                ✕
              </button>
            </form>
          ) : (
            <button
              onClick={() => setEditingHost(true)}
              title="Click to change target robot IP / Host"
              className="btn-action"
              style={{ fontSize: '11px', padding: '4px 9px', background: 'rgba(255,255,255,0.03)' }}
            >
              <Globe size={12} color="var(--accent-cyan)" />
              <span>{robotHost || 'kiwi.local'}</span>
              <span style={{
                width: '7px', height: '7px', borderRadius: '50%',
                background: connected ? '#34d399' : '#f43f5e',
                boxShadow: connected ? '0 0 6px #34d399' : '0 0 6px #f43f5e',
                display: 'inline-block'
              }} />
            </button>
          )}

          <button onClick={onRefresh} title="Refresh connection" className="btn-action" style={{ padding: '5px' }}>
            <RefreshCw size={13} color="var(--text-muted)" />
          </button>
        </div>
      </header>

      {/* Primary Navigation Tabs in Navbar */}
      <div style={{
        display: 'grid',
        gridTemplateColumns: 'repeat(auto-fit, minmax(160px, 1fr))',
        gap: '8px',
        marginTop: '14px'
      }}>
        {navTabs.map((t) => {
          const Icon = t.icon
          const isActive = activeTab === t.id
          return (
            <button
              key={t.id}
              onClick={() => onTabChange(t.id)}
              style={{
                padding: '10px 14px',
                borderRadius: '12px',
                fontSize: '13px',
                fontWeight: 600,
                cursor: 'pointer',
                display: 'flex',
                alignItems: 'center',
                justifyContent: 'center',
                gap: '8px',
                transition: 'all 0.2s',
                background: isActive ? 'linear-gradient(135deg, #0284c7, #0369a1)' : 'rgba(16, 21, 30, 0.65)',
                color: isActive ? '#ffffff' : 'var(--text-muted)',
                border: isActive ? '1px solid rgba(56, 189, 248, 0.6)' : '1px solid var(--border-subtle)',
                boxShadow: isActive ? '0 4px 16px rgba(2, 132, 199, 0.35)' : 'none'
              }}
            >
              <Icon size={16} />
              <span>{t.label}</span>
            </button>
          )
        })}
      </div>
    </div>
  )
}
