import React from 'react'
import { Utensils, Heart, Hand, Sun, Coffee } from 'lucide-react'

export default function RobotCare({ status, onTriggerAction }) {
  const hunger = status?.hunger || 'satisfied'
  const meal = status?.meal || 'Snack'

  let hungerColor = '#34d399'
  let hungerLabel = 'Well Fed & Happy'

  if (hunger === 'hungry') {
    hungerColor = '#fbbf24'
    hungerLabel = 'Kiwi is Feeling Hungry!'
  } else if (hunger === 'angry') {
    hungerColor = '#f43f5e'
    hungerLabel = 'Kiwi is Hangry! Feed Now'
  }

  return (
    <div className="glass-panel" style={{ padding: '24px', display: 'flex', flexDirection: 'column', gap: '16px' }}>
      <div style={{ display: 'flex', alignItems: 'center', justifyContent: 'space-between' }}>
        <div style={{ display: 'flex', alignItems: 'center', gap: '10px' }}>
          <Heart size={18} color="var(--accent-coral)" />
          <h2 style={{ fontSize: '15px', fontWeight: 600 }}>Pet & Mood Care</h2>
        </div>
        <span style={{ fontSize: '11px', color: 'var(--text-muted)' }}>Companion Engine</span>
      </div>

      {/* Hunger & Mood Status Card */}
      <div style={{
        background: 'rgba(0,0,0,0.25)',
        border: `1px solid ${hungerColor}40`,
        borderRadius: '12px',
        padding: '14px',
        display: 'flex',
        alignItems: 'center',
        justifyContent: 'space-between'
      }}>
        <div>
          <span style={{ fontSize: '11px', color: 'var(--text-dim)', textTransform: 'uppercase', letterSpacing: '0.5px' }}>
            Current Mood / Appetite
          </span>
          <p style={{ fontSize: '14px', fontWeight: 600, color: hungerColor, marginTop: '2px' }}>
            {hungerLabel}
          </p>
        </div>
        <div style={{
          background: `${hungerColor}20`,
          border: `1px solid ${hungerColor}`,
          padding: '4px 10px',
          borderRadius: '999px',
          fontSize: '11px',
          fontWeight: 700,
          color: hungerColor
        }}>
          {meal}
        </div>
      </div>

      {/* Action Buttons */}
      <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr', gap: '10px' }}>
        <button
          onClick={() => onTriggerAction('/do?hunger=snack')}
          className="btn-action"
          style={{ justifyContent: 'center', padding: '12px', background: 'rgba(255,255,255,0.03)' }}
        >
          <Utensils size={15} color="var(--accent-cyan)" />
          <span>Feed Snack</span>
        </button>

        <button
          onClick={() => onTriggerAction('/do?hunger=lunch')}
          className="btn-action"
          style={{ justifyContent: 'center', padding: '12px', background: 'rgba(255,255,255,0.03)' }}
        >
          <Coffee size={15} color="#fbbf24" />
          <span>Serve Meal</span>
        </button>

        <button
          onClick={() => onTriggerAction('/do?pat=1')}
          className="btn-action"
          style={{ justifyContent: 'center', padding: '12px', background: 'rgba(255,255,255,0.03)' }}
        >
          <Hand size={15} color="#fb7185" />
          <span>Pat Head</span>
        </button>

        <button
          onClick={() => onTriggerAction('/do?wake=1')}
          className="btn-action"
          style={{ justifyContent: 'center', padding: '12px', background: 'rgba(255,255,255,0.03)' }}
        >
          <Sun size={15} color="#38bdf8" />
          <span>Wake Up</span>
        </button>
      </div>
    </div>
  )
}
