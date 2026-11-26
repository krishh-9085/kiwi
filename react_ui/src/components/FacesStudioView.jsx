import React, { useState, useMemo, useCallback } from 'react'
import { Tooltip } from './Tooltip'
import {
  OLED_FACES,
  SKIN_FACES,
  ALL_42_EMOTIONS,
  ALL_SKINS,
  EMOTION_CATEGORIES
} from '../data/faceData'
import {
  Smile, EyeOff, MessageSquare, Send, StopCircle, Clock, CloudSun,
  Sparkles, Check, Search, RotateCcw, X
} from 'lucide-react'

const QUICK_BANNERS = ['HELLO KIWI', 'FOCUS MODE', 'COFFEE BREAK', 'GG EZ']

// Mini OLED Aperture Screen with CRT scanlines & vector glow - Expands to fill outer container
const FaceAperture = React.memo(function FaceAperture({ svgContent, color = '#38BDF8', width = '100%', height = 'auto', active = false, style = {} }) {
  const isNumericWidth = typeof width === 'number'
  const isNumericHeight = typeof height === 'number'

  return (
    <div
      style={{
        width: isNumericWidth ? `${width}px` : width,
        height: isNumericHeight ? `${height}px` : height,
        aspectRatio: !isNumericHeight ? '2 / 1' : undefined,
        background: '#07090E',
        borderRadius: '10px',
        boxShadow: 'inset 0 0 14px rgba(0, 0, 0, 0.95)',
        position: 'relative',
        overflow: 'hidden',
        display: 'flex',
        alignItems: 'center',
        justifyContent: 'center',
        color,
        flexShrink: 0,
        boxSizing: 'border-box',
        padding: '2px',
        ...style
      }}
    >
      {/* Scanline CRT overlay */}
      <div
        style={{
          position: 'absolute',
          inset: 0,
          background: 'repeating-linear-gradient(0deg, rgba(255, 255, 255, 0.035) 0px 1px, transparent 1px 3px)',
          pointerEvents: 'none',
          zIndex: 2
        }}
      />
      {svgContent ? (
        <svg
          viewBox="0 0 64 32"
          fill="none"
          xmlns="http://www.w3.org/2000/svg"
          aria-hidden="true"
          preserveAspectRatio="xMidYMid meet"
          style={{
            width: '100%',
            height: '100%',
            display: 'block',
            color: 'inherit',
            filter: active ? 'drop-shadow(0 0 3.5px currentColor)' : 'none'
          }}
          dangerouslySetInnerHTML={{ __html: svgContent }}
        />
      ) : null}
    </div>
  )
})

