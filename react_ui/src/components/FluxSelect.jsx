import React, { useState, useRef, useEffect } from 'react'
import { ChevronDown, Check } from 'lucide-react'

/**
 * UntitledUI-styled Rounded Dropdown / Select Component
 * Replaces boxy Windows native <select> menus with modern Scandinavian rounded floating menus.
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
  const containerRef = useRef(null)

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

  // Close when clicking outside or pressing Escape
  useEffect(() => {
    const handleOutsideClick = (e) => {
      if (containerRef.current && !containerRef.current.contains(e.target)) {
        setOpen(false)
      }
    }
    const handleKeyDown = (e) => {
      if (e.key === 'Escape') setOpen(false)
    }

    if (open) {
      document.addEventListener('mousedown', handleOutsideClick)
      document.addEventListener('touchstart', handleOutsideClick)
      document.addEventListener('keydown', handleKeyDown)
    }
    return () => {
      document.removeEventListener('mousedown', handleOutsideClick)
      document.removeEventListener('touchstart', handleOutsideClick)
      document.removeEventListener('keydown', handleKeyDown)
    }
  }, [open])

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
        onClick={() => setOpen(!open)}
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
            ? '1.5px solid var(--accent-lime)'
            : (dark ? '1px solid rgba(255, 255, 255, 0.12)' : '1px solid var(--border-light)'),
          color: dark ? '#FFFFFF' : 'var(--text-dark)',
          fontSize: btnFontSize,
          fontWeight: 600,
          fontFamily: 'inherit',
          cursor: disabled ? 'not-allowed' : 'pointer',
          outline: 'none',
          boxShadow: open
            ? '0 0 0 3px rgba(210, 250, 75, 0.35)'
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

      {/* Floating Popover Menu (Untitled UI Rounded Card with soft shadow & pills) */}
      {open && (
        <div
          role="listbox"
          style={{
            position: 'absolute',
            top: 'calc(100% + 6px)',
            left: 0,
            minWidth: '100%',
            width: 'max-content',
            maxWidth: 'min(360px, 90vw)',
            zIndex: 99999,
            background: dark ? '#1A1C23' : 'var(--card-white)',
            borderRadius: compact ? '12px' : '16px',
            border: dark ? '1px solid rgba(255, 255, 255, 0.12)' : '1px solid var(--border-light)',
            boxShadow: '0 16px 40px -4px rgba(0, 0, 0, 0.35), 0 6px 16px -2px rgba(0, 0, 0, 0.15)',
            padding: compact ? '4px' : '6px',
            maxHeight: '260px',
            overflowY: 'auto',
            animation: 'fluxDropdownFadeIn 0.18s cubic-bezier(0.16, 1, 0.3, 1)'
          }}
        >
          {normalizedOptions.map((opt) => {
            const isSelected = opt.value === String(value)

            return (
              <div
                key={opt.value}
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
                  color: 'var(--text-dark)',
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
        </div>
      )}
    </div>
  )
}
