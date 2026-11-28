import React, { useEffect, useRef, useState, useCallback } from 'react'
import { LayoutDashboard, Smile, Settings, UploadCloud } from 'lucide-react'

const LEFT_TABS = [
  { id: 'live', label: 'Home', ariaLabel: 'Dashboard', icon: LayoutDashboard },
  { id: 'faces', label: 'Faces', ariaLabel: 'Faces & Studio', icon: Smile }
]

const RIGHT_TABS = [
  { id: 'settings', label: 'Settings', ariaLabel: 'Settings', icon: Settings },
  { id: 'ota', label: 'OTA', ariaLabel: 'OTA Firmware', icon: UploadCloud }
]

const ALL_TABS = [...LEFT_TABS, ...RIGHT_TABS]

function DockTabButton({ item, active, onSelect, setButtonRef }) {
  const Icon = item.icon
  return (
    <button
      ref={setButtonRef}
      data-tab-id={item.id}
      type="button"
      onClick={() => onSelect(item.id)}
      className={`mobile-dock-btn ${active ? 'active' : ''}`}
      aria-label={item.ariaLabel}
    >
      <Icon size={18} strokeWidth={active ? 2.3 : 1.9} />
      <span className="mobile-dock-label">{item.label}</span>
    </button>
  )
}