const PresetTile = React.memo(function PresetTile({
  item,
  label,
  sub,
  isActive,
  svgMarkup,
  onSelect,
  showCheck = false,
  compact = false
}) {
  return (
    <button
      type="button"
      onClick={() => onSelect(item)}
      style={{
        padding: compact ? '6px 6px 8px' : '8px 8px 10px',
        borderRadius: compact ? '14px' : '16px',
        background: isActive ? 'rgba(14, 165, 233, 0.12)' : 'var(--card-subtle)',
        border: isActive ? '1.5px solid #0EA5E9' : '1px solid var(--border-light)',
        boxShadow: isActive ? '0 0 14px rgba(14, 165, 233, 0.22)' : 'none',
        display: 'flex',
        flexDirection: 'column',
        alignItems: 'center',
        gap: compact ? '6px' : '7px',
        cursor: 'pointer',
        transition: 'transform 0.15s, box-shadow 0.15s, border-color 0.15s, background 0.15s',
        userSelect: 'none',
        width: '100%',
        minWidth: 0,
        boxSizing: 'border-box',
        fontFamily: 'inherit'
      }}
      onMouseEnter={(e) => {
        if (!isActive) {
          e.currentTarget.style.transform = 'translateY(-2px)'
          e.currentTarget.style.borderColor = 'var(--border-dark)'
        }
      }}
      onMouseLeave={(e) => {
        if (!isActive) {
          e.currentTarget.style.transform = 'translateY(0)'
          e.currentTarget.style.borderColor = 'var(--border-light)'
        }
      }}
    >
      <FaceAperture
        svgContent={svgMarkup}
        color={isActive ? '#00E5FF' : '#38BDF8'}
        active={isActive}
        width="100%"
      />
      <div style={{ textAlign: 'center', width: '100%', minWidth: 0 }}>
        <div
          style={{
            fontSize: compact ? '11px' : '12px',
            fontWeight: 700,
            color: isActive ? 'var(--text-dark)' : 'var(--text-muted)',
            display: 'flex',
            alignItems: 'center',
            justifyContent: 'center',
            gap: '4px',
            minWidth: 0
          }}
        >
          <span style={{ overflow: 'hidden', textOverflow: 'ellipsis', whiteSpace: 'nowrap' }}>
            {label}
          </span>
          {isActive && (
            showCheck
              ? <Check size={11} style={{ color: '#38BDF8', flexShrink: 0 }} />
              : (
                <span
                  style={{
                    width: '6px',
                    height: '6px',
                    borderRadius: '50%',
                    background: '#38BDF8',
                    flexShrink: 0
                  }}
                />
              )
          )}
        </div>
        {sub && (
          <div
            style={{
              fontSize: '9.5px',
              color: 'var(--text-muted)',
              opacity: 0.78,
              whiteSpace: 'nowrap',
              overflow: 'hidden',
              textOverflow: 'ellipsis',
              marginTop: '1px'
            }}
          >
            {sub}
          </div>
        )}
      </div>
    </button>
  )
})

