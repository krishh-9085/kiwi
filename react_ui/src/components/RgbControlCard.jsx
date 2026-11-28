import React, { useState, useRef, useCallback, useEffect } from 'react'
import { RotateCcw } from 'lucide-react'

// Live emotion-to-color mapping matching C++ RgbLed hardware
const MOOD_COLOR_MAP = {
  listening: '#00D4FF',   // Electric Cyan
  speaking: '#10B981',    // Vibrant Emerald Green
  thinking: '#A855F7',    // Mystical Deep Violet
  happy: '#F59E0B',       // Warm Sunny Gold
  excited: '#F59E0B',     // Warm Sunny Gold
  party: '#F59E0B',       // Warm Sunny Gold
  petted: '#EC4899',      // Sweet Rose Pink
  hearts: '#EC4899',      // Sweet Rose Pink
  lovestruck: '#EC4899',  // Sweet Rose Pink
  cute: '#EC4899',        // Sweet Rose Pink
  angry: '#EF4444',       // Fiery Crimson Red
  evil: '#EF4444',        // Fiery Crimson Red
  sad: '#3B82F6',         // Ocean Blue
  crying: '#3B82F6',      // Ocean Blue
  confused: '#F97316',    // Warm Amber / Orange
  curious: '#F97316',     // Warm Amber / Orange
  error: '#EF4444',       // Red Alert
  sleeping: '#78350F',    // Cozy Warm Ember
  idle: '#0EA5E9'         // Calming Sky Cyan / Teal
}

// Curated mood presets for instant one-tap LED changes
const PRESET_COLORS = [
  { name: 'Sky Cyan', hex: '#38BDF8' },
  { name: 'Kiwi Lime', hex: '#D2FA4B' },
  { name: 'Emerald', hex: '#10B981' },
  { name: 'Violet', hex: '#A855F7' },
  { name: 'Rose Red', hex: '#F43F5E' },
  { name: 'Warm Amber', hex: '#F59E0B' },
  { name: 'Pure White', hex: '#FFFFFF' }
]

// Pure JS Color Conversion Helpers (Zero dependencies)
function hexToRgb(hex) {
  let c = (hex || '#38BDF8').replace('#', '').trim()
  if (c.length === 3) c = c.split('').map(x => x + x).join('')
  const num = parseInt(c, 16) || 0
  return {
    r: (num >> 16) & 255,
    g: (num >> 8) & 255,
    b: num & 255
  }
}

function rgbToHex(r, g, b) {
  const toHex = (n) => Math.max(0, Math.min(255, Math.round(n))).toString(16).padStart(2, '0')
  return `#${toHex(r)}${toHex(g)}${toHex(b)}`.toUpperCase()
}

function rgbToHsv(r, g, b) {
  r /= 255; g /= 255; b /= 255
  const max = Math.max(r, g, b)
  const min = Math.min(r, g, b)
  const d = max - min
  let h = 0
  const s = max === 0 ? 0 : d / max
  const v = max
  if (d !== 0) {
    if (max === r) h = ((g - b) / d) % 6
    else if (max === g) h = (b - r) / d + 2
    else h = (r - g) / d + 4
    h = Math.round(h * 60)
    if (h < 0) h += 360
  }
  return { h, s, v }
}

function hsvToRgb(h, s, v) {
  h = (h % 360 + 360) % 360
  s = Math.max(0, Math.min(1, s))
  v = Math.max(0, Math.min(1, v))
  const c = v * s
  const x = c * (1 - Math.abs(((h / 60) % 2) - 1))
  const m = v - c
  let r = 0, g = 0, b = 0
  if (h < 60) { r = c; g = x }
  else if (h < 120) { r = x; g = c }
  else if (h < 180) { g = c; b = x }
  else if (h < 240) { g = x; b = c }
  else if (h < 300) { r = x; b = c }
  else { r = c; b = x }
  return {
    r: Math.round((r + m) * 255),
    g: Math.round((g + m) * 255),
    b: Math.round((b + m) * 255)
  }
}

function hexToHsv(hex) {
  const { r, g, b } = hexToRgb(hex)
  return rgbToHsv(r, g, b)
}