export default function MobileBottomDock({
  activeTab,
  onTabChange,
  isListening,
  onMicClick
}) {
  const navRef = useRef(null)
  const pillRef = useRef(null)
  const btnRefs = useRef({})
  const hasPositionedRef = useRef(false)
  const dragRef = useRef({
    active: false,
    pointerId: null,
    startClientX: 0,
    navInnerLeft: 0,
    grabOffset: 0,
    width: 0,
    minLeft: 0,
    maxLeft: 0,
    metrics: [],
    dragging: false,
    previewTab: null,
    suppressClickUntil: 0
  })

  const [dragPreviewTab, setDragPreviewTab] = useState(null)
  const [isDraggingPill, setIsDraggingPill] = useState(false)

  const applyPillPosition = useCallback((leftPx, widthPx, animate = true, dragging = false) => {
    const pill = pillRef.current
    if (!pill) return
    if (dragging) {
      pill.style.transition = 'opacity 0.14s ease, box-shadow 0.16s ease'
    } else if (!animate) {
      pill.style.transition = 'none'
    } else {
      if (pill.style.transition) {
        pill.style.transition = ''
        void pill.offsetWidth
      } else {
        pill.style.transition = ''
      }
    }
    pill.style.width = `${widthPx}px`
    pill.style.opacity = widthPx > 0 ? '1' : '0'
    pill.style.transform = dragging
      ? `translate3d(${leftPx}px, -50%, 0) scale(1.04)`
      : `translate3d(${leftPx}px, -50%, 0)`
  }, [])

  const getTabMetrics = useCallback(() => {
    const out = []
    for (const t of ALL_TABS) {
      const el = btnRefs.current[t.id]
      if (el && el.offsetWidth > 0) {
        out.push({
          id: t.id,
          left: el.offsetLeft,
          width: el.offsetWidth,
          center: el.offsetLeft + el.offsetWidth / 2
        })
      }
    }
    return out
  }, [])

  const syncPillToTab = useCallback((tabId, animate) => {
    const el = btnRefs.current[tabId]
    if (!el || el.offsetWidth <= 0) {
      hasPositionedRef.current = false
      if (pillRef.current) pillRef.current.style.opacity = '0'
      return
    }
    const shouldAnimate = animate && hasPositionedRef.current
    hasPositionedRef.current = true
    applyPillPosition(el.offsetLeft, el.offsetWidth, shouldAnimate, false)
  }, [applyPillPosition])

  useEffect(() => {
    if (!dragRef.current.dragging) {
      syncPillToTab(activeTab, true)
    }
  }, [activeTab, syncPillToTab])

  useEffect(() => {
    const nav = navRef.current
    if (!nav) return undefined

    const handleLayout = () => {
      if (!dragRef.current.dragging) {
        syncPillToTab(activeTab, false)
      }
    }

    let ro = null
    if (typeof ResizeObserver !== 'undefined') {
      ro = new ResizeObserver(handleLayout)
      ro.observe(nav)
    }
    window.addEventListener('resize', handleLayout)
    return () => {
      if (ro) ro.disconnect()
      window.removeEventListener('resize', handleLayout)
    }
  }, [activeTab, syncPillToTab])

  useEffect(() => {
    const handlePointerMove = (e) => {
      const d = dragRef.current
      if (!d.active || e.pointerId !== d.pointerId) return

      const dx = e.clientX - d.startClientX
      if (!d.dragging) {
        if (Math.abs(dx) < 6) return
        d.dragging = true
        setIsDraggingPill(true)
      }

      const pointerNavX = e.clientX - d.navInnerLeft
      const rawLeft = Math.max(d.minLeft, Math.min(d.maxLeft, pointerNavX - d.grabOffset))
      const pillCenter = rawLeft + d.width / 2

      let nearest = d.metrics[0]
      let bestDist = Infinity
      for (const m of d.metrics) {
        const dist = Math.abs(pillCenter - m.center)
        if (dist < bestDist) {
          bestDist = dist
          nearest = m
        }
      }

      if (!nearest) return
      applyPillPosition(rawLeft, nearest.width, false, true)
      if (nearest.id !== d.previewTab) {
        d.previewTab = nearest.id
        setDragPreviewTab(nearest.id)
        try {
          if (typeof navigator !== 'undefined' && navigator.vibrate) {
            navigator.vibrate(6)
          }
        } catch {
          // ignore if haptics unsupported
        }
      }
    }

    const finishDrag = (e, commit) => {
      const d = dragRef.current
      if (!d.active || e.pointerId !== d.pointerId) return

      const wasDragging = d.dragging
      const targetTab = d.previewTab
      const targetMetric = d.metrics.find((m) => m.id === (commit ? targetTab : activeTab)) ||
        d.metrics.find((m) => m.id === activeTab)

      d.active = false
      d.dragging = false
      d.pointerId = null
      d.previewTab = null
      setIsDraggingPill(false)
      setDragPreviewTab(null)

      if (wasDragging) {
        d.suppressClickUntil = Date.now() + 280
        if (targetMetric) {
          applyPillPosition(targetMetric.left, targetMetric.width, true, false)
        }
        if (commit && targetTab && targetTab !== activeTab) {
          onTabChange(targetTab)
        }
      }
    }

    const handlePointerUp = (e) => finishDrag(e, true)
    const handlePointerCancel = (e) => finishDrag(e, false)

    window.addEventListener('pointermove', handlePointerMove)
    window.addEventListener('pointerup', handlePointerUp)
    window.addEventListener('pointercancel', handlePointerCancel)
    return () => {
      window.removeEventListener('pointermove', handlePointerMove)
      window.removeEventListener('pointerup', handlePointerUp)
      window.removeEventListener('pointercancel', handlePointerCancel)
    }
  }, [activeTab, applyPillPosition, onTabChange])

  const handleDockPointerDown = (e) => {
    if (e.button != null && e.button !== 0) return
    if (e.target.closest?.('.mobile-dock-mic')) return

    const nav = navRef.current
    if (!nav) return
    const metrics = getTabMetrics()
    if (metrics.length < 2) return

    const navRect = nav.getBoundingClientRect()
    const navInnerLeft = navRect.left + (nav.clientLeft || 0)
    const pointerNavX = e.clientX - navInnerLeft
    const pressedBtn = e.target.closest?.('[data-tab-id]')
    const startTabId = pressedBtn?.getAttribute('data-tab-id') || activeTab
    const anchorMetric = metrics.find((m) => m.id === startTabId) || metrics[0]

    dragRef.current = {
      ...dragRef.current,
      active: true,
      pointerId: e.pointerId,
      startClientX: e.clientX,
      navInnerLeft,
      grabOffset: pointerNavX - anchorMetric.left,
      width: anchorMetric.width,
      minLeft: metrics[0].left,
      maxLeft: metrics[metrics.length - 1].left,
      metrics,
      dragging: false,
      previewTab: startTabId
    }
  }

  const handleSelectTab = (tabId) => {
    if (Date.now() < dragRef.current.suppressClickUntil) return
    onTabChange(tabId)
  }

  const displayedTab = dragPreviewTab || activeTab

  return (
    <nav
      ref={navRef}
      onPointerDown={handleDockPointerDown}
      className="mobile-bottom-dock"
      aria-label="Mobile Navigation Dock"
    >
      <div
        ref={pillRef}
        aria-hidden="true"
        className={`mobile-dock-slider-pill ${isDraggingPill ? 'is-dragging' : ''}`}
      />

      {LEFT_TABS.map((item) => (
        <DockTabButton
          key={item.id}
          item={item}
          active={displayedTab === item.id}
          onSelect={handleSelectTab}
          setButtonRef={(el) => { btnRefs.current[item.id] = el }}
        />
      ))}

      {/* Inline Elevated Center Voice Trigger */}
      <button
        type="button"
        onClick={onMicClick}
        className={`mobile-dock-mic ${isListening ? 'is-recording' : ''}`}
        aria-label={isListening ? 'Cancel Voice' : 'Voice AI Assistant'}
      >
        <div className="mobile-mic-inner">
          {isListening ? (
            <svg
              xmlns="http://www.w3.org/2000/svg"
              width="22"
              height="22"
              viewBox="0 0 24 24"
              fill="none"
              style={{ display: 'block' }}
            >
              <path
                fill="none"
                stroke="#FFFFFF"
                strokeLinecap="round"
                strokeLinejoin="round"
                strokeWidth={2.2}
                d="m2.63 2.63l18.74 18.74M1.75 15.4V8.6m20.5 6.8V8.6m-8.2 6.53v-1m0-3.61V8.87m-4.1 8.45V10m-4.1 9.61V6.05m12.3 8.58V4.39m0 15.22V18.4"
              />
            </svg>
          ) : (
            <svg
              xmlns="http://www.w3.org/2000/svg"
              width="22"
              height="22"
              viewBox="0 0 24 24"
              fill="none"
              style={{ display: 'block' }}
            >
              <path
                fill="none"
                stroke="#FFFFFF"
                strokeLinecap="round"
                strokeLinejoin="round"
                strokeWidth={2.4}
                d="M3.75 10.5v3m2.75-6v9m5.5-8v7M17.5 7v10m2.75-7v4m-11-9.25v14.5m5.5-15.5v16.5"
              />
            </svg>
          )}
        </div>
      </button>

      {RIGHT_TABS.map((item) => (
        <DockTabButton
          key={item.id}
          item={item}
          active={displayedTab === item.id}
          onSelect={handleSelectTab}
          setButtonRef={(el) => { btnRefs.current[item.id] = el }}
        />
      ))}
    </nav>
  )
}
