import React, { useId } from 'react'

export default function KiwiLogo({ size = 34, className = '', style = {} }) {
  const uid = useId().replace(/:/g, '')
  const bgGradId = `kiwi-bg-${uid}`
  const bezelGradId = `kiwi-bezel-${uid}`
  const screenRefId = `kiwi-screen-${uid}`
  const glowId = `kiwi-glow-${uid}`
  const blinkName = `kiwi-blink-${uid}`

  const eyeStyle = {
    transformBox: 'view-box',
    animation: `${blinkName} 4s infinite`,
  }

  return (
    <svg
      xmlns="http://www.w3.org/2000/svg"
      viewBox="0 0 512 512"
      width={size}
      height={size}
      className={className}
      style={{ display: 'block', flexShrink: 0, ...style }}
    >
      <style>{`
        @keyframes ${blinkName} {
          0%, 92%, 100% { transform: scaleY(1); }
          95% { transform: scaleY(0.08); }
        }
        @media (prefers-reduced-motion: reduce) {
          .kiwi-eye-${uid} { animation: none !important; }
        }
      `}</style>

      <defs>
        <linearGradient id={bgGradId} x1="0%" y1="0%" x2="0%" y2="100%">
          <stop offset="0%" stopColor="#191c23" />
          <stop offset="50%" stopColor="#121318" />
          <stop offset="100%" stopColor="#0a0b0e" />
        </linearGradient>

        <linearGradient id={bezelGradId} x1="0%" y1="0%" x2="100%" y2="100%">
          <stop offset="0%" stopColor="#707685" stopOpacity="0.7" />
          <stop offset="40%" stopColor="#30333b" stopOpacity="0.4" />
          <stop offset="100%" stopColor="#1a1b20" stopOpacity="0.8" />
        </linearGradient>

        <linearGradient id={screenRefId} x1="50%" y1="0%" x2="50%" y2="100%">
          <stop offset="0%" stopColor="#ffffff" stopOpacity="0.07" />
          <stop offset="45%" stopColor="#ffffff" stopOpacity="0.01" />
          <stop offset="100%" stopColor="#000000" stopOpacity="0.0" />
        </linearGradient>

        <filter id={glowId} x="-30%" y="-30%" width="160%" height="160%">
          <feGaussianBlur stdDeviation="8" result="blur1" />
          <feGaussianBlur stdDeviation="3" result="blur2" />
          <feMerge>
            <feMergeNode in="blur1" />
            <feMergeNode in="blur2" />
            <feMergeNode in="SourceGraphic" />
          </feMerge>
        </filter>
      </defs>

      {/* Circular Base */}
      <circle cx="256" cy="256" r="248" fill={`url(#${bgGradId})`} />

      {/* Screen Highlight */}
      <circle cx="256" cy="256" r="248" fill={`url(#${screenRefId})`} />

      {/* Subtle Hardware Bezel Stroke */}
      <circle cx="256" cy="256" r="245" fill="none" stroke={`url(#${bezelGradId})`} strokeWidth="6" />

      {/* Inner Rim Highlight */}
      <circle cx="256" cy="256" r="243" fill="none" stroke="#ffffff" strokeOpacity="0.08" strokeWidth="1.5" />

      {/* Eyes Group (Tilted 8.1° Clockwise matching Kiwi face) */}
      <g filter={`url(#${glowId})`}>
        {/* Left Capsule Eye */}
        <g transform="rotate(8.1 205 256)">
          <rect
            className={`kiwi-eye-${uid}`}
            x="165" y="176" width="80" height="160" rx="40"
            fill="#ffffff"
            style={{ ...eyeStyle, transformOrigin: '205px 256px' }}
          />
        </g>

        {/* Right Capsule Eye */}
        <g transform="rotate(8.1 307 256)">
          <rect
            className={`kiwi-eye-${uid}`}
            x="267" y="176" width="80" height="160" rx="40"
            fill="#ffffff"
            style={{ ...eyeStyle, transformOrigin: '307px 256px' }}
          />
        </g>
      </g>
    </svg>
  )
}