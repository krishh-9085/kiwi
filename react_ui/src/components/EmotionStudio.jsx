import React from 'react'
import { Smile, Heart, Flame, Moon, Eye, Zap, Coffee, Sparkles, AlertCircle, Glasses } from 'lucide-react'

const EMOTIONS = [
  { id: 'idle', label: 'Idle / Neutral', icon: Eye, color: '#38bdf8' },
  { id: 'happy', label: 'Happy', icon: Smile, color: '#34d399' },
  { id: 'excited', label: 'Excited', icon: Sparkles, color: '#fbbf24' },
  { id: 'sleeping', label: 'Sleeping', icon: Moon, color: '#94a3b8' },
  { id: 'hearts', label: 'Love Hearts', icon: Heart, color: '#fb7185' },
  { id: 'curious', label: 'Curious', icon: Coffee, color: '#38bdf8' },
  { id: 'wink', label: 'Wink', icon: Sparkles, color: '#a78bfa' },
  { id: 'angry', label: 'Angry', icon: Flame, color: '#f43f5e' },
  { id: 'cool', label: 'Cool Shades', icon: Glasses, color: '#38bdf8' },
  { id: 'surprised', label: 'Surprised', icon: AlertCircle, color: '#f59e0b' },
  { id: 'laughing', label: 'Laughing', icon: Smile, color: '#34d399' },
  { id: 'bored', label: 'Bored', icon: Coffee, color: '#64748b' }
]

const SKINS = [
  { id: 'kiwi', label: 'Kiwi (Default)' },
  { id: 'pixel', label: 'Retro 8-Bit' },
  { id: 'squircle_glint', label: 'Lumina Spark' },
  { id: 'mecha', label: 'Mecha' },
  { id: 'emo', label: 'EMO Style' }
]

export default function EmotionStudio({ currentEmotion, currentSkin, onTriggerAction }) {
  return (
    <div className="glass-panel" style={{ padding: '24px', display: 'flex', flexDirection: 'column', gap: '20px' }}>
      <div style={{ display: 'flex', alignItems: 'center', justifyContent: 'space-between' }}>
        <div style={{ display: 'flex', alignItems: 'center', gap: '10px' }}>
          <Smile size={18} color="var(--accent-cyan)" />
          <h2 style={{ fontSize: '15px', fontWeight: 600 }}>Face & Emotion Studio</h2>
        </div>
        <span style={{ fontSize: '11px', color: 'var(--text-muted)' }}>OLED Expressions</span>
      </div>

      {/* Emotion Grid */}
      <div style={{
        display: 'grid',
        gridTemplateColumns: 'repeat(auto-fill, minmax(100px, 1fr))',
        gap: '10px'
      }}>
        {EMOTIONS.map((emo) => {
          const Icon = emo.icon
          const isActive = (currentEmotion || '').toLowerCase() === emo.id
          return (
            <button
              key={emo.id}
              onClick={() => onTriggerAction(`/do?emotion=${emo.id}`)}
              style={{
                background: isActive ? 'rgba(56, 189, 248, 0.15)' : 'rgba(0, 0, 0, 0.25)',
                border: isActive ? `1.5px solid ${emo.color}` : '1px solid var(--border-subtle)',
                borderRadius: '12px',
                padding: '12px 8px',
                display: 'flex',
                flexDirection: 'column',
                alignItems: 'center',
                gap: '8px',
                cursor: 'pointer',
                transition: 'all 0.18s ease',
                boxShadow: isActive ? `0 0 16px ${emo.color}40` : 'none'
              }}
              className="btn-action"
            >
              <Icon size={22} color={isActive ? emo.color : 'var(--text-muted)'} />
              <span style={{
                fontSize: '11px',
                fontWeight: isActive ? 600 : 400,
                color: isActive ? '#ffffff' : 'var(--text-muted)'
              }}>
                {emo.label}
              </span>
            </button>
          )
        })}
      </div>

      {/* Eye Skins Bar */}
      <div style={{ paddingTop: '10px', borderTop: '1px solid var(--border-subtle)' }}>
        <span style={{ fontSize: '12px', fontWeight: 600, color: 'var(--text-muted)', display: 'block', marginBottom: '10px' }}>
          Eye Shape Style (Skins)
        </span>
        <div style={{ display: 'flex', gap: '8px', flexWrap: 'wrap' }}>
          {SKINS.map((s) => {
            const isSkinActive = (currentSkin || '').toLowerCase() === s.id
            return (
              <button
                key={s.id}
                onClick={() => onTriggerAction(`/do?skin=${s.id}`)}
                className="btn-action"
                style={{
                  fontSize: '11px',
                  padding: '6px 12px',
                  borderRadius: '8px',
                  background: isSkinActive ? 'var(--accent-cyan)' : 'rgba(255,255,255,0.04)',
                  color: isSkinActive ? '#000' : 'var(--text-main)',
                  fontWeight: isSkinActive ? 700 : 400,
                  border: isSkinActive ? '1px solid #38bdf8' : '1px solid var(--border-subtle)'
                }}
              >
                {s.label}
              </button>
            )
          })}
        </div>
      </div>
    </div>
  )
}
