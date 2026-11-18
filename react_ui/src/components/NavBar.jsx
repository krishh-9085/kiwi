import React, { useState } from 'react'
import { Bot, Wifi, BatteryCharging, Battery, Cpu, Activity, Globe, RefreshCw } from 'lucide-react'

export default function NavBar({ status, robotHost, onSetRobotHost, onRefresh, connected }) {
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

  // Derive state colors
  const isBusy = status?.busy
  const isListening = status?.state?.toLowerCase().includes('listen') || status?.emotion === 'listening'
  const isSpeaking = status?.state?.toLowerCase().includes('speak') || status?.emotion === 'speaking'
  const isSleeping = status?.sleeping

  let badgeText = 'IDLE'
  let badgeColor = '#34d399' // emerald
  let badgeGlow = 'rgba(52, 211, 153, 0.4)'

  if (isListening) {
    badgeText = 'LISTENING'
    badgeColor = '#38bdf8' // cyan
    badgeGlow = 'rgba(56, 189, 248, 0.5)'
  } else if (isSpeaking) {
    badgeText = 'SPEAKING'
    badgeColor = '#c084fc' // purple
    badgeGlow = 'rgba(192, 132, 252, 0.5)'
  } else if (isBusy) {
    badgeText = 'THINKING'
    badgeColor = '#fbbf24' // amber
    badgeGlow = 'rgba(251, 191, 36, 0.5)'
  } else if (isSleeping) {
    badgeText = 'SLEEPING'
    badgeColor = '#64748b' // dim gray
    badgeGlow = 'transparent'
  }

  return (
    <header className="glass-panel" style={{ padding: '12px 20px', marginBottom: '20px', display: 'flex', flexWrap: 'wrap', alignItems: 'center', justifyContent: 'space-between', gap: '16px' }}>
      {/* Brand Identity */}
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
            <h1 style={{ fontSize: '16px', fontWeight: 700, letterSpacing: '-0.3px' }}>Kiwi Robot</h1>
            <span style={{ fontSize: '11px', color: '#64748b', background: 'rgba(255,255,255,0.06)', padding: '1px 6px', borderRadius: '4px' }}>
              v{status?.version || '2.0'}
            </span>
          </div>
          <p style={{ fontSize: '11px', color: 'var(--text-muted)' }}>
            Next-Gen Interactive Companion
          </p>
        </div>
      </div>

      {/* Target Host Config */}
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
                borderRadius: '8px',
                padding: '4px 10px',
                color: '#fff',
                fontSize: '12px',
                outline: 'none',
                width: '180px'
              }}
              autoFocus
            />
            <button type="submit" className="btn-action btn-primary" style={{ padding: '4px 10px', fontSize: '11px' }}>
              Save
            </button>
            <button type="button" onClick={() => setEditingHost(false)} className="btn-action" style={{ padding: '4px 8px', fontSize: '11px' }}>
              Cancel
            </button>
          </form>
        ) : (
          <button
            onClick={() => setEditingHost(true)}
            title="Click to change target robot IP / Host"
            className="btn-action"
            style={{ fontSize: '12px', padding: '5px 10px', background: 'rgba(255,255,255,0.03)' }}
          >
            <Globe size={13} color="var(--accent-cyan)" />
            <span>{robotHost || 'kiwi.local'}</span>
            <span style={{
              width: '7px', height: '7px', borderRadius: '50%',
              background: connected ? '#34d399' : '#f43f5e',
              boxShadow: connected ? '0 0 6px #34d399' : '0 0 6px #f43f5e',
              display: 'inline-block'
            }} />
          </button>
        )}

        <button onClick={onRefresh} title="Refresh connection" className="btn-action" style={{ padding: '6px' }}>
          <RefreshCw size={14} color="var(--text-muted)" />
        </button>
      </div>

      {/* Live Telemetry Badges */}
      <div style={{ display: 'flex', alignItems: 'center', gap: '10px', flexWrap: 'wrap' }}>
        {/* State Pill */}
        <div style={{
          display: 'flex', alignItems: 'center', gap: '6px',
          background: 'rgba(0,0,0,0.4)',
          border: `1px solid ${badgeColor}`,
          boxShadow: `0 0 10px ${badgeGlow}`,
          padding: '4px 10px', borderRadius: '999px',
          fontSize: '11px', fontWeight: 700, color: badgeColor
        }}>
          <span style={{ width: '6px', height: '6px', borderRadius: '50%', background: badgeColor, display: 'inline-block' }} className="pulse-active" />
          {badgeText}
        </div>

        {/* Emotion Pill */}
        <div style={{
          background: 'rgba(255,255,255,0.05)',
          padding: '4px 10px', borderRadius: '8px',
          fontSize: '12px', color: 'var(--text-main)',
          display: 'flex', alignItems: 'center', gap: '6px'
        }}>
          <Activity size={13} color="var(--accent-cyan)" />
          <span style={{ textTransform: 'capitalize' }}>{status?.emotion || 'Idle'}</span>
        </div>

        {/* Battery Pill */}
        <div style={{
          background: 'rgba(255,255,255,0.05)',
          padding: '4px 10px', borderRadius: '8px',
          fontSize: '12px', color: 'var(--text-main)',
          display: 'flex', alignItems: 'center', gap: '6px'
        }}>
          <Battery size={14} color="#34d399" />
          <span>{status?.battery?.pct != null ? `${status.battery.pct}%` : 'USB/100%'}</span>
        </div>
      </div>
    </header>
  )
}
