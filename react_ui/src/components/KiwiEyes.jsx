import React, { useState, useEffect } from 'react'
import { OLED_FACES } from '../data/faceData'

function emotionColor(emotion, sleeping) {
  if (sleeping || emotion === 'sleeping') return '#00f0ff'
  if (emotion === 'hearts' || emotion === 'lovestruck' || emotion === 'angry' || emotion === 'evil' || emotion === 'error' || emotion === 'dead') {
    return '#f43f5e'
  }
  if (emotion === 'happy' || emotion === 'excited' || emotion === 'laughing' || emotion === 'party' || emotion === 'relieved' || emotion === 'cute') {
    return '#10b981'
  }
  if (emotion === 'sad' || emotion === 'crying') return '#60a5fa'
  if (emotion === 'confused' || emotion === 'smug' || emotion === 'suspicious' || emotion === 'nervous' || emotion === 'scared' || emotion === 'dizzy') {
    return '#fbbf24'
  }
  return '#00f0ff'
}

export default function KiwiEyes({ emotion = 'idle', skin = 'kiwi', sleeping = false }) {
  const [blink, setBlink] = useState(false)

  // Natural blink loop (every 3–6 seconds) with leak-free timer cleanup
  useEffect(() => {
    if (sleeping || emotion === 'sleeping') {
      return
    }

    let cancelled = false
    let nextTimer = null
    let closeTimer = null

    const scheduleBlink = () => {
      const nextDelay = 3000 + Math.random() * 3000
      nextTimer = setTimeout(() => {
        if (cancelled) return
        setBlink(true)
        closeTimer = setTimeout(() => {
          if (cancelled) return
          setBlink(false)
          scheduleBlink()
        }, 140)
      }, nextDelay)
    }

    scheduleBlink()
    return () => {
      cancelled = true
      if (nextTimer) clearTimeout(nextTimer)
      if (closeTimer) clearTimeout(closeTimer)
    }
  }, [sleeping, emotion])

  const isSleeping = sleeping || emotion === 'sleeping'
  const effectiveBlink = !isSleeping && blink
  const glowColor = emotionColor(emotion, sleeping)
  const oledKey = emotion === 'proud' ? 'proud_regal' : emotion
  const usePresetFace = !isSleeping && !effectiveBlink && emotion !== 'idle' && Boolean(OLED_FACES[oledKey])

  const renderSkinEye = (side = 'left') => {
    if (isSleeping || effectiveBlink) {
      return <rect x="10" y="38" width="44" height="6" rx="3" fill={glowColor} />
    }

    // Lumina Spark (squircle_glint): Superellipse squircle + square black pupil + white corner glint
    if (skin === 'squircle_glint') {
      return (
        <g>
          <rect x="12" y="13" width="40" height="42" rx="15" fill={glowColor} />
          <rect x="24" y="25" width="16" height="16" rx="1.5" fill="#0b0f19" />
          <rect x="34.5" y="26.5" width="4" height="4" rx="0.6" fill="#ffffff" />
        </g>
      )
    }

    // Pixel 8-Bit: Stepped retro arcade eye with pixel glint cutout
    if (skin === 'pixel') {
      return (
        <g>
          <rect x="16" y="12" width="32" height="44" fill={glowColor} />
          <rect x="11" y="17" width="42" height="34" fill={glowColor} />
          <rect x="18" y="19" width="10" height="10" fill="#0b0f19" />
        </g>
      )
    }

    // Mecha: Tall rounded rectangular solid outer eye with floating tall rounded rectangular pupil & glint dot
    if (skin === 'mecha') {
      const eyeTilt = side === 'left' ? 'rotate(2 32 34)' : 'rotate(-2 32 34)'
      return (
        <g transform={eyeTilt}>
          <rect x="15" y="7" width="34" height="48" rx="8" fill={glowColor} />
          <rect x="25" y="17" width="14" height="28" rx="3" fill="#0b0f19" />
          <rect x="32" y="20" width="3.5" height="3.5" rx="1" fill={glowColor} opacity="0.9" />
        </g>
      )
    }

    // EMO: Solid rounded rectangle with prominent corner radius, no pupil
    if (skin === 'emo') {
      const eyeTilt = side === 'left' ? 'rotate(1 32 34)' : 'rotate(-1 32 34)'
      return (
        <g transform={eyeTilt}>
          <rect x="14" y="16" width="36" height="36" rx="10" fill={glowColor} />
        </g>
      )
    }

    // Kiwi+: Plush arched Kiwi evolution with bright upper rim, deep rounded U-pupil & dual anime catchlights
    if (skin === 'kiwi_plus') {
      const eyeTilt = side === 'left' ? 'rotate(3.5 32 34)' : 'rotate(-3.5 32 34)'
      return (
        <g transform={eyeTilt}>
          <path d="M14 21 Q14 12 23 12 H41 Q50 12 50 21 V38 C50 50 42 56 32 56 C22 56 14 50 14 38 Z" fill={glowColor} />
          <rect x="21" y="15" width="22" height="27" rx="10" fill="#0b0f19" />
          <circle cx="36" cy="21" r="3.2" fill="#ffffff" opacity="0.96" />
          <circle cx="26.5" cy="34.5" r="1.7" fill="#ffffff" opacity="0.88" />
        </g>
      )
    }

    // Kiwi (Default): Flat upper edge, rounded bottom, U-shaped dark pupil cutout + specular glint
    const eyeTilt = side === 'left' ? 'rotate(3 32 34)' : 'rotate(-3 32 34)'
    return (
      <g transform={eyeTilt}>
        <path d="M16 14 H48 V39 C48 49 41 54 32 54 C23 54 16 49 16 39 Z" fill={glowColor} />
        <path d="M24 14 H40 V31 C40 36 37 39 32 39 C27 39 24 36 24 31 Z" fill="#0b0f19" />
        <rect x="34" y="17" width="4" height="4" rx="1" fill="#ffffff" opacity="0.9" />
      </g>
    )
  }

  return (
    <div className="eye-float-wrapper">
      <div
        className="eye-display-bezel"
        style={{
          width: '100%',
          maxWidth: '320px',
          height: '140px',
          margin: '0 auto',
          display: 'flex',
          alignItems: 'center',
          justifyContent: 'center',
          gap: usePresetFace ? '0px' : '32px',
          color: glowColor
        }}
      >
        {/* Animated scanline */}
        <div className="eye-scanline" />

        {usePresetFace ? (
          <svg
            viewBox="0 0 64 32"
            fill="none"
            style={{
              width: '210px',
              height: '105px',
              position: 'relative',
              zIndex: 2,
              color: glowColor,
              filter: `drop-shadow(0 0 6px ${glowColor})`
            }}
            dangerouslySetInnerHTML={{ __html: OLED_FACES[oledKey] }}
          />
        ) : (
          <div
            style={{
              display: 'flex',
              alignItems: 'center',
              justifyContent: 'center',
              gap: '32px',
              position: 'relative',
              zIndex: 2,
              filter: `drop-shadow(0 0 7px ${glowColor})`
            }}
          >
            <svg width="64" height="68" viewBox="0 0 64 68" style={{ overflow: 'visible' }}>
              {renderSkinEye('left')}
            </svg>
            <svg width="64" height="68" viewBox="0 0 64 68" style={{ overflow: 'visible' }}>
              {renderSkinEye('right')}
            </svg>
          </div>
        )}
      </div>
    </div>
  )
}
