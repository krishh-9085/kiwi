import React, { useState, useRef, useEffect, cloneElement } from 'react'
import { createPortal } from 'react-dom'

/**
 * Floating Tooltip rendered into document.body with boundary collision clamping.
 */
export const Tooltip = ({
  title,
  description,
  children,
  arrow = true,
  delay = 180,
  placement = 'top',
  offset = 8,
  className = ''
}) => {
  const [isOpen, setIsOpen] = useState(false)
  const [coords, setCoords] = useState({ top: 0, left: 0 })
  const triggerRef = useRef(null)
  const tooltipRef = useRef(null)
  const timeoutRef = useRef(null)

  const updatePosition = () => {
    if (!triggerRef.current) return
    const rect = triggerRef.current.getBoundingClientRect()
    const tooltipEl = tooltipRef.current
    const ttWidth = tooltipEl ? tooltipEl.offsetWidth : 160
    const ttHeight = tooltipEl ? tooltipEl.offsetHeight : 34

    let top = 0
    let left = 0

    if (placement === 'top') {
      top = rect.top - ttHeight - offset
      left = rect.left + rect.width / 2 - ttWidth / 2
    } else if (placement === 'bottom') {
      top = rect.bottom + offset
      left = rect.left + rect.width / 2 - ttWidth / 2
    } else if (placement === 'left') {
      top = rect.top + rect.height / 2 - ttHeight / 2
      left = rect.left - ttWidth - offset
    } else if (placement === 'right') {
      top = rect.top + rect.height / 2 - ttHeight / 2
      left = rect.right + offset
    }

    const pad = 8
    if (left < pad) left = pad
    if (left + ttWidth > window.innerWidth - pad) {
      left = window.innerWidth - pad - ttWidth
    }
    if (top < pad && placement === 'top') {
      top = rect.bottom + offset
    }

    setCoords({ top, left })
  }

  const isTouchDevice = () => {
    if (typeof window === 'undefined') return false
    return (
      'ontouchstart' in window ||
      navigator.maxTouchPoints > 0 ||
      window.matchMedia('(pointer: coarse)').matches ||
      window.innerWidth <= 768
    )
  }

  const handleMouseEnter = () => {
    if (isTouchDevice()) return
    clearTimeout(timeoutRef.current)
    timeoutRef.current = setTimeout(() => {
      setIsOpen(true)
    }, delay)
  }

  const handleMouseLeave = () => {
    clearTimeout(timeoutRef.current)
    setIsOpen(false)
  }

  useEffect(() => {
    if (isOpen) {
      updatePosition()
      const timer = setTimeout(updatePosition, 10)
      const handleScrollOrResize = () => updatePosition()
      window.addEventListener('scroll', handleScrollOrResize, true)
      window.addEventListener('resize', handleScrollOrResize)
      return () => {
        clearTimeout(timer)
        window.removeEventListener('scroll', handleScrollOrResize, true)
        window.removeEventListener('resize', handleScrollOrResize)
      }
    }
  }, [isOpen])

  useEffect(() => {
    return () => clearTimeout(timeoutRef.current)
  }, [])

  if (!React.isValidElement(children)) {
    return <>{children}</>
  }

  const trigger = cloneElement(children, {
    ref: (node) => {
      triggerRef.current = node
      const { ref } = children
      if (typeof ref === 'function') ref(node)
      else if (ref) ref.current = node
    },
    onMouseEnter: (e) => {
      handleMouseEnter()
      if (children.props && children.props.onMouseEnter) children.props.onMouseEnter(e)
    },
    onMouseOver: (e) => {
      handleMouseEnter()
      if (children.props && children.props.onMouseOver) children.props.onMouseOver(e)
    },
    onMouseLeave: (e) => {
      handleMouseLeave()
      if (children.props && children.props.onMouseLeave) children.props.onMouseLeave(e)
    },
    onFocus: (e) => {
      if (isTouchDevice()) return
      handleMouseEnter()
      if (children.props && children.props.onFocus) children.props.onFocus(e)
    },
    onTouchStart: (e) => {
      clearTimeout(timeoutRef.current)
      setIsOpen(false)
      if (children.props && children.props.onTouchStart) children.props.onTouchStart(e)
    },
    onBlur: (e) => {
      handleMouseLeave()
      if (children.props && children.props.onBlur) children.props.onBlur(e)
    }
  })

  return (
    <>
      {trigger}
      {isOpen &&
        typeof document !== 'undefined' &&
        createPortal(
          <div
            ref={tooltipRef}
            role="tooltip"
            className={`untitledui-tooltip-box ${className}`}
            style={{
              position: 'fixed',
              top: `${coords.top}px`,
              left: `${coords.left}px`,
              zIndex: 999999,
              pointerEvents: 'none'
            }}
          >
            <span className="untitledui-tooltip-title">{title}</span>
            {description && <span className="untitledui-tooltip-desc">{description}</span>}
          </div>,
          document.body
        )}
    </>
  )
}

export const TooltipTrigger = ({ children }) => <>{children}</>
