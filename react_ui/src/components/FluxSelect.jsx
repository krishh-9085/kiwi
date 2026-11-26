import React, { useState, useRef, useEffect, useCallback } from 'react'
import { createPortal } from 'react-dom'
import { ChevronDown, Check } from 'lucide-react'

/**
 * UntitledUI-styled Rounded Dropdown / Select Component
 * Uses a document.body portal with viewport-aware flip/alignment so menus are never clipped
 * by card overflow containers or pushed off-screen near mobile bottom navigation.
 */
export default function FluxSelect({
  name,
  value,
  onChange,
  options = [],
  placeholder = 'Select option...',
  disabled = false,
  style = {},
  dark = false,
  compact = false,
  micro = false,
  className = ''
}) {
  const [open, setOpen] = useState(false)
  const [menuPos, setMenuPos] = useState(null)
  const containerRef = useRef(null)
  const menuRef = useRef(null)
  const selectedRef = useRef(null)

  const computeMenuPosition = useCallback(() => {
    const el = containerRef.current
    if (!el || typeof window === 'undefined') return null
    const rect = el.getBoundingClientRect()
    const vw = window.innerWidth
    const vh = window.innerHeight
    const bottomPad = vw <= 768 ? 92 : 14
    const topPad = 14
    const gap = 6

    const spaceBelow = Math.max(0, vh - rect.bottom - bottomPad - gap)
    const spaceAbove = Math.max(0, rect.top - topPad - gap)
    const openUp = spaceBelow < 200 && spaceAbove > spaceBelow
    const availableHeight = openUp ? spaceAbove : spaceBelow
    const maxHeight = Math.max(120, Math.min(260, availableHeight))
    const maxWidth = Math.min(360, vw - 24)
    const minWidth = Math.min(rect.width, maxWidth)
    const alignRight = rect.left + Math.max(rect.width, 165) > vw - 12

    return {
      position: 'fixed',
      zIndex: 99999,
      minWidth: `${minWidth}px`,
      width: 'max-content',
      maxWidth: `${maxWidth}px`,
      maxHeight: `${maxHeight}px`,
      ...(alignRight
        ? { right: `${Math.max(12, vw - rect.right)}px` }
        : { left: `${Math.max(12, Math.min(rect.left, vw - minWidth - 12))}px` }),
      ...(openUp
        ? { bottom: `${Math.max(bottomPad, vh - rect.top + gap)}px`, transformOrigin: 'bottom center' }
        : { top: `${Math.max(topPad, rect.bottom + gap)}px`, transformOrigin: 'top center' })
    }
  }, [])

  // Normalize options array
  const normalizedOptions = options.map((opt) => {
    if (typeof opt === 'object' && opt !== null) {
      return {
        value: String(opt.val ?? opt.value ?? opt.id),
        label: String(opt.lbl ?? opt.label ?? opt.name ?? opt.val ?? opt.value)
      }
    }
    return { value: String(opt), label: String(opt) }
  })

  // Find currently selected option
  const selectedOption = normalizedOptions.find((opt) => opt.value === String(value))
  const displayLabel = selectedOption ? selectedOption.label : placeholder

  // Close when clicking outside, pressing Escape, or scrolling away; keep menu aligned on resize/scroll
  useEffect(() => {
    if (!open) return undefined

    const handleOutsideClick = (e) => {
      const inTrigger = containerRef.current && containerRef.current.contains(e.target)
      const inMenu = menuRef.current && menuRef.current.contains(e.target)
      if (!inTrigger && !inMenu) {
        setOpen(false)
      }
    }
    const handleKeyDown = (e) => {
      if (e.key === 'Escape') setOpen(false)
    }
    const handleViewportUpdate = (e) => {
      if (e?.type === 'scroll' && menuRef.current && menuRef.current.contains(e.target)) return
      const rect = containerRef.current?.getBoundingClientRect()
      if (!rect || rect.bottom < 0 || rect.top > window.innerHeight) {
        setOpen(false)
        return
      }
      setMenuPos(computeMenuPosition())
    }

    const rafId = window.requestAnimationFrame(() => {
      selectedRef.current?.scrollIntoView({ block: 'nearest' })
    })

    document.addEventListener('mousedown', handleOutsideClick)
    document.addEventListener('touchstart', handleOutsideClick)
    document.addEventListener('keydown', handleKeyDown)
    window.addEventListener('resize', handleViewportUpdate)
    window.addEventListener('scroll', handleViewportUpdate, true)

    return () => {
      window.cancelAnimationFrame(rafId)
      document.removeEventListener('mousedown', handleOutsideClick)
      document.removeEventListener('touchstart', handleOutsideClick)
      document.removeEventListener('keydown', handleKeyDown)
      window.removeEventListener('resize', handleViewportUpdate)
      window.removeEventListener('scroll', handleViewportUpdate, true)
    }
  }, [open, computeMenuPosition])

  const handleToggleOpen = () => {
    if (disabled) return
    if (!open) {
      setMenuPos(computeMenuPosition())
      setOpen(true)
    } else {
      setOpen(false)
    }
  }

  const handleSelect = (optVal) => {
    if (onChange) {
      // Support both standard synthetic event and direct value
      onChange({
        target: { name, value: optVal }
      })
    }
    setOpen(false)
  }

  const isMicro = micro
  const btnPadding = isMicro ? '2px 7px' : (compact ? '6px 9px' : '11px 16px')
  const btnRadius = isMicro ? '7px' : (compact ? '9px' : '14px')
  const btnFontSize = isMicro ? '10px' : (compact ? '11px' : '13px')
  const btnGap = isMicro ? '3px' : (compact ? '5px' : '10px')
  const iconSize = isMicro ? 10 : (compact ? 12 : 16)

  return (
    <div
      ref={containerRef}
      className={`flux-select-container ${className}`}
      style={{
        position: 'relative',
        width: '100%',
        maxWidth: '100%',
        boxSizing: 'border-box',
        userSelect: 'none',
        ...style
      }}
    >
      {/* Hidden input for HTML form submissions */}
      {name && <input type="hidden" name={name} value={value ?? ''} />}

      {/* Trigger Button (Untitled UI Rounded Select Field) */}
      <button
        type="button"
        disabled={disabled}
        onClick={handleToggleOpen}
        aria-haspopup="listbox"
        aria-expanded={open}
        style={{
          width: '100%',
          maxWidth: '100%',
          boxSizing: 'border-box',
          display: 'flex',
          alignItems: 'center',
          justifyContent: 'space-between',
          gap: btnGap,
          padding: btnPadding,
          borderRadius: btnRadius,
          background: dark ? '#22252C' : 'var(--card-muted)',
          border: open
            ? '1px solid rgba(56, 189, 248, 0.55)'
            : (dark ? '1px solid rgba(255, 255, 255, 0.12)' : '1px solid var(--border-light)'),
          color: dark ? '#FFFFFF' : 'var(--text-dark)',
          fontSize: btnFontSize,
          fontWeight: 600,
          fontFamily: 'inherit',
          cursor: disabled ? 'not-allowed' : 'pointer',
          outline: 'none',
          boxShadow: open
            ? '0 0 0 3px rgba(56, 189, 248, 0.16)'
            : 'none',
          transition: 'all 0.18s cubic-bezier(0.16, 1, 0.3, 1)'
        }}
      >
        <span style={{
          minWidth: 0,
          overflow: 'hidden',
          textOverflow: 'ellipsis',
          whiteSpace: 'nowrap',
          textAlign: 'left',
          flex: 1
        }}>
          {displayLabel}
        </span>

        {/* Untitled UI Chevron Icon */}
        <div style={{
          display: 'flex',
          alignItems: 'center',
          justifyContent: 'center',
          flexShrink: 0,
          transform: open ? 'rotate(180deg)' : 'rotate(0deg)',
          transition: 'transform 0.2s cubic-bezier(0.16, 1, 0.3, 1)'
        }}>
          <ChevronDown
            className="shrink-0 text-fg-quaternary"
            style={{
              width: `${iconSize}px`,
              height: `${iconSize}px`,
              color: 'var(--text-muted)'
            }}
          />
        </div>
      </button>

      {/* Portal Floating Popover Menu (Never clipped by parent overflow containers) */}
      {open && menuPos && typeof document !== 'undefined' && createPortal(
        <div
          ref={menuRef}
          role="listbox"
          style={{
            ...menuPos,
            boxSizing: 'border-box',
            background: dark ? '#1A1C23' : 'var(--card-white)',
            borderRadius: compact ? '12px' : '16px',
            border: dark ? '1px solid rgba(255, 255, 255, 0.14)' : '1px solid var(--border-light)',
            boxShadow: '0 18px 44px rgba(0, 0, 0, 0.48), 0 6px 16px rgba(0, 0, 0, 0.22)',
            padding: compact ? '4px' : '6px',
            overflowY: 'auto',
            overscrollBehavior: 'contain',
            animation: 'fluxDropdownFadeIn 0.16s cubic-bezier(0.16, 1, 0.3, 1)'
          }}
        >
          {normalizedOptions.map((opt) => {
            const isSelected = opt.value === String(value)

            return (
              <div
                key={opt.value}
                ref={isSelected ? selectedRef : null}
                role="option"
                aria-selected={isSelected}
                onClick={() => handleSelect(opt.value)}
                style={{
                  display: 'flex',
                  alignItems: 'center',
                  justifyContent: 'space-between',
                  gap: '8px',
                  padding: isMicro ? '4px 8px' : (compact ? '6px 10px' : '10px 14px'),
                  borderRadius: isMicro ? '6px' : (compact ? '8px' : '10px'),
                  fontSize: isMicro ? '10px' : (compact ? '11px' : '13px'),
                  fontWeight: isSelected ? 700 : 500,
                  color: dark ? '#F8FAFC' : 'var(--text-dark)',
                  background: isSelected
                    ? 'var(--active-card-bg)'
                    : 'transparent',
                  cursor: 'pointer',
                  transition: 'background 0.12s ease'
                }}
                onMouseEnter={(e) => {
                  if (!isSelected) {
                    e.currentTarget.style.background = 'var(--card-muted)'
                  }
                }}
                onMouseLeave={(e) => {
                  if (!isSelected) {
                    e.currentTarget.style.background = 'transparent'
                  }
                }}
              >
                <span>{opt.label}</span>
                {isSelected && (
                  <Check
                    strokeWidth={2.25}
                    style={{
                      flexShrink: 0,
                      width: isMicro ? '10px' : (compact ? '12px' : '15px'),
                      height: isMicro ? '10px' : (compact ? '12px' : '15px'),
                      color: 'var(--select-check, var(--text-dark))'
                    }}
                  />
                )}
              </div>
            )
          })}
        </div>,
        document.body
      )}
    </div>
  )
}