export default function FacesStudioView({
  status,
  activeScreen: controlledScreen,
  onSetActiveScreen,
  onTriggerAction,
  onToast
}) {
  const [bannerText, setBannerText] = useState('')
  const [searchQuery, setSearchQuery] = useState('')
  const [selectedCategory, setSelectedCategory] = useState('All')
  const [internalScreen, setInternalScreen] = useState(null)

  const activeScreen = controlledScreen !== undefined ? controlledScreen : internalScreen
  const setActiveScreen = useCallback((val) => {
    setInternalScreen(val)
    onSetActiveScreen?.(val)
  }, [onSetActiveScreen])

  const currentEmotion = status?.emotion || 'idle'
  const currentSkin = status?.skin || 'kiwi'
  const showMouth = status?.mouth ?? true
  const activeSkinObj = useMemo(
    () => ALL_SKINS.find((s) => s.id.toLowerCase() === currentSkin.toLowerCase()),
    [currentSkin]
  )

  const sendBannerText = useCallback(async (rawText) => {
    const txt = rawText.trim()
    if (!txt) return
    setActiveScreen(null)
    setBannerText(txt)
    await onTriggerAction(`/do?banner=${encodeURIComponent(txt)}`)
    onToast(`Scrolling "${txt}" on OLED display`)
  }, [onTriggerAction, onToast, setActiveScreen])

  const handleSendBanner = async (e) => {
    e.preventDefault()
    await sendBannerText(bannerText)
  }

  const handleStopBanner = async () => {
    setActiveScreen(null)
    await onTriggerAction('/do?stopbanner=1')
    onToast('OLED banner stopped.')
  }

  const handleToggleMouth = async () => {
    const nextVal = showMouth ? '0' : '1'
    await onTriggerAction(`/do?mouth=${nextVal}`)
    onToast(showMouth ? 'Mouth hidden (speaking only)' : 'Mouth & Lips visible on OLED')
  }

  const handleScreenToggle = async (mode, label, toastMsg) => {
    if (activeScreen === mode) {
      setActiveScreen(null)
      await onTriggerAction('/do?e=idle&s=1')
      onToast(`${label} hidden. Normal face restored!`)
    } else {
      setActiveScreen(mode)
      await onTriggerAction(`/do?e=${mode}&s=1`)
      onToast(toastMsg)
    }
  }

  const handleRestoreNormal = async () => {
    setActiveScreen(null)
    await onTriggerAction('/do?e=idle&s=1')
    onToast('Restored normal Kiwi face!')
  }

  const handleTriggerEmotion = useCallback(async (emo) => {
    setActiveScreen(null)
    await onTriggerAction(`/do?e=${encodeURIComponent(emo.id)}&s=1`)
    onToast(`Emotion triggered: ${emo.title}`)
  }, [onTriggerAction, onToast, setActiveScreen])

  const handleSwitchSkin = useCallback(async (skin) => {
    setActiveScreen(null)
    await onTriggerAction(`/do?skin=${encodeURIComponent(skin.id)}`)
    onToast(`Switched eye skin to ${skin.label}`)
  }, [onTriggerAction, onToast, setActiveScreen])

  const categoryCounts = useMemo(() => {
    const counts = { All: ALL_42_EMOTIONS.length }
    for (const emo of ALL_42_EMOTIONS) {
      counts[emo.category] = (counts[emo.category] || 0) + 1
    }
    return counts
  }, [])

  const filteredEmotions = useMemo(() => {
    const q = searchQuery.trim().toLowerCase()
    return ALL_42_EMOTIONS.filter((emo) => {
      const catOk = selectedCategory === 'All' || emo.category === selectedCategory
      if (!catOk) return false
      if (!q) return true
      return (
        emo.title.toLowerCase().includes(q) ||
        emo.desc.toLowerCase().includes(q) ||
        emo.id.toLowerCase().includes(q)
      )
    })
  }, [searchQuery, selectedCategory])

  const canSendBanner = Boolean(bannerText.trim())

  return (
    <div style={{ display: 'flex', flexDirection: 'column', gap: '18px' }}>
      {/* Title & Display Overlay Modes */}
      <div className="flux-title-section" style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'center', flexWrap: 'wrap', gap: '12px' }}>
        <div>
          <h1 className="flux-main-title">Face &amp; Emotion Studio</h1>
        </div>

        <div
          className="flux-title-actions"
          style={{
            display: 'grid',
            gridTemplateColumns: '1fr 1fr auto',
            gap: '8px',
            width: '100%',
            maxWidth: '460px'
          }}
        >
          <Tooltip title={activeScreen === 'clock' ? 'Hide Clock (Restore Normal Face)' : 'Show Live Clock on Kiwi'}>
            <button
              type="button"
              onClick={() => handleScreenToggle('clock', 'Clock', 'Showing Live Clock on Kiwi screen')}
              className="flux-btn"
              style={{
                display: 'flex',
                alignItems: 'center',
                justifyContent: 'center',
                gap: '7px',
                padding: '9px 12px',
                fontSize: '12.5px',
                fontWeight: 700,
                background: activeScreen === 'clock'
                  ? 'linear-gradient(135deg, #0EA5E9 0%, #0284C7 100%)'
                  : 'var(--card-muted)',
                color: activeScreen === 'clock' ? '#FFFFFF' : 'var(--text-dark)',
                border: activeScreen === 'clock' ? '1px solid #38BDF8' : '1px solid var(--border-light)',
                boxShadow: activeScreen === 'clock' ? '0 4px 14px rgba(14, 165, 233, 0.36)' : 'none',
                transition: 'all 0.15s ease'
              }}
            >
              {activeScreen === 'clock' ? <EyeOff size={14} color="#FFFFFF" /> : <Clock size={14} color="#38BDF8" />}
              <span>{activeScreen === 'clock' ? 'Hide Clock' : 'Clock'}</span>
            </button>
          </Tooltip>

          <Tooltip title={activeScreen === 'weather' ? 'Hide Weather (Restore Normal Face)' : 'Show Weather Forecast'}>
            <button
              type="button"
              onClick={() => handleScreenToggle('weather', 'Weather', 'Showing Weather Forecast on Kiwi screen')}
              className="flux-btn"
              style={{
                display: 'flex',
                alignItems: 'center',
                justifyContent: 'center',
                gap: '7px',
                padding: '9px 12px',
                fontSize: '12.5px',
                fontWeight: 700,
                background: activeScreen === 'weather'
                  ? 'linear-gradient(135deg, #F59E0B 0%, #D97706 100%)'
                  : 'var(--card-muted)',
                color: activeScreen === 'weather' ? '#FFFFFF' : 'var(--text-dark)',
                border: activeScreen === 'weather' ? '1px solid #FBBF24' : '1px solid var(--border-light)',
                boxShadow: activeScreen === 'weather' ? '0 4px 14px rgba(245, 158, 11, 0.34)' : 'none',
                transition: 'all 0.15s ease'
              }}
            >
              {activeScreen === 'weather' ? <EyeOff size={14} color="#FFFFFF" /> : <CloudSun size={14} color="#FBBF24" />}
              <span>{activeScreen === 'weather' ? 'Hide Weather' : 'Weather'}</span>
            </button>
          </Tooltip>

          <Tooltip title="Restore Normal Kiwi Face">
            <button
              type="button"
              onClick={handleRestoreNormal}
              aria-label="Restore normal face"
              className="flux-btn"
              style={{
                display: 'flex',
                alignItems: 'center',
                justifyContent: 'center',
                gap: '5px',
                padding: '9px 12px',
                fontSize: '12px',
                fontWeight: 700,
                background: 'var(--card-muted)',
                color: 'var(--text-muted)',
                border: '1px solid var(--border-light)'
              }}
            >
              <RotateCcw size={13} />
              <span>Reset</span>
            </button>
          </Tooltip>
        </div>
      </div>

      {/* OLED Marquee Banner Generator Card */}
      <div className="flux-white-card" style={{ display: 'flex', flexDirection: 'column', gap: '12px' }}>
        <div className="flux-card-header" style={{ marginBottom: 0 }}>
          <span className="flux-card-title">
            <MessageSquare size={15} style={{ color: '#38BDF8' }} />
            <span>OLED Marquee Banner</span>
          </span>
          <span
            style={{
              fontSize: '9.5px',
              fontWeight: 800,
              letterSpacing: '0.04em',
              color: '#0284C7',
              background: 'rgba(14, 165, 233, 0.1)',
              border: '1px solid rgba(14, 165, 233, 0.25)',
              padding: '2px 8px',
              borderRadius: '999px'
            }}
          >
            128×64 OLED
          </span>
        </div>

        <form
          onSubmit={handleSendBanner}
          style={{
            display: 'flex',
            alignItems: 'center',
            gap: '6px',
            padding: '4px 4px 4px 14px',
            borderRadius: '999px',
            background: 'var(--card-muted)',
            border: '1px solid var(--border-light)'
          }}
        >
          <input
            type="text"
            value={bannerText}
            onChange={(e) => setBannerText(e.target.value)}
            placeholder="Enter banner text..."
            style={{
              flex: 1,
              minWidth: 0,
              height: '36px',
              background: 'transparent',
              border: 'none',
              color: 'var(--text-dark)',
              fontSize: '13px',
              fontFamily: 'inherit',
              outline: 'none'
            }}
          />
          <Tooltip title="Stop scrolling OLED banner">
            <button
              type="button"
              onClick={handleStopBanner}
              aria-label="Stop banner"
              style={{
                width: '34px',
                height: '34px',
                padding: 0,
                borderRadius: '50%',
                display: 'flex',
                alignItems: 'center',
                justifyContent: 'center',
                flexShrink: 0,
                cursor: 'pointer',
                background: 'var(--card-white)',
                color: 'var(--text-muted)',
                border: '1px solid var(--border-light)',
                transition: 'color 0.15s ease, border-color 0.15s ease'
              }}
            >
              <StopCircle size={15} />
            </button>
          </Tooltip>
          <button
            type="submit"
            disabled={!canSendBanner}
            aria-label="Scroll banner"
            style={{
              height: '36px',
              padding: '0 14px',
              borderRadius: '999px',
              display: 'inline-flex',
              alignItems: 'center',
              justifyContent: 'center',
              gap: '6px',
              flexShrink: 0,
              fontSize: '12px',
              fontWeight: 700,
              fontFamily: 'inherit',
              cursor: canSendBanner ? 'pointer' : 'not-allowed',
              background: canSendBanner
                ? 'linear-gradient(135deg, #0EA5E9 0%, #0284C7 100%)'
                : 'var(--card-white)',
              color: canSendBanner ? '#FFFFFF' : 'var(--text-muted)',
              border: canSendBanner ? '1px solid rgba(56, 189, 248, 0.45)' : '1px solid var(--border-light)',
              boxShadow: canSendBanner ? '0 2px 10px rgba(14, 165, 233, 0.28)' : 'none',
              transition: 'all 0.15s ease'
            }}
          >
            <Send size={13} />
            <span>Scroll</span>
          </button>
        </form>

        <div className="flux-chip-row">
          {QUICK_BANNERS.map((txt) => (
            <button
              key={txt}
              type="button"
              onClick={() => sendBannerText(txt)}
              style={{
                padding: '4px 10px',
                borderRadius: '999px',
                background: 'var(--card-subtle)',
                border: '1px solid var(--border-light)',
                color: 'var(--text-muted)',
                fontSize: '10.5px',
                fontWeight: 600,
                fontFamily: 'inherit',
                cursor: 'pointer',
                whiteSpace: 'nowrap',
                flexShrink: 0
              }}
            >
              {txt}
            </button>
          ))}
        </div>
      </div>

      {/* Eye Skins Card (Horizontal swipe strip on mobile, 5-up row on desktop) */}
      <div className="flux-white-card" style={{ display: 'flex', flexDirection: 'column', gap: '14px' }}>
        <div className="flux-card-header" style={{ marginBottom: 0, gap: '10px' }}>
          <div style={{ display: 'flex', alignItems: 'center', gap: '8px', minWidth: 0, flexWrap: 'wrap' }}>
            <span className="flux-card-title">
              <Sparkles size={15} style={{ color: '#0EA5E9' }} />
              <span>Eye Skins</span>
            </span>
            <span
              style={{
                fontSize: '10px',
                fontWeight: 700,
                color: '#0284C7',
                background: 'rgba(14, 165, 233, 0.1)',
                border: '1px solid rgba(14, 165, 233, 0.25)',
                padding: '2px 8px',
                borderRadius: '999px',
                whiteSpace: 'nowrap'
              }}
            >
              {activeSkinObj?.label || currentSkin}
            </span>
          </div>

          <div
            style={{
              display: 'inline-flex',
              alignItems: 'center',
              gap: '8px',
              padding: '4px 6px 4px 10px',
              borderRadius: '999px',
              background: 'var(--card-subtle)',
              border: '1px solid var(--border-light)',
              flexShrink: 0
            }}
          >
            <span style={{ fontSize: '11px', fontWeight: 700, color: 'var(--text-muted)' }}>
              Mouth
            </span>
            <Tooltip title={showMouth ? 'Disable OLED animated mouth' : 'Enable OLED animated mouth'}>
              <label className="flux-toggle flux-toggle-sm" style={{ margin: 0, gap: 0, cursor: 'pointer', '--toggle-accent': '#06B6D4' }}>
                <input
                  type="checkbox"
                  checked={Boolean(showMouth)}
                  onChange={handleToggleMouth}
                />
                <span className="flux-toggle-slider" />
              </label>
            </Tooltip>
          </div>
        </div>

        <div className="flux-skin-strip">
          {ALL_SKINS.map((s) => (
            <PresetTile
              key={s.id}
              item={s}
              label={s.label}
              sub={s.sub}
              isActive={currentSkin.toLowerCase() === s.id.toLowerCase()}
              svgMarkup={SKIN_FACES[s.id] || ''}
              onSelect={handleSwitchSkin}
              showCheck
            />
          ))}
        </div>
      </div>

      {/* Full 42 Expressions Matrix with Category Filters + Search */}
      <div className="flux-white-card" style={{ display: 'flex', flexDirection: 'column', gap: '12px' }}>
        <div
          style={{
            display: 'flex',
            justifyContent: 'space-between',
            alignItems: 'center',
            flexWrap: 'wrap',
            gap: '10px'
          }}
        >
          <div style={{ display: 'flex', alignItems: 'center', gap: '8px' }}>
            <span className="flux-card-title">
              <Smile size={15} style={{ color: '#0EA5E9' }} />
              <span>Expressions</span>
            </span>
            <span
              style={{
                fontSize: '10px',
                fontWeight: 800,
                color: 'var(--text-muted)',
                background: 'var(--card-subtle)',
                border: '1px solid var(--border-light)',
                padding: '2px 7px',
                borderRadius: '999px'
              }}
            >
              {filteredEmotions.length}
            </span>
          </div>

          <div
            style={{
              display: 'flex',
              alignItems: 'center',
              gap: '6px',
              background: 'var(--card-muted)',
              borderRadius: '999px',
              padding: '6px 12px',
              border: '1px solid var(--border-light)',
              flex: '1 1 170px',
              maxWidth: '240px'
            }}
          >
            <Search size={13} style={{ color: 'var(--text-muted)', flexShrink: 0 }} />
            <input
              type="text"
              value={searchQuery}
              onChange={(e) => setSearchQuery(e.target.value)}
              placeholder="Filter presets..."
              style={{
                border: 'none',
                background: 'transparent',
                outline: 'none',
                color: 'var(--text-dark)',
                fontSize: '12px',
                fontFamily: 'inherit',
                flex: 1,
                minWidth: 0
              }}
            />
            {searchQuery && (
              <button
                type="button"
                onClick={() => setSearchQuery('')}
                aria-label="Clear search"
                style={{
                  border: 'none',
                  background: 'transparent',
                  color: 'var(--text-muted)',
                  padding: 0,
                  cursor: 'pointer',
                  display: 'flex',
                  alignItems: 'center'
                }}
              >
                <X size={13} />
              </button>
            )}
          </div>
        </div>

        {/* Category Filter Tabs */}
        <div className="flux-chip-row">
          {EMOTION_CATEGORIES.map((cat) => {
            const activeCat = selectedCategory === cat
            return (
              <button
                key={cat}
                type="button"
                onClick={() => setSelectedCategory(cat)}
                style={{
                  padding: '5px 11px',
                  borderRadius: '999px',
                  fontSize: '11px',
                  fontWeight: activeCat ? 700 : 600,
                  fontFamily: 'inherit',
                  whiteSpace: 'nowrap',
                  flexShrink: 0,
                  cursor: 'pointer',
                  background: activeCat ? 'rgba(14, 165, 233, 0.12)' : 'var(--card-subtle)',
                  color: activeCat ? '#0284C7' : 'var(--text-muted)',
                  border: activeCat ? '1px solid rgba(14, 165, 233, 0.38)' : '1px solid var(--border-light)',
                  transition: 'all 0.14s ease'
                }}
              >
                {cat} ({categoryCounts[cat] || 0})
              </button>
            )
          })}
        </div>

        {/* Emotion Cards Grid */}
        <div className="flux-emotion-grid">
          {filteredEmotions.map((emo) => (
            <PresetTile
              key={`${emo.catId}-${emo.id}-${emo.key}`}
              item={emo}
              label={emo.title}
              isActive={currentEmotion.toLowerCase() === emo.id.toLowerCase()}
              svgMarkup={OLED_FACES[emo.key] || OLED_FACES[emo.id] || ''}
              onSelect={handleTriggerEmotion}
              compact
            />
          ))}
        </div>

        {filteredEmotions.length === 0 && (
          <div style={{ textAlign: 'center', padding: '28px 12px', color: 'var(--text-muted)', fontSize: '12.5px', display: 'flex', flexDirection: 'column', alignItems: 'center', gap: '8px' }}>
            <span>No emotions matching "{searchQuery}".</span>
            <button
              type="button"
              onClick={() => {
                setSearchQuery('')
                setSelectedCategory('All')
              }}
              className="flux-btn"
              style={{
                fontSize: '11.5px',
                padding: '6px 12px',
                background: 'var(--card-muted)',
                border: '1px solid var(--border-light)',
                color: 'var(--text-dark)'
              }}
            >
              Reset filters
            </button>
          </div>
        )}
      </div>
    </div>
  )
}
