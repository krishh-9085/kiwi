import React, { useState } from 'react'

/**
 * Animated Lightbulb Theme Toggle Button
 * CSS-animated SVG lightbulb matching Kiwi Robot design system.
 */
export function ThemeToggle({
  isDark: controlledIsDark,
  onToggle,
  className = '',
  mobile = false,
  style = {}
}) {
  const [internalIsDark, setInternalIsDark] = useState(false)
  const isDark = controlledIsDark !== undefined ? controlledIsDark : internalIsDark

  const handleClick = (e) => {
    if (onToggle) {
      onToggle(e)
    } else {
      setInternalIsDark(!internalIsDark)
    }
  }

  return (
    <button
      type="button"
      role="switch"
      aria-checked={isDark}
      aria-label={isDark ? 'Switch to light mode' : 'Switch to dark mode'}
      title={isDark ? 'Switch to Light Mode' : 'Switch to Dark Mode'}
      className={`flux-theme-btn-4 ${isDark ? 'is-dark' : 'is-light'} ${mobile ? 'mobile-size' : ''} ${className}`}
      style={style}
      onClick={handleClick}
    >
      <svg
        xmlns="http://www.w3.org/2000/svg"
        aria-hidden="true"
        strokeWidth="0.8"
        stroke="currentColor"
        fill="currentColor"
        strokeLinecap="round"
        viewBox="0 0 32 32"
        className="flux-theme-btn-4-svg"
      >
        {/* Animate bulb to exact vertical center in dark mode when top rays are hidden */}
        <g
          style={{
            transform: isDark ? 'translateY(-2.8px)' : 'translateY(0px)',
            transition: 'transform 0.35s ease-in-out'
          }}
        >
          {/* Bulb glass outline */}
          <path
            strokeWidth="0"
            d="M9.4 9.9c1.8-1.8 4.1-2.7 6.6-2.7 5.1 0 9.3 4.2 9.3 9.3 0 2.3-.8 4.4-2.3 6.1-.7.8-2 2.8-2.5 4.4 0 .2-.2.4-.5.4-.2 0-.4-.2-.4-.5v-.1c.5-1.8 2-3.9 2.7-4.8 1.4-1.5 2.1-3.5 2.1-5.6 0-4.7-3.7-8.5-8.4-8.5-2.3 0-4.4.9-5.9 2.5-1.6 1.6-2.5 3.7-2.5 6 0 2.1.7 4 2.1 5.6.8.9 2.2 2.9 2.7 4.9 0 .2-.1.5-.4.5h-.1c-.2 0-.4-.1-.4-.4-.5-1.7-1.8-3.7-2.5-4.5-1.5-1.7-2.3-3.9-2.3-6.1 0-2.3 1-4.7 2.7-6.5z"
          />
          {/* Base screw treads */}
          <path d="M19.8 28.3h-7.6" />
          <path d="M19.8 29.5h-7.6" />
          <path d="M19.8 30.7h-7.6" />

          {/* Animated Filament (visible & glowing in Light Mode, unwinds in Dark Mode) */}
          <path
            pathLength="1"
            strokeDasharray="1"
            style={{
              strokeDashoffset: isDark ? 1 : 0,
              opacity: isDark ? 0 : 1,
              transition: 'stroke-dashoffset 0.35s ease-in-out, opacity 0.35s ease-in-out'
            }}
            fill="none"
            stroke="currentColor"
            strokeWidth="1.3"
            d="M14.6 27.1c0-3.4 0-6.8-.1-10.2-.2-1-1.1-1.7-2-1.7-1.2-.1-2.3 1-2.2 2.3.1 1 .9 1.9 2.1 2h7.2c1.1-.1 2-1 2.1-2 .1-1.2-1-2.3-2.2-2.3-.9 0-1.7.7-2 1.7 0 3.4 0 6.8-.1 10.2"
          />

          {/* Animated Light Rays (expand and beam out in Light Mode, shrink and fade in Dark Mode) */}
          <g
            style={{
              transform: isDark ? 'scale(0.35)' : 'scale(1)',
              transformOrigin: 'center',
              transformBox: 'fill-box',
              opacity: isDark ? 0 : 1,
              transition: 'transform 0.35s ease-in-out, opacity 0.35s ease-in-out'
            }}
            stroke="currentColor"
            strokeWidth="1.5"
            fill="none"
          >
            <path d="M16 6.4V1.3" />
            <path d="M26.3 15.8h5.1" />
            <path d="m22.6 9 3.7-3.6" />
            <path d="M9.4 9 5.7 5.4" />
            <path d="M5.7 15.8H.6" />
          </g>
        </g>
      </svg>
    </button>
  )
}

export default ThemeToggle