export default function RgbControlCard({
  onColorChange,
  onClearOverride,
  onToggleRgb,
  initialColor = '#38BDF8',
  rgbOverride = false,
  currentEmotion = 'idle',
  rgbEnabled = true,
  variant = 'dark',
  title = null,
  showTitle = false,
  showHex = false,
  className = '',
  style = {}
}) {
  const [hsv, setHsv] = useState(() => hexToHsv(initialColor))
  const [isOpen, setIsOpen] = useState(false)
  const [isOverride, setIsOverride] = useState(() => Boolean(rgbOverride))

  const containerRef = useRef(null)
  const popoverRef = useRef(null)
  const areaRef = useRef(null)
  const hueRef = useRef(null)

  const lastSentRef = useRef(0)
  const pendingTimerRef = useRef(null)
  const lastHexRef = useRef('')

  useEffect(() => {
    setIsOverride(Boolean(rgbOverride))
    if (rgbOverride && initialColor && !isOpen) {
      setHsv(hexToHsv(initialColor))
    }
  }, [rgbOverride, initialColor, isOpen])

  const curRgb = hsvToRgb(hsv.h, hsv.s, hsv.v)
  const hexString = rgbToHex(curRgb.r, curRgb.g, curRgb.b)

  const emitColorChange = useCallback((hex) => {
    if (!onColorChange || !hex) return
    const cleanHex = hex.trim().toUpperCase()
    if (cleanHex === lastHexRef.current) return
    lastHexRef.current = cleanHex
    setIsOverride(true)
    onColorChange(cleanHex)
  }, [onColorChange])

  // Throttled live update (fires at most every 70ms during continuous drag, with trailing flush)
  const scheduleColorChange = useCallback((hex) => {
    const now = Date.now()
    const elapsed = now - lastSentRef.current

    if (elapsed >= 70) {
      if (pendingTimerRef.current) {
        clearTimeout(pendingTimerRef.current)
        pendingTimerRef.current = null
      }
      lastSentRef.current = now
      emitColorChange(hex)
    } else {
      if (pendingTimerRef.current) {
        clearTimeout(pendingTimerRef.current)
      }
      pendingTimerRef.current = setTimeout(() => {
        lastSentRef.current = Date.now()
        emitColorChange(hex)
        pendingTimerRef.current = null
      }, 70 - elapsed)
    }
  }, [emitColorChange])

  // Close popover when clicking outside or pressing Escape
  useEffect(() => {
    if (!isOpen) return
    const handleDown = (e) => {
      if (containerRef.current && !containerRef.current.contains(e.target)) {
        setIsOpen(false)
      }
    }
    const handleKey = (e) => {
      if (e.key === 'Escape') setIsOpen(false)
    }
    document.addEventListener('mousedown', handleDown)
    document.addEventListener('touchstart', handleDown)
    document.addEventListener('keydown', handleKey)
    return () => {
      document.removeEventListener('mousedown', handleDown)
      document.removeEventListener('touchstart', handleDown)
      document.removeEventListener('keydown', handleKey)
    }
  }, [isOpen])

  // Clean up pending throttle timers on unmount
  useEffect(() => {
    return () => {
      if (pendingTimerRef.current) {
        clearTimeout(pendingTimerRef.current)
      }
    }
  }, [])

  // 2D Color Area Drag Handling
  const updateAreaFromPointer = useCallback((e) => {
    if (!areaRef.current) return
    const rect = areaRef.current.getBoundingClientRect()
    const clientX = e.clientX ?? (e.touches && e.touches[0]?.clientX) ?? 0
    const clientY = e.clientY ?? (e.touches && e.touches[0]?.clientY) ?? 0

    const s = Math.max(0, Math.min(1, (clientX - rect.left) / rect.width))
    const v = Math.max(0, Math.min(1, 1 - (clientY - rect.top) / rect.height))

    setHsv(prev => {
      const next = { ...prev, s, v }
      const rgb = hsvToRgb(next.h, next.s, next.v)
      scheduleColorChange(rgbToHex(rgb.r, rgb.g, rgb.b))
      return next
    })
  }, [scheduleColorChange])

  const handleAreaPointerDown = (e) => {
    if (e.button != null && e.button !== 0) return
    updateAreaFromPointer(e)

    const onPointerMove = (ev) => {
      ev.preventDefault()
      updateAreaFromPointer(ev)
    }
    const onPointerUp = () => {
      window.removeEventListener('pointermove', onPointerMove)
      window.removeEventListener('pointerup', onPointerUp)
      window.removeEventListener('touchmove', onPointerMove)
      window.removeEventListener('touchend', onPointerUp)
    }
    window.addEventListener('pointermove', onPointerMove, { passive: false })
    window.addEventListener('pointerup', onPointerUp)
    window.addEventListener('touchmove', onPointerMove, { passive: false })
    window.addEventListener('touchend', onPointerUp)
  }

  // Hue Slider Drag Handling
  const updateHueFromPointer = useCallback((e) => {
    if (!hueRef.current) return
    const rect = hueRef.current.getBoundingClientRect()
    const clientX = e.clientX ?? (e.touches && e.touches[0]?.clientX) ?? 0
    const ratio = Math.max(0, Math.min(1, (clientX - rect.left) / rect.width))
    const h = Math.round(ratio * 360)

    setHsv(prev => {
      const next = { ...prev, h }
      const rgb = hsvToRgb(next.h, next.s, next.v)
      scheduleColorChange(rgbToHex(rgb.r, rgb.g, rgb.b))
      return next
    })
  }, [scheduleColorChange])

  const handleHuePointerDown = (e) => {
    if (e.button != null && e.button !== 0) return
    updateHueFromPointer(e)

    const onPointerMove = (ev) => {
      ev.preventDefault()
      updateHueFromPointer(ev)
    }
    const onPointerUp = () => {
      window.removeEventListener('pointermove', onPointerMove)
      window.removeEventListener('pointerup', onPointerUp)
      window.removeEventListener('touchmove', onPointerMove)
      window.removeEventListener('touchend', onPointerUp)
    }
    window.addEventListener('pointermove', onPointerMove, { passive: false })
    window.addEventListener('pointerup', onPointerUp)
    window.addEventListener('touchmove', onPointerMove, { passive: false })
    window.addEventListener('touchend', onPointerUp)
  }

  const handleSelectPreset = (presetHex) => {
    const newHsv = hexToHsv(presetHex)
    setHsv(newHsv)
    if (pendingTimerRef.current) {
      clearTimeout(pendingTimerRef.current)
      pendingTimerRef.current = null
    }
    lastSentRef.current = Date.now()
    emitColorChange(presetHex)
  }

  const handleResetAuto = (e) => {
    if (e && e.stopPropagation) e.stopPropagation()
    if (pendingTimerRef.current) {
      clearTimeout(pendingTimerRef.current)
      pendingTimerRef.current = null
    }
    setIsOverride(false)
    lastHexRef.current = ''
    if (onClearOverride) {
      onClearOverride()
    }
  }

  const isEmbedded = variant === 'embedded'

  // Dynamic mood / selection color resolution
  const moodKey = (currentEmotion || 'idle').toLowerCase()
  const moodHex = MOOD_COLOR_MAP[moodKey] || '#0EA5E9'
  const activeHex = (isOverride ? hexString : moodHex).toUpperCase()

  const { r, g, b } = hexToRgb(activeHex)
  const luminance = 0.299 * r + 0.587 * g + 0.114 * b
  const isUltraLight = luminance > 210

  const cardBgImage = rgbEnabled
    ? `radial-gradient(circle at top right, rgba(${r}, ${g}, ${b}, ${isUltraLight ? 0.14 : 0.24}), transparent 72%)`
    : 'none'
  const cardBorder = rgbEnabled
    ? (isUltraLight ? '1px solid var(--border-dark)' : `1px solid rgba(${r}, ${g}, ${b}, 0.36)`)
    : '1px solid var(--border-light)'
  const badgeBg = !rgbEnabled
    ? 'var(--card-muted)'
    : (isOverride ? 'rgba(56, 189, 248, 0.16)' : (isUltraLight ? 'var(--card-muted)' : `rgba(${r}, ${g}, ${b}, 0.18)`))
  const badgeColor = !rgbEnabled
    ? 'var(--text-muted)'
    : (isOverride ? '#0EA5E9' : (isUltraLight ? 'var(--text-dark)' : activeHex))
  const iconColor = !rgbEnabled
    ? 'var(--text-muted)'
    : (isUltraLight ? '#64748B' : activeHex)

  const emotionName = currentEmotion
    ? `${currentEmotion.charAt(0).toUpperCase() + currentEmotion.slice(1)}`
    : 'Auto'

  return (
    <div
      ref={containerRef}
      className={className}
      style={{
        ...(isEmbedded
          ? {
              display: 'flex',
              flexDirection: 'column',
              position: 'relative',
              width: '100%'
            }
          : {
              background: 'var(--card-white)',
              backgroundImage: cardBgImage,
              borderRadius: '22px',
              padding: '12px 12px',
              color: 'var(--text-dark)',
              display: 'flex',
              flexDirection: 'column',
              justifyContent: 'space-between',
              gap: '8px',
              boxShadow: 'var(--shadow-card)',
              border: cardBorder,
              position: 'relative',
              boxSizing: 'border-box',
              minHeight: '92px',
              transition: 'background-image 0.35s ease, border-color 0.35s ease'
            }),
        ...style
      }}
    >
      {/* Header */}
      <div
        style={{
          display: 'flex',
          alignItems: 'center',
          justifyContent: 'space-between',
          gap: '6px',
          marginBottom: isEmbedded ? '6px' : '0'
        }}
      >
        <div style={{ display: 'flex', alignItems: 'center', gap: '5px', flex: 1, minWidth: 0 }}>
          <svg
            xmlns="http://www.w3.org/2000/svg"
            width="13"
            height="13"
            viewBox="0 0 24 24"
            style={{
              color: iconColor,
              filter: (rgbEnabled && !isUltraLight) ? `drop-shadow(0 0 5px ${activeHex}66)` : 'none',
              display: 'block',
              flexShrink: 0,
              transition: 'color 0.3s ease'
            }}
          >
            <path
              fill="currentColor"
              d="M15 20v-2H9v2c0 1.1.9 2 2 2h2c1.1 0 2-.9 2-2m-6.81-4.92c.13.18.29.53.43.92h6.76c.15-.39.3-.74.43-.92c.35-.5.72-.93 1.08-1.36c1.03-1.21 2.1-2.46 2.1-4.72c0-3.86-3.14-7-7-7s-7 3.14-7 7c0 2.28 1.07 3.53 2.1 4.73c.36.42.73.85 1.09 1.35ZM12 5v2c-1.1 0-2 .9-2 2H8c0-2.21 1.79-4 4-4"
            />
          </svg>
          <span
            style={{
              flex: 1,
              minWidth: 0,
              overflow: 'hidden',
              textOverflow: 'ellipsis',
              whiteSpace: 'nowrap',
              fontSize: '10.5px',
              fontWeight: 800,
              color: 'var(--text-dark)',
              letterSpacing: '0.03em',
              textTransform: (showTitle || title || isEmbedded) ? 'none' : 'uppercase'
            }}
          >
            {(showTitle || title)
              ? (title || 'WS2812 RGB LED')
              : (isEmbedded ? 'Color Customizer' : 'RGB Light')}
          </span>
        </div>

        <div style={{ display: 'flex', alignItems: 'center', gap: '6px', flexShrink: 0 }}>
          {(isEmbedded || !onToggleRgb) && renderModeBadge({ rgbEnabled, isOverride, badgeBg, badgeColor })}
          {onToggleRgb && (
            <label
              className="flux-toggle flux-toggle-sm"
              style={{ gap: 0, flexShrink: 0, '--toggle-accent': activeHex }}
              title={rgbEnabled ? 'Turn off RGB LED' : 'Turn on RGB LED'}
            >
              <input
                type="checkbox"
                checked={rgbEnabled}
                onChange={(e) => onToggleRgb(e.target.checked)}
                aria-label="Toggle RGB LED"
              />
              <span className="flux-toggle-slider" />
            </label>
          )}
        </div>
      </div>

      {/* Lightweight Dependency-Free Color Picker Trigger */}
      <div style={{ position: 'relative', width: '100%' }}>
        <button
          type="button"
          onClick={() => {
            if (!rgbEnabled) {
              onToggleRgb?.(true)
              return
            }
            setIsOpen(!isOpen)
          }}
          style={{
            width: '100%',
            display: 'flex',
            alignItems: 'center',
            justifyContent: 'flex-start',
            gap: '8px',
            background: 'var(--card-muted)',
            border: rgbEnabled
              ? `1px solid rgba(${r}, ${g}, ${b}, 0.28)`
              : '1px solid var(--border-light)',
            borderRadius: isEmbedded ? '10px' : '11px',
            padding: isEmbedded ? '6px 10px' : '7px 9px',
            cursor: (rgbEnabled || onToggleRgb) ? 'pointer' : 'not-allowed',
            opacity: rgbEnabled ? 1 : 0.72,
            transition: 'background 0.2s ease, border-color 0.25s ease',
            textAlign: 'left',
            fontFamily: 'inherit'
          }}
        >
          {/* Swatch circle */}
          <span
            style={{
              width: isEmbedded ? '18px' : '20px',
              height: isEmbedded ? '18px' : '20px',
              borderRadius: '50%',
              backgroundColor: rgbEnabled ? activeHex : '#64748B',
              border: '2px solid rgba(255, 255, 255, 0.75)',
              flexShrink: 0,
              boxShadow: rgbEnabled ? `0 0 10px ${activeHex}66` : 'none',
              display: 'inline-block'
            }}
          />
          <span
            style={{
              fontSize: isEmbedded ? '11.5px' : '11.5px',
              fontWeight: 700,
              color: 'var(--text-dark)',
              whiteSpace: 'nowrap',
              overflow: 'hidden',
              textOverflow: 'ellipsis',
              flex: 1
            }}
          >
            {rgbEnabled ? (isOverride ? 'Change Color' : 'Pick Color') : (onToggleRgb ? 'Tap to Turn On' : 'LED Disabled')}
          </span>
          {(showHex && rgbEnabled) && (
            <span
              style={{
                fontSize: '10px',
                fontFamily: 'monospace',
                fontWeight: 600,
                color: 'var(--text-muted)'
              }}
            >
              {activeHex}
            </span>
          )}
        </button>

        {/* Lightweight Popover Floating Dropdown */}
        {isOpen && rgbEnabled && (
          <div
            ref={popoverRef}
            style={{
              position: 'absolute',
              top: 'calc(100% + 8px)',
              left: 0,
              zIndex: 9999,
              background: 'var(--card-white)',
              borderRadius: '18px',
              padding: '14px',
              boxShadow: '0 20px 40px rgba(0, 0, 0, 0.4)',
              display: 'flex',
              flexDirection: 'column',
              gap: '12px',
              width: '240px',
              border: '1px solid var(--border-light)',
              boxSizing: 'border-box'
            }}
          >
            {/* 2D Area for Saturation & Brightness */}
            <div
              ref={areaRef}
              onPointerDown={handleAreaPointerDown}
              style={{
                width: '100%',
                height: '135px',
                borderRadius: '12px',
                position: 'relative',
                cursor: 'crosshair',
                overflow: 'hidden',
                boxShadow: 'inset 0 0 0 1px rgba(0, 0, 0, 0.1)',
                backgroundColor: `hsl(${hsv.h}, 100%, 50%)`,
                backgroundImage: 'linear-gradient(to top, #000, transparent), linear-gradient(to right, #fff, transparent)',
                touchAction: 'none',
                userSelect: 'none'
              }}
            >
              {/* Thumb marker */}
              <div
                style={{
                  position: 'absolute',
                  left: `${hsv.s * 100}%`,
                  top: `${(1 - hsv.v) * 100}%`,
                  width: '18px',
                  height: '18px',
                  borderRadius: '50%',
                  border: '2px solid #FFFFFF',
                  boxShadow: '0 2px 6px rgba(0, 0, 0, 0.45)',
                  transform: 'translate(-50%, -50%)',
                  pointerEvents: 'none',
                  backgroundColor: hexString
                }}
              />
            </div>

            {/* Linear Hue Rainbow Slider */}
            <div style={{ display: 'flex', flexDirection: 'column', gap: '5px' }}>
              <div style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'center' }}>
                <span style={{ fontSize: '11px', fontWeight: 700, color: 'var(--text-dark, #4B5563)' }}>Hue</span>
                <span style={{ fontSize: '10px', fontFamily: 'monospace', color: 'var(--text-muted, #9CA3AF)' }}>{hsv.h}°</span>
              </div>
              <div
                ref={hueRef}
                onPointerDown={handleHuePointerDown}
                style={{
                  width: '100%',
                  height: '14px',
                  borderRadius: '7px',
                  position: 'relative',
                  cursor: 'pointer',
                  background: 'linear-gradient(to right, #ff0000 0%, #ffff00 17%, #00ff00 33%, #00ffff 50%, #0000ff 67%, #ff00ff 83%, #ff0000 100%)',
                  boxShadow: 'inset 0 0 0 1px rgba(0, 0, 0, 0.15)',
                  touchAction: 'none',
                  userSelect: 'none'
                }}
              >
                {/* Hue thumb */}
                <div
                  style={{
                    position: 'absolute',
                    left: `${(hsv.h / 360) * 100}%`,
                    top: '50%',
                    width: '16px',
                    height: '16px',
                    borderRadius: '50%',
                    border: '2px solid #FFFFFF',
                    boxShadow: '0 2px 5px rgba(0, 0, 0, 0.4)',
                    transform: 'translate(-50%, -50%)',
                    pointerEvents: 'none',
                    backgroundColor: `hsl(${hsv.h}, 100%, 50%)`
                  }}
                />
              </div>
            </div>

            {/* Curated Presets Palette */}
            <div style={{ display: 'flex', flexDirection: 'column', gap: '6px' }}>
              <div style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'center' }}>
                <span style={{ fontSize: '10px', fontWeight: 700, color: 'var(--text-muted, #6B7280)', textTransform: 'uppercase', letterSpacing: '0.04em' }}>
                  Presets
                </span>
                <span style={{ fontSize: '10px', fontFamily: 'monospace', fontWeight: 600, color: 'var(--text-muted, #6B7280)' }}>
                  {hexString}
                </span>
              </div>
              <div style={{ display: 'flex', gap: '6px', alignItems: 'center', justifyContent: 'space-between' }}>
                {PRESET_COLORS.map((p) => {
                  const isActive = hexString.toUpperCase() === p.hex.toUpperCase()
                  return (
                    <button
                      key={p.hex}
                      type="button"
                      aria-label={p.name}
                      title={p.name}
                      onClick={() => handleSelectPreset(p.hex)}
                      style={{
                        width: '22px',
                        height: '22px',
                        borderRadius: '50%',
                        background: p.hex,
                        border: isActive ? '2px solid #16181D' : '1px solid rgba(0, 0, 0, 0.15)',
                        boxShadow: isActive ? '0 0 0 2px var(--accent-lime, #D2FA4B)' : '0 1px 3px rgba(0,0,0,0.08)',
                        cursor: 'pointer',
                        padding: 0,
                        outline: 'none',
                        transition: 'transform 0.1s ease',
                        transform: isActive ? 'scale(1.15)' : 'scale(1)'
                      }}
                    />
                  )
                })}
              </div>
            </div>

            {/* Reset to Auto Button */}
            <button
              type="button"
              onClick={handleResetAuto}
              style={{
                display: 'flex',
                alignItems: 'center',
                justifyContent: 'center',
                gap: '6px',
                width: '100%',
                padding: '7px 0',
                borderRadius: '8px',
                background: 'var(--card-muted)',
                border: '1px solid var(--border-light)',
                fontSize: '11px',
                fontWeight: 700,
                color: 'var(--text-dark)',
                cursor: 'pointer',
                marginTop: '2px',
                fontFamily: 'inherit'
              }}
            >
              <RotateCcw size={12} />
              <span>Reset to Auto Mood</span>
            </button>
          </div>
        )}
      </div>

      {/* Inline Quick Color Swatches + Status Footer */}
      {!isEmbedded && rgbEnabled && (
        <div style={{ display: 'flex', alignItems: 'center', justifyContent: 'space-between', gap: '4px' }}>
          {PRESET_COLORS.slice(0, 6).map((p) => {
            const isSel = isOverride && hexString.toUpperCase() === p.hex.toUpperCase()
            return (
              <button
                key={p.hex}
                type="button"
                aria-label={p.name}
                title={p.name}
                onClick={() => handleSelectPreset(p.hex)}
                style={{
                  width: '16px',
                  height: '16px',
                  borderRadius: '50%',
                  background: p.hex,
                  border: isSel
                    ? `2px solid ${p.hex === '#FFFFFF' ? '#64748B' : '#FFFFFF'}`
                    : `1px solid ${p.hex === '#FFFFFF' ? 'var(--border-dark)' : 'rgba(255, 255, 255, 0.25)'}`,
                  boxShadow: isSel ? `0 0 8px ${p.hex === '#FFFFFF' ? 'rgba(100,116,139,0.55)' : p.hex}` : 'none',
                  cursor: 'pointer',
                  padding: 0,
                  flexShrink: 0,
                  transform: isSel ? 'scale(1.15)' : 'scale(1)',
                  transition: 'transform 0.12s ease'
                }}
              />
            )
          })}
        </div>
      )}

      {/* Quick Action Footer: Auto Mood status & toggle */}
      {(() => {
        const compactFooterMode = !isEmbedded && Boolean(onToggleRgb)
        const footerLabel = !rgbEnabled
          ? (compactFooterMode ? 'LED off' : 'LED turned off')
          : (isOverride
              ? (compactFooterMode ? hexString : `Manual · ${hexString}`)
              : (compactFooterMode ? `${emotionName} mood` : `${emotionName} mood active`))
        return (
          <div
            style={{
              display: 'flex',
              alignItems: 'center',
              justifyContent: 'space-between',
              gap: '6px',
              marginTop: isEmbedded ? '6px' : '0'
            }}
          >
            <div style={{ display: 'flex', alignItems: 'center', gap: '4px', flex: 1, minWidth: 0 }}>
              {compactFooterMode && rgbEnabled && isOverride && renderModeBadge({
                rgbEnabled,
                isOverride,
                badgeBg,
                badgeColor,
                compact: true
              })}
              <span
                style={{
                  fontSize: '9.5px',
                  color: 'var(--text-muted)',
                  fontWeight: 600,
                  whiteSpace: 'nowrap',
                  overflow: 'hidden',
                  textOverflow: 'ellipsis',
                  minWidth: 0
                }}
              >
                {footerLabel}
              </span>
            </div>
            {(rgbEnabled && isOverride) ? (
              <button
                type="button"
                onClick={handleResetAuto}
                style={{
                  background: 'var(--card-muted)',
                  border: '1px solid var(--border-light)',
                  borderRadius: '999px',
                  color: 'var(--text-dark)',
                  fontSize: '8.5px',
                  fontWeight: 700,
                  cursor: 'pointer',
                  display: 'inline-flex',
                  alignItems: 'center',
                  gap: '3px',
                  padding: '2px 5px',
                  flexShrink: 0,
                  fontFamily: 'inherit'
                }}
              >
                <RotateCcw size={9} />
                <span>Auto</span>
              </button>
            ) : (
              compactFooterMode && renderModeBadge({ rgbEnabled, isOverride, badgeBg, badgeColor, compact: true })
            )}
          </div>
        )
      })()}
    </div>
  )
}

function renderModeBadge({ rgbEnabled, isOverride, badgeBg, badgeColor, compact = false }) {
  return (
    <span
      style={{
        fontSize: compact ? '7.5px' : '8.5px',
        fontWeight: 800,
        background: badgeBg,
        color: badgeColor,
        border: '1px solid ' + (rgbEnabled ? `${badgeColor}44` : 'var(--border-light)'),
        padding: compact ? '1px 5px' : '2px 6px',
        borderRadius: '999px',
        letterSpacing: '0.04em',
        lineHeight: 1.35,
        flexShrink: 0
      }}
    >
      {!rgbEnabled ? 'OFF' : (isOverride ? 'CUSTOM' : 'AUTO')}
    </span>
  )
}
