import React, { useEffect, useState } from 'react'
import { RefreshCw, CheckCircle } from 'lucide-react'

function RebootOverlayInner({ message, getBaseUrl }) {
  const [reconnected, setReconnected] = useState(false)
  const [attempts, setAttempts] = useState(0)

  useEffect(() => {
    let pollInterval = null
    let reloadTimer = null
    let cancelled = false

    // Wait 2.5 seconds for ESP32 to restart, then poll /status
    const timer = setTimeout(() => {
      if (cancelled) return
      pollInterval = setInterval(async () => {
        if (cancelled) return
        setAttempts((a) => a + 1)
        try {
          const res = await fetch(`${getBaseUrl()}/status`, { signal: AbortSignal.timeout(1500) })
          if (res.ok && !cancelled) {
            clearInterval(pollInterval)
            pollInterval = null
            setReconnected(true)
            reloadTimer = setTimeout(() => {
              window.location.reload()
            }, 1200)
          }
        } catch {
          // Still rebooting
        }
      }, 1500)
    }, 2500)

    return () => {
      cancelled = true
      clearTimeout(timer)
      if (pollInterval) clearInterval(pollInterval)
      if (reloadTimer) clearTimeout(reloadTimer)
    }
  }, [getBaseUrl])

  return (
    <div style={{
      position: 'fixed',
      top: 0,
      left: 0,
      right: 0,
      bottom: 0,
      background: 'rgba(5, 7, 12, 0.92)',
      backdropFilter: 'blur(12px)',
      display: 'flex',
      alignItems: 'center',
      justifyContent: 'center',
      zIndex: 9999
    }}>
      <div className="glass-panel" style={{
        maxWidth: '440px',
        padding: '36px 30px',
        textAlign: 'center',
        display: 'flex',
        flexDirection: 'column',
        alignItems: 'center',
        gap: '16px',
        border: '1px solid rgba(56, 189, 248, 0.4)',
        boxShadow: '0 0 40px rgba(56, 189, 248, 0.25)'
      }}>
        {reconnected ? (
          <CheckCircle size={48} color="#34d399" />
        ) : (
          <div style={{
            width: '56px', height: '56px', borderRadius: '50%',
            background: 'linear-gradient(135deg, #0284c7, #0f172a)',
            display: 'flex', alignItems: 'center', justifyContent: 'center',
            border: '2px solid rgba(56, 189, 248, 0.5)'
          }}>
            <RefreshCw size={28} color="var(--accent-cyan)" className="pulse-active" />
          </div>
        )}

        <h3 style={{ fontSize: '18px', fontWeight: 700, margin: 0, letterSpacing: '0.5px' }}>
          {reconnected ? 'Kiwi Reconnected!' : 'Restarting Kiwi...'}
        </h3>

        <p style={{ fontSize: '13px', color: 'var(--text-muted)', lineHeight: 1.5, margin: 0 }}>
          {reconnected
            ? 'Configuration applied successfully. Reloading system console...'
            : (message || 'Applying configuration changes and reconnecting to Wi-Fi. Waiting for robot to come back online...')}
        </p>

        {!reconnected && (
          <span style={{ fontSize: '11px', color: 'var(--text-dim)' }}>
            Polling reconnection attempt #{attempts}...
          </span>
        )}
      </div>
    </div>
  )
}

export default function RebootOverlay({ open, message, getBaseUrl }) {
  if (!open) return null
  return <RebootOverlayInner message={message} getBaseUrl={getBaseUrl} />
}
