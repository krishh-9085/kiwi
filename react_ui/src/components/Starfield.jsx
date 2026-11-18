import React, { useEffect, useRef } from 'react'

export default function Starfield() {
  const canvasRef = useRef(null)

  useEffect(() => {
    const canvas = canvasRef.current
    if (!canvas) return
    const ctx = canvas.getContext('2d')
    let stars = []
    let animationId
    let lastTime = 0

    const resize = () => {
      canvas.width = window.innerWidth
      canvas.height = window.innerHeight
      stars = []
      const count = Math.min(100, Math.round((canvas.width * canvas.height) / 14000))
      for (let i = 0; i < count; i++) {
        stars.push({
          x: Math.random() * canvas.width,
          y: Math.random() * canvas.height,
          r: Math.random() * 1.1 + 0.3,
          speed: Math.random() * 0.12 + 0.03,
          phase: Math.random() * 6.28
        })
      }
    }

    const render = (time) => {
      if (time - lastTime < 32) {
        animationId = requestAnimationFrame(render)
        return
      }
      lastTime = time
      ctx.clearRect(0, 0, canvas.width, canvas.height)

      for (let i = 0; i < stars.length; i++) {
        const s = stars[i]
        s.y -= s.speed
        if (s.y < 0) {
          s.y = canvas.height
          s.x = Math.random() * canvas.width
        }
        const alpha = 0.25 + 0.35 * Math.sin(time * 0.002 + s.phase)
        ctx.fillStyle = `rgba(180, 210, 255, ${Math.max(0.1, alpha)})`
        ctx.beginPath()
        ctx.arc(s.x, s.y, s.r, 0, Math.PI * 2)
        ctx.fill()
      }
      animationId = requestAnimationFrame(render)
    }

    resize()
    window.addEventListener('resize', resize)
    animationId = requestAnimationFrame(render)

    return () => {
      window.removeEventListener('resize', resize)
      cancelAnimationFrame(animationId)
    }
  }, [])

  return (
    <canvas
      ref={canvasRef}
      style={{
        position: 'fixed',
        top: 0,
        left: 0,
        width: '100%',
        height: '100%',
        pointerEvents: 'none',
        zIndex: 0,
        opacity: 0.65
      }}
    />
  )
}
