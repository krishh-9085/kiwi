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
      background: 'rgba(5, 7, 12, 0.84)',
      backdropFilter: 'blur(12px)',
      WebkitBackdropFilter: 'blur(12px)',
      display: 'flex',
      alignItems: 'center',
      justifyContent: 'center',
      padding: '20px',
      zIndex: 1000000
    }}>
      <div style={{
        width: '100%',
        maxWidth: '420px',
        padding: '32px 28px',
        textAlign: 'center',
        display: 'flex',
        flexDirection: 'column',
        alignItems: 'center',
        gap: '14px',
        borderRadius: '24px',
        background: 'var(--card-white)',
        color: 'var(--text-dark)',
        border: '1px solid var(--border-light)',
        boxShadow: '0 24px 60px rgba(0, 0, 0, 0.45), 0 0 36px rgba(56, 189, 248, 0.16)'
      }}>
        {reconnected ? (
          <CheckCircle size={48} color="#10B981" />
        ) : (
          <div style={{
            width: '56px', height: '56px', borderRadius: '50%',
            background: 'linear-gradient(135deg, #0284C7, #0F172A)',
            display: 'flex', alignItems: 'center', justifyContent: 'center',
            border: '2px solid rgba(56, 189, 248, 0.5)'
          }}>
            <RefreshCw size={26} color="#38BDF8" className="spin-fast" />
          </div>
        )}

        <h3 style={{ fontSize: '18px', fontWeight: 800, margin: 0, color: 'var(--text-dark)', letterSpacing: '-0.01em' }}>
          {reconnected ? 'Kiwi Reconnected!' : 'Restarting Kiwi...'}
        </h3>

        <p style={{ fontSize: '13px', color: 'var(--text-muted)', lineHeight: 1.5, margin: 0 }}>
          {reconnected
            ? 'Configuration applied successfully. Reloading system console...'
            : (message || 'Applying configuration changes and reconnecting to Wi-Fi. Waiting for robot to come back online...')}
        </p>

        {!reconnected && (
          <span style={{ fontSize: '11px', fontWeight: 600, color: 'var(--text-muted)' }}>
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
