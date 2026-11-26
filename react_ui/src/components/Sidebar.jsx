import React from 'react'
import { LayoutDashboard, Smile, Settings, UploadCloud, Sparkles } from 'lucide-react'
import { Tooltip } from './Tooltip'
import KiwiLogo from './KiwiLogo'

const NAV_ITEMS = [
  { id: 'live', label: 'Dashboard', icon: LayoutDashboard, badge: '3' },
  { id: 'faces', label: 'Faces & Studio', icon: Smile },
  { id: 'settings', label: 'Settings & AI', icon: Settings },
  { id: 'ota', label: 'OTA Firmware', icon: UploadCloud }
]

export default function Sidebar({ activeTab, onTabChange, status, connected, onTriggerAction }) {
  const isSleeping = status?.sleeping

  return (
    <aside className="flux-sidebar">
      {/* Brand Logo */}
      <div>
        <div className="flux-brand">
          <div className="flux-brand-logo">
            <KiwiLogo size={34} />
          </div>
          <span className="flux-brand-text">kiwi</span>
        </div>

        {/* Navigation List matching image */}
        <nav className="flux-nav-list">
          {NAV_ITEMS.map((item) => {
            const Icon = item.icon
            const isActive = activeTab === item.id
            return (
              <button
                key={item.id}
                onClick={() => onTabChange(item.id)}
                className={`flux-nav-item ${isActive ? 'active' : ''}`}
              >
                <div className="flux-nav-item-content">
                  <Icon size={18} />
                  <span>{item.label}</span>
                </div>
                {item.badge && isActive && (
                  <span className="flux-badge-lime">{item.badge}</span>
                )}
              </button>
            )
          })}
        </nav>
      </div>

      {/* Bottom Promo Card matching 'Upgrade to Pro' in image */}
      <div className="flux-sidebar-promo">
        <div style={{
          display: 'inline-flex', alignItems: 'center', gap: '6px',
          background: 'rgba(56, 189, 248, 0.15)', color: '#38BDF8',
          padding: '2px 8px', borderRadius: '999px', fontSize: '10px', fontWeight: 800,
          marginBottom: '8px'
        }}>
          <Sparkles size={11} />
          <span>{connected ? 'NEURAL LINK ACTIVE' : 'CONNECTING...'}</span>
        </div>

        <h4 className="flux-promo-title">Kiwi AI Companion</h4>
        <p className="flux-promo-sub">
          {connected ? 'Hardware connected and running in low-latency mode.' : 'Connecting to local robot core...'}
        </p>

        <Tooltip
          title={isSleeping ? 'Wake Up' : 'Sleep Mode'}
          placement="top"
        >
          <button
            onClick={() => onTriggerAction(`/do?sleep=${isSleeping ? '0' : '1'}`)}
            className="flux-promo-btn"
          >
            {isSleeping ? 'Wake Up' : 'Put to Sleep'}
          </button>
        </Tooltip>
      </div>
    </aside>
  )
}
