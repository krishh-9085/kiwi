import React, { useState, useEffect } from 'react'
import { Sliders, Volume2, Sun, Moon, Music, Radio, Zap, Sparkles, Smile, VolumeX } from 'lucide-react'
import WakeSlider from './ui/WakeSlider'

const SOUNDS = [
  { id: 'wake', label: 'Wake Chime' },
  { id: 'giggle', label: 'Pet Giggle' },
  { id: 'sparkle', label: 'Magic Sparkle' },
  { id: 'laugh', label: 'Joy Laugh' },
  { id: 'alert', label: 'Alert Ping' },
  { id: 'violin', label: 'Sad Violin' }
]

export default function HardwareDeck({ status, onTriggerAction }) {
  const [volume, setVolume] = useState(70)
  const [brightness, setBrightness] = useState(90)

  useEffect(() => {
    if (status?.volume != null) setVolume(Math.round(status.volume / 5) * 5)
    if (status?.brightness != null) setBrightness(Math.max(5, Math.round(status.brightness / 5) * 5))
  }, [status])

  const handleVolumeChange = (e) => {
    const val = Number(e.target.value)
    setVolume(val)
  }

  const handleVolumeCommit = () => {
    onTriggerAction(`/do?volume=${volume}`)
  }

  const handleBrightnessChange = (e) => {
    const val = Number(e.target.value)
    setBrightness(val)
  }

  const handleBrightnessCommit = () => {
    onTriggerAction(`/do?bright=${brightness}`)
  }

  return (
    <div className="glass-panel" style={{ padding: '24px', display: 'flex', flexDirection: 'column', gap: '20px' }}>
      <div style={{ display: 'flex', alignItems: 'center', justifyContent: 'space-between' }}>
        <div style={{ display: 'flex', alignItems: 'center', gap: '10px' }}>
          <Sliders size={18} color="var(--accent-cyan)" />
          <h2 style={{ fontSize: '15px', fontWeight: 600 }}>Hardware & Soundboard</h2>
        </div>
        <span style={{ fontSize: '11px', color: 'var(--text-muted)' }}>Audio & Display</span>
      </div>

      {/* Sliders Container */}
      <div style={{ display: 'flex', flexDirection: 'column', gap: '16px' }}>
        {/* Speaker Volume Slider */}
        <div style={{ background: 'rgba(0,0,0,0.25)', padding: '14px', borderRadius: '12px', border: '1px solid var(--border-subtle)' }}>
          <div style={{ display: 'flex', alignItems: 'center', justifyContent: 'space-between', marginBottom: '8px' }}>
            <span style={{ fontSize: '12px', color: 'var(--text-main)', display: 'flex', alignItems: 'center', gap: '6px' }}>
              <Volume2 size={15} color="var(--accent-cyan)" />
              Speaker Volume
            </span>
            <div style={{ display: 'flex', alignItems: 'center', gap: '8px' }}>
              <span style={{ fontSize: '12px', fontWeight: 700, color: 'var(--accent-cyan)' }}>{volume}%</span>
              <button
                onClick={() => onTriggerAction('/do?sound=wake')}
                className="btn-action"
                style={{ padding: '2px 8px', fontSize: '10px', borderRadius: '6px' }}
              >
                Test Sound
              </button>
            </div>
          </div>
          <WakeSlider
            value={volume}
            min={0}
            max={100}
            step={5}
            bars={20}
            height={32}
            restHeight={16}
            gap={4}
            fillColor="var(--accent-cyan, #38bdf8)"
            trackColor="rgba(255,255,255,0.12)"
            crestColor="#67e8f9"
            onChange={setVolume}
            onCommit={() => onTriggerAction(`/do?volume=${volume}`)}
            ariaLabel="Speaker Volume"
          />
        </div>

        {/* Display Brightness Slider */}
        <div style={{ background: 'rgba(0,0,0,0.25)', padding: '14px', borderRadius: '12px', border: '1px solid var(--border-subtle)' }}>
          <div style={{ display: 'flex', alignItems: 'center', justifyContent: 'space-between', marginBottom: '8px' }}>
            <span style={{ fontSize: '12px', color: 'var(--text-main)', display: 'flex', alignItems: 'center', gap: '6px' }}>
              <Sun size={15} color="#38bdf8" />
              OLED Display Brightness
            </span>
            <span style={{ fontSize: '12px', fontWeight: 700, color: '#38bdf8' }}>{brightness}%</span>
          </div>
          <WakeSlider
            value={brightness}
            min={5}
            max={100}
            step={5}
            bars={20}
            height={32}
            restHeight={16}
            gap={4}
            fillColor="#0ea5e9"
            trackColor="rgba(255,255,255,0.12)"
            crestColor="#38bdf8"
            onChange={setBrightness}
            onCommit={() => onTriggerAction(`/do?bright=${brightness}`)}
            ariaLabel="Display Brightness"
          />
        </div>
      </div>

      {/* Sound Effects Soundboard */}
      <div>
        <span style={{ fontSize: '12px', fontWeight: 600, color: 'var(--text-muted)', display: 'block', marginBottom: '10px' }}>
          Interactive Sound Effects
        </span>
        <div style={{ display: 'grid', gridTemplateColumns: 'repeat(auto-fill, minmax(110px, 1fr))', gap: '8px' }}>
          {SOUNDS.map((snd) => (
            <button
              key={snd.id}
              onClick={() => onTriggerAction(`/do?sound=${snd.id}`)}
              className="btn-action"
              style={{
                fontSize: '11px',
                padding: '8px 10px',
                borderRadius: '8px',
                justifyContent: 'center',
                background: 'rgba(255,255,255,0.03)'
              }}
            >
              <Music size={12} color="var(--accent-cyan)" />
              <span>{snd.label}</span>
            </button>
          ))}
        </div>
      </div>

      {/* Modes Quick Switchers */}
      <div style={{ display: 'flex', gap: '8px', flexWrap: 'wrap', paddingTop: '10px', borderTop: '1px solid var(--border-subtle)' }}>
        <button
          onClick={() => onTriggerAction(`/do?boredom=${status?.boredom ? '0' : '1'}`)}
          className="btn-action"
          style={{
            fontSize: '11px', padding: '6px 12px', borderRadius: '8px',
            background: status?.boredom ? 'rgba(52, 211, 153, 0.15)' : 'rgba(255,255,255,0.04)',
            border: status?.boredom ? '1px solid #34d399' : '1px solid var(--border-subtle)'
          }}
        >
          <Zap size={12} color={status?.boredom ? '#34d399' : 'var(--text-muted)'} />
          <span>Auto Boredom: {status?.boredom ? 'ON' : 'OFF'}</span>
        </button>

        <button
          onClick={() => onTriggerAction(`/do?sleep=${status?.sleeping ? '0' : '1'}`)}
          className="btn-action"
          style={{
            fontSize: '11px', padding: '6px 12px', borderRadius: '8px',
            background: status?.sleeping ? 'rgba(148, 163, 184, 0.2)' : 'rgba(255,255,255,0.04)',
            border: status?.sleeping ? '1px solid #94a3b8' : '1px solid var(--border-subtle)'
          }}
        >
          <Moon size={12} color={status?.sleeping ? '#94a3b8' : 'var(--text-muted)'} />
          <span>Sleep Mode: {status?.sleeping ? 'Active' : 'Awake'}</span>
        </button>
      </div>
    </div>
  )
}
