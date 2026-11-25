import React, { useState, useEffect } from 'react'

export default function KiwiEyes({ emotion = 'idle', skin = 'kiwi', sleeping = false }) {
  const [blink, setBlink] = useState(false)

  // Natural blink loop (every 3.5–6 seconds)
  useEffect(() => {
    if (sleeping || emotion === 'sleeping') return

    const scheduleBlink = () => {
      const nextDelay = 3000 + Math.random() * 3000
      return setTimeout(() => {
        setBlink(true)
        setTimeout(() => setBlink(false), 140)
        timer = scheduleBlink()
      }, nextDelay)
    }

    let timer = scheduleBlink()
    return () => clearTimeout(timer)
  }, [sleeping, emotion])

  const renderEye = (side = 'left') => {
    const isWink    = emotion === 'wink' && side === 'right'
    const isSleeping = sleeping || emotion === 'sleeping'

    if (isSleeping || blink || isWink) {
      return (
        <rect x="10" y="38" width="44" height="6" rx="3"
          fill="#00f0ff" filter="drop-shadow(0 0 8px rgba(0,240,255,0.8))" />
      )
    }

    if (emotion === 'hearts' || emotion === 'lovestruck') {
      return (
        <path
          d="M 32 48 C 20 38, 14 26, 22 18 C 28 12, 32 18, 32 20 C 32 18, 36 12, 42 18 C 50 26, 44 38, 32 48 Z"
          fill="#f43f5e" filter="drop-shadow(0 0 10px rgba(244,63,94,0.9))" />
      )
    }

    if (emotion === 'happy' || emotion === 'excited' || emotion === 'laughing') {
      return (
        <path
          d="M 12 44 C 16 16, 48 16, 52 44 C 44 32, 20 32, 12 44 Z"
          fill="#10b981" filter="drop-shadow(0 0 10px rgba(16,185,129,0.8))" />
      )
    }

    if (emotion === 'angry' || emotion === 'evil') {
      const tilt = side === 'left' ? 'rotate(18 32 35)' : 'rotate(-18 32 35)'
      return (
        <g transform={tilt}>
          <rect x="14" y="18" width="36" height="34" rx="8"
            fill="#f43f5e" filter="drop-shadow(0 0 10px rgba(244,63,94,0.8))" />
        </g>
      )
    }

    if (emotion === 'curious' || emotion === 'surprised' || emotion === 'shocked') {
      return (
        <circle cx="32" cy="34" r="22"
          fill="#00f0ff" filter="drop-shadow(0 0 12px rgba(0,240,255,0.85))" />
      )
    }

    if (emotion === 'sad' || emotion === 'crying') {
      const tilt = side === 'left' ? 'rotate(-12 32 35)' : 'rotate(12 32 35)'
      return (
        <g transform={tilt}>
          <rect x="14" y="20" width="36" height="32" rx="12"
            fill="#60a5fa" filter="drop-shadow(0 0 8px rgba(96,165,250,0.7))" />
        </g>
      )
    }

    // Mecha: Tall rounded rectangular solid outer eye with floating tall rounded rectangular pupil & glint dot
    if (skin === 'mecha') {
      const eyeTilt = side === 'left' ? 'rotate(2 32 34)' : 'rotate(-2 32 34)'
      return (
        <g transform={eyeTilt}>
          {/* Tall Rounded Rectangular Sclera */}
          <rect x="15" y="7" width="34" height="48" rx="8"
            fill="#00f0ff"
            filter="drop-shadow(0 0 12px rgba(0,240,255,0.85))" />
          {/* Floating Tall Rounded Rectangular Pupil */}
          <rect x="25" y="17" width="14" height="28" rx="3" fill="#0b0f19" />
          {/* Catchlight Glint Dot inside pupil */}
          <rect x="32" y="20" width="3.5" height="3.5" rx="1" fill="#00f0ff" opacity="0.9" />
        </g>
      )
    }

    // EMO: Solid rounded rectangle with prominent corner radius, no pupil
    if (skin === 'emo') {
      const eyeTilt = side === 'left' ? 'rotate(1 32 34)' : 'rotate(-1 32 34)'
      return (
        <g transform={eyeTilt}>
          <rect x="14" y="16" width="36" height="36" rx="10"
            fill="#00f0ff"
            filter="drop-shadow(0 0 14px rgba(0,240,255,0.9))" />
        </g>
      )
    }

    // Default / Kiwi / Vector
    const rx      = skin === 'pixel' ? 0 : 12
    const eyeTilt = side === 'left' ? 'rotate(4 32 34)' : 'rotate(-4 32 34)'

    return (
      <g transform={eyeTilt}>
        <rect x="16" y="15" width="32" height="40" rx={rx}
          fill="#00f0ff"
          filter="drop-shadow(0 0 12px rgba(0,240,255,0.8))" />
        <circle cx="38" cy="22" r="4" fill="#ffffff" opacity="0.65" />
      </g>
    )
  }

  return (
    <div className="eye-float-wrapper">
      <div
        className="eye-display-bezel"
        style={{ width: '100%', maxWidth: '320px', height: '140px', margin: '0 auto', display: 'flex', alignItems: 'center', justifyContent: 'center', gap: '32px' }}
      >
        {/* Animated scanline */}
        <div className="eye-scanline" />

        <svg width="64" height="68" viewBox="0 0 64 68" style={{ overflow: 'visible', position: 'relative', zIndex: 2 }}>
          {renderEye('left')}
        </svg>
        <svg width="64" height="68" viewBox="0 0 64 68" style={{ overflow: 'visible', position: 'relative', zIndex: 2 }}>
          {renderEye('right')}
        </svg>
      </div>
    </div>
  )
}
