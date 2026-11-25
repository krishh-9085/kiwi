import React, { useState } from 'react'
import { Tooltip } from './Tooltip'
import {
  OLED_FACES,
  SKIN_FACES,
  ALL_42_EMOTIONS,
  ALL_SKINS
} from '../data/faceData'
import {
  Smile, Eye, EyeOff, MessageSquare, Send, StopCircle, Clock, CloudSun,
  Sparkles, Check, Search, RotateCcw
} from 'lucide-react'

// Mini OLED Aperture Screen with CRT scanlines & vector glow - Expands to fill outer container
function FaceAperture({ svgContent, color = '#38BDF8', width = '100%', height = 'auto', style = {} }) {
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
        color: color,
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
            filter: 'drop-shadow(0 0 3.5px currentColor)'
          }}
          dangerouslySetInnerHTML={{ __html: svgContent }}
        />
      ) : null}
    </div>
  )
}

export default function FacesStudioView({ status, onTriggerAction, onToast }) {
  const [bannerText, setBannerText] = useState('')
  const [searchQuery, setSearchQuery] = useState('')
  const [activeScreen, setActiveScreen] = useState(null) // 'clock' | 'weather' | null

  const currentEmotion = status?.emotion || 'idle'
  const currentSkin = status?.skin || 'kiwi'
  const showMouth = status?.mouth ?? true

  // Send OLED Marquee Banner
  const handleSendBanner = async (e) => {
    e.preventDefault()
    const txt = bannerText.trim()
    if (!txt) return
    setActiveScreen(null)
    await onTriggerAction(`/do?banner=${encodeURIComponent(txt)}`)
    onToast(`Scrolling banner: "${txt}" on OLED display!`)
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

  const handleToggleClock = async () => {
    if (activeScreen === 'clock') {
      setActiveScreen(null)
      await onTriggerAction('/do?e=idle&s=1')
      onToast('Clock hidden. Kiwi face restored to normal!')
    } else {
      setActiveScreen('clock')
      await onTriggerAction('/do?e=clock&s=1')
      onToast('Showing Live Clock on Kiwi screen')
    }
  }

  const handleToggleWeather = async () => {
    if (activeScreen === 'weather') {
      setActiveScreen(null)
      await onTriggerAction('/do?e=idle&s=1')
      onToast('Weather hidden. Kiwi face restored to normal!')
    } else {
      setActiveScreen('weather')
      await onTriggerAction('/do?e=weather&s=1')
      onToast('Showing Weather Forecast on Kiwi screen')
    }
  }

  const handleRestoreNormal = async () => {
    setActiveScreen(null)
    await onTriggerAction('/do?e=idle&s=1')
    onToast('Restored normal Kiwi face!')
  }

  const handleTriggerEmotion = async (emo) => {
    setActiveScreen(null)
    await onTriggerAction(`/do?e=${encodeURIComponent(emo.id)}&s=1`)
    onToast(`Emotion triggered: ${emo.title}`)
  }

  const handleSwitchSkin = async (skin) => {
    setActiveScreen(null)
    await onTriggerAction(`/do?skin=${encodeURIComponent(skin.id)}`)
    onToast(`Switched eye skin to ${skin.label}`)
  }

  // Filter emotions by search query
  const filteredEmotions = ALL_42_EMOTIONS.filter((emo) => {
    const q = searchQuery.trim().toLowerCase()
    return !q ||
      emo.title.toLowerCase().includes(q) ||
      emo.desc.toLowerCase().includes(q) ||
      emo.id.toLowerCase().includes(q)
  })

  return (
    <div style={{ display: 'flex', flexDirection: 'column', gap: '24px' }}>
      {/* Title & Quick Actions Header */}
      <div className="flux-title-section" style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'center', flexWrap: 'wrap', gap: '16px' }}>
        <div>
          <h1 className="flux-main-title">Face &amp; Emotion Studio</h1>
        </div>

        {/* Quick Utilities Row */}
        <div
          className="flux-title-actions"
          style={{
            display: 'flex',
            alignItems: 'center',
            gap: '10px',
            width: '100%',
            flexWrap: 'wrap'
          }}
        >
          <Tooltip title={activeScreen === 'clock' ? 'Hide Clock (Restore Normal Face)' : 'Show Live Clock on Kiwi'}>
            <button
              onClick={handleToggleClock}
              className="flux-btn"
              style={{
                flex: 1,
                minWidth: '130px',
                display: 'flex',
                alignItems: 'center',
                justifyContent: 'center',
                gap: '8px',
                padding: '10px 16px',
                fontSize: '13px',
                fontWeight: 700,
                background: activeScreen === 'clock'
                  ? 'linear-gradient(135deg, #0EA5E9 0%, #0284C7 100%)'
                  : 'var(--card-muted)',
                color: activeScreen === 'clock' ? '#FFFFFF' : 'var(--text-dark)',
                border: activeScreen === 'clock' ? '1px solid #38BDF8' : '1px solid var(--border-light)',
                boxShadow: activeScreen === 'clock' ? '0 4px 16px rgba(14, 165, 233, 0.4)' : 'none',
                transition: 'all 0.15s ease'
              }}
            >
              {activeScreen === 'clock' ? <EyeOff size={15} color="#FFFFFF" /> : <Clock size={15} color="#38BDF8" />}
              <span>{activeScreen === 'clock' ? 'Hide Clock' : 'Show Clock'}</span>
            </button>
          </Tooltip>

          <Tooltip title={activeScreen === 'weather' ? 'Hide Weather (Restore Normal Face)' : 'Show Weather Forecast'}>
            <button
              onClick={handleToggleWeather}
              className="flux-btn"
              style={{
                flex: 1,
                minWidth: '130px',
                display: 'flex',
                alignItems: 'center',
                justifyContent: 'center',
                gap: '8px',
                padding: '10px 16px',
                fontSize: '13px',
                fontWeight: 700,
                background: activeScreen === 'weather'
                  ? 'linear-gradient(135deg, #0EA5E9 0%, #0284C7 100%)'
                  : 'var(--card-muted)',
                color: activeScreen === 'weather' ? '#FFFFFF' : 'var(--text-dark)',
                border: activeScreen === 'weather' ? '1px solid #38BDF8' : '1px solid var(--border-light)',
                boxShadow: activeScreen === 'weather' ? '0 4px 16px rgba(14, 165, 233, 0.4)' : 'none',
                transition: 'all 0.15s ease'
              }}
            >
              {activeScreen === 'weather' ? <EyeOff size={15} color="#FFFFFF" /> : <CloudSun size={15} color="#38BDF8" />}
              <span>{activeScreen === 'weather' ? 'Hide Weather' : 'Show Weather'}</span>
            </button>
          </Tooltip>
        </div>
      </div>

      {/* OLED Marquee Banner Generator Card */}
      <div className="flux-white-card">
        <div className="flux-card-header">
          <span className="flux-card-title">
            <MessageSquare size={16} />
            <span>OLED Text Marquee Banner</span>
          </span>
        </div>

        <form onSubmit={handleSendBanner} style={{ display: 'flex', flexDirection: 'column', gap: '10px', width: '100%' }}>
          <input
            type="text"
            value={bannerText}
            onChange={(e) => setBannerText(e.target.value)}
            placeholder="Type marquee banner message (e.g. HELLO KIWI)..."
            style={{
              width: '100%',
              boxSizing: 'border-box',
              background: 'var(--card-muted)',
              border: '1px solid var(--border-light)',
              color: 'var(--text-dark)',
              borderRadius: '999px',
              padding: '12px 18px',
              fontSize: '13px',
              outline: 'none'
            }}
          />
          <div style={{ display: 'grid', gridTemplateColumns: 'repeat(2, 1fr)', gap: '10px', width: '100%' }}>
            <button
              type="submit"
              className="flux-btn"
              style={{
                width: '100%',
                display: 'flex',
                alignItems: 'center',
                justifyContent: 'center',
                gap: '8px',
                padding: '10px 16px',
                background: 'linear-gradient(135deg, #0EA5E9 0%, #0284C7 100%)',
                color: '#FFFFFF',
                border: '1px solid rgba(56, 189, 248, 0.5)',
                boxShadow: '0 4px 14px rgba(14, 165, 233, 0.35)',
                fontWeight: 700
              }}
            >
              <Send size={14} color="#FFFFFF" />
              <span>Scroll Banner</span>
            </button>
            <button
              type="button"
              onClick={handleStopBanner}
              className="flux-btn flux-btn-dark"
              style={{
                width: '100%',
                display: 'flex',
                alignItems: 'center',
                justifyContent: 'center',
                gap: '8px',
                padding: '10px 16px'
              }}
            >
              <StopCircle size={14} />
              <span>Stop</span>
            </button>
          </div>
        </form>
      </div>

      {/* Eye Hardware Skin Silhouettes Card with Exact SVGs */}
      <div className="flux-white-card">
        <div className="flux-card-header" style={{ marginBottom: '16px', display: 'flex', justifyContent: 'space-between', alignItems: 'center', gap: '12px' }}>
          <div style={{ display: 'flex', flexDirection: 'column', gap: '3px' }}>
            <span className="flux-card-title">
              <Eye size={16} />
              <span>Eye &amp; Face Skin Themes</span>
            </span>
            <span style={{ fontSize: '11px', color: 'var(--text-muted)' }}>
              Active: <strong style={{ color: 'var(--text-dark)', textTransform: 'capitalize' }}>{currentSkin}</strong>
            </span>
          </div>

          <div style={{ display: 'flex', alignItems: 'center', gap: '10px', flexShrink: 0 }}>
            <span style={{ fontSize: '12px', fontWeight: 600, color: 'var(--text-muted)' }}>
              Mouth
            </span>
            <Tooltip title={showMouth ? 'Disable OLED animated mouth' : 'Enable OLED animated mouth'}>
              <label className="flux-toggle" style={{ margin: 0, cursor: 'pointer' }}>
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

        <div style={{
          display: 'grid',
          gridTemplateColumns: 'repeat(auto-fill, minmax(135px, 1fr))',
          gap: '12px'
        }}>
          {ALL_SKINS.map((s) => {
            const isSkinActive = currentSkin.toLowerCase() === s.id.toLowerCase()
            const svgMarkup = SKIN_FACES[s.id] || ''

            return (
              <button
                key={s.id}
                onClick={() => handleSwitchSkin(s)}
                style={{
                  padding: '8px 8px 10px',
                  borderRadius: '16px',
                  background: isSkinActive ? 'rgba(14, 165, 233, 0.12)' : 'var(--card-white)',
                  border: isSkinActive ? '2px solid #38BDF8' : '1px solid var(--border-light)',
                  boxShadow: isSkinActive ? '0 0 16px rgba(56, 189, 248, 0.35)' : 'none',
                  display: 'flex',
                  flexDirection: 'column',
                  alignItems: 'center',
                  gap: '8px',
                  cursor: 'pointer',
                  transition: 'transform 0.15s, box-shadow 0.15s, border-color 0.15s',
                  width: '100%',
                  boxSizing: 'border-box'
                }}
                onMouseEnter={(e) => {
                  if (!isSkinActive) {
                    e.currentTarget.style.transform = 'translateY(-2px)'
                    e.currentTarget.style.borderColor = 'var(--border-dark)'
                  }
                }}
                onMouseLeave={(e) => {
                  if (!isSkinActive) {
                    e.currentTarget.style.transform = 'translateY(0)'
                    e.currentTarget.style.borderColor = 'var(--border-light)'
                  }
                }}
              >
                {/* SVG Silhouette Display Box - Expands to fill full container */}
                <FaceAperture
                  svgContent={svgMarkup}
                  color={isSkinActive ? '#00E5FF' : '#38BDF8'}
                  width="100%"
                />

                <div style={{ textAlign: 'center', width: '100%' }}>
                  <div style={{
                    fontSize: '12px',
                    fontWeight: 700,
                    color: isSkinActive ? 'var(--text-dark)' : 'var(--text-muted)',
                    display: 'flex',
                    alignItems: 'center',
                    justifyContent: 'center',
                    gap: '4px'
                  }}>
                    {s.label}
                    {isSkinActive && <Check size={12} style={{ color: '#38BDF8' }} />}
                  </div>
                </div>
              </button>
            )
          })}
        </div>
      </div>

      {/* Full 42 Expressions Matrix with Exact OLED SVGs */}
      <div className="flux-white-card">
        <div style={{
          display: 'flex',
          justifyContent: 'space-between',
          alignItems: 'center',
          marginBottom: '20px',
          flexWrap: 'wrap',
          gap: '12px'
        }}>
          <span className="flux-card-title">
            <Smile size={16} />
            <span>All 42 Expression Presets</span>
          </span>

          {/* Quick Search */}
          <div style={{
            display: 'flex',
            alignItems: 'center',
            gap: '6px',
            background: 'var(--card-muted)',
            borderRadius: '999px',
            padding: '6px 14px',
            border: '1px solid var(--border-light)'
          }}>
            <Search size={13} style={{ color: 'var(--text-muted)' }} />
            <input
              type="text"
              value={searchQuery}
              onChange={(e) => setSearchQuery(e.target.value)}
              placeholder="Search emotions..."
              style={{
                border: 'none', background: 'transparent', outline: 'none',
                color: 'var(--text-dark)', fontSize: '12px', width: '130px'
              }}
            />
          </div>
        </div>

        {/* Emotion Cards Grid with Exact OLED Aperture SVGs */}
        <div style={{
          display: 'grid',
          gridTemplateColumns: 'repeat(auto-fill, minmax(135px, 1fr))',
          gap: '12px'
        }}>
          {filteredEmotions.map((emo) => {
            const isActive = currentEmotion.toLowerCase() === emo.id.toLowerCase()
            const svgMarkup = OLED_FACES[emo.key] || OLED_FACES[emo.id] || ''

            return (
              <button
                key={`${emo.catId}-${emo.id}-${emo.key}`}
                onClick={() => handleTriggerEmotion(emo)}
                style={{
                  padding: '8px 8px 10px',
                  borderRadius: '16px',
                  background: isActive ? 'rgba(14, 165, 233, 0.12)' : 'var(--card-white)',
                  border: isActive ? '2px solid #38BDF8' : '1px solid var(--border-light)',
                  boxShadow: isActive ? '0 0 16px rgba(56, 189, 248, 0.35)' : 'none',
                  display: 'flex',
                  flexDirection: 'column',
                  alignItems: 'center',
                  gap: '8px',
                  cursor: 'pointer',
                  transition: 'transform 0.15s, box-shadow 0.15s, border-color 0.15s',
                  userSelect: 'none',
                  width: '100%',
                  boxSizing: 'border-box'
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
                {/* 64x32 Miniature OLED Screen - Expands to fill full container */}
                <FaceAperture
                  svgContent={svgMarkup}
                  color={isActive ? '#00E5FF' : '#38BDF8'}
                  width="100%"
                />

                <div style={{ textAlign: 'center', width: '100%' }}>
                  <div style={{
                    fontSize: '12px',
                    fontWeight: 700,
                    color: isActive ? 'var(--text-dark)' : 'var(--text-muted)',
                    display: 'flex',
                    alignItems: 'center',
                    justifyContent: 'center',
                    gap: '4px'
                  }}>
                    <span>{emo.title}</span>
                    {isActive && (
                      <span style={{
                        width: '6px', height: '6px', borderRadius: '50%',
                        background: '#38BDF8', display: 'inline-block'
                      }} />
                    )}
                  </div>
                </div>
              </button>
            )
          })}
        </div>

        {filteredEmotions.length === 0 && (
          <div style={{ textAlign: 'center', padding: '36px 12px', color: 'var(--text-muted)', fontSize: '13px' }}>
            No emotions found matching "{searchQuery}".
          </div>
        )}
      </div>
    </div>
  )
}
