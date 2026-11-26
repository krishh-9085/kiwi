import React, { useState } from 'react'
import { Globe, RefreshCw } from 'lucide-react'
import { Tooltip } from './Tooltip'
import ThemeToggle from './ThemeToggle'

function formatProviderBadge(p) {
  const k = String(p || '').toLowerCase()
  if (k === 'fish' || k === 'fishaudio') return 'Fish Audio'
  if (k === 'gemini') return 'Gemini'
  if (k === 'groq') return 'Groq'
  if (k === 'sarvam') return 'Sarvam'
  return p
}

export default function ContentHeader({
  status,
  config,
  connected,
  robotHost,
  onSetRobotHost,
  onRefresh,
  theme,
  onToggleTheme
}) {
  const [editingHost, setEditingHost] = useState(false)
  const [inputHost, setInputHost] = useState(robotHost)

  const handleHostSubmit = (e) => {
    e.preventDefault()
    let h = inputHost.trim()
    if (h && !h.startsWith('http://') && !h.startsWith('https://')) {
      h = 'http://' + h
    }
    onSetRobotHost(h)
    setEditingHost(false)
  }

  const apBadge = status?.host || config?.host || (config?.hostname ? `${config.hostname}.local` : null) || config?.apSSID || 'kiwi.local'
  const llmBadge = formatProviderBadge(config?.llm?.provider || config?.llmProvider || status?.llm || 'Gemini')
  const sttBadge = formatProviderBadge(config?.stt?.provider || config?.sttProvider || status?.stt || 'Sarvam')
  const ttsBadge = formatProviderBadge(config?.tts?.provider || config?.ttsProvider || status?.tts || 'Sarvam')

  return (
    <header className="flux-header">
      {/* Title Block */}
      <div className="flux-header-title-block">
        <h1 className="flux-main-title">Kiwi Robot Console</h1>
        <p className="flux-main-subtitle">
          Autonomous companion telemetry, live animatronics & voice controls
        </p>
      </div>

      {/* Right Tools: Live Telemetry Badges & Host Switcher */}
      <div className="flux-header-tools">
        {/* Connection Status Pill */}
        <div className="flux-pill-chip">
          <span style={{
            width: '8px', height: '8px', borderRadius: '50%',
            background: connected ? '#10B981' : '#F43F5E',
            boxShadow: connected ? '0 0 8px rgba(16, 185, 129, 0.6)' : 'none',
            display: 'inline-block'
          }} />
          <span style={{ fontSize: '11px', fontWeight: 800 }}>
            {connected ? 'ONLINE' : 'OFFLINE'}
          </span>
        </div>

        {/* AI & Telemetry Chips */}
        <div className="flux-pill-chip" style={{ color: 'var(--text-muted)' }}>
          <span>LLM:</span>
          <strong style={{ color: 'var(--text-dark)', textTransform: 'capitalize' }}>{llmBadge}</strong>
        </div>

        <div className="flux-pill-chip" style={{ color: 'var(--text-muted)' }}>
          <span>STT:</span>
          <strong style={{ color: 'var(--text-dark)', textTransform: 'capitalize' }}>{sttBadge}</strong>
        </div>

        <div className="flux-pill-chip" style={{ color: 'var(--text-muted)' }}>
          <span>TTS:</span>
          <strong style={{ color: 'var(--text-dark)', textTransform: 'capitalize' }}>{ttsBadge}</strong>
        </div>

        {/* Target Host Switcher */}
        {editingHost ? (
          <form onSubmit={handleHostSubmit} style={{ display: 'flex', gap: '6px' }}>
            <input
              type="text"
              value={inputHost}
              onChange={(e) => setInputHost(e.target.value)}
              placeholder="e.g. kiwi.local"
              style={{
                padding: '6px 12px', fontSize: '12px', width: '150px',
                borderRadius: '999px', border: '1px solid var(--border-light)',
                background: 'var(--card-white)', color: 'var(--text-dark)', outline: 'none'
              }}
              autoFocus
            />
            <button type="submit" className="flux-btn" style={{ padding: '6px 12px', fontSize: '11px' }}>
              Set
            </button>
            <button type="button" onClick={() => setEditingHost(false)} style={{ background: 'transparent', border: 'none', cursor: 'pointer', padding: '4px 6px', color: 'var(--text-muted)' }}>
              ✕
            </button>
          </form>
        ) : (
          <Tooltip title="Target Host">
            <button
              onClick={() => { setInputHost(robotHost); setEditingHost(true) }}
              className="flux-pill-chip"
              style={{ cursor: 'pointer' }}
            >
              <Globe size={13} color="var(--text-muted)" />
              <span>{robotHost || apBadge}</span>
            </button>
          </Tooltip>
        )}

        {/* Animated Lightbulb Theme Toggle */}
        <Tooltip title={theme === 'dark' ? 'Switch to Light Mode' : 'Switch to Dark Mode'}>
          <ThemeToggle
            isDark={theme === 'dark'}
            onToggle={onToggleTheme}
            style={{ width: '34px', height: '34px' }}
          />
        </Tooltip>

        {/* Refresh Telemetry */}
        <Tooltip title="Refresh">
          <button
            onClick={onRefresh}
            className="flux-header-btn"
            aria-label="Refresh Telemetry"
          >
            <RefreshCw size={13} color="var(--text-muted)" />
          </button>
        </Tooltip>
      </div>
    </header>
  )
}
