import React from 'react'
import { LayoutDashboard, Smile, Settings, UploadCloud } from 'lucide-react'

const LEFT_TABS = [
  { id: 'live', label: 'Home', ariaLabel: 'Dashboard', icon: LayoutDashboard },
  { id: 'faces', label: 'Faces', ariaLabel: 'Faces & Studio', icon: Smile }
]

const RIGHT_TABS = [
  { id: 'settings', label: 'Settings', ariaLabel: 'Settings', icon: Settings },
  { id: 'ota', label: 'OTA', ariaLabel: 'OTA Firmware', icon: UploadCloud }
]

function DockTabButton({ item, active, onSelect }) {
  const Icon = item.icon
  return (
    <button
      type="button"
      onClick={() => onSelect(item.id)}
      className={`mobile-dock-btn ${active ? 'active' : ''}`}
      aria-label={item.ariaLabel}
    >
      <Icon size={18} strokeWidth={active ? 2.3 : 1.9} />
      <span className="mobile-dock-label">{item.label}</span>
    </button>
  )
}

export default function MobileBottomDock({
  activeTab,
  onTabChange,
  isListening,
  onMicClick
}) {
  return (
    <nav className="mobile-bottom-dock" aria-label="Mobile Navigation Dock">
      {LEFT_TABS.map((item) => (
        <DockTabButton
          key={item.id}
          item={item}
          active={activeTab === item.id}
          onSelect={onTabChange}
        />
      ))}

      {/* Inline Elevated Center Voice Trigger */}
      <button
        type="button"
        onClick={onMicClick}
        className={`mobile-dock-mic ${isListening ? 'is-recording' : ''}`}
        aria-label={isListening ? 'Cancel Voice' : 'Voice AI Assistant'}
      >
        <div className="mobile-mic-inner">
          {isListening ? (
            <svg
              xmlns="http://www.w3.org/2000/svg"
              width="22"
              height="22"
              viewBox="0 0 24 24"
              fill="none"
              style={{ display: 'block' }}
            >
              <path
                fill="none"
                stroke="#FFFFFF"
                strokeLinecap="round"
                strokeLinejoin="round"
                strokeWidth={2.2}
                d="m2.63 2.63l18.74 18.74M1.75 15.4V8.6m20.5 6.8V8.6m-8.2 6.53v-1m0-3.61V8.87m-4.1 8.45V10m-4.1 9.61V6.05m12.3 8.58V4.39m0 15.22V18.4"
              />
            </svg>
          ) : (
            <svg
              xmlns="http://www.w3.org/2000/svg"
              width="22"
              height="22"
              viewBox="0 0 24 24"
              fill="none"
              style={{ display: 'block' }}
            >
              <path
                fill="none"
                stroke="#FFFFFF"
                strokeLinecap="round"
                strokeLinejoin="round"
                strokeWidth={2.4}
                d="M3.75 10.5v3m2.75-6v9m5.5-8v7M17.5 7v10m2.75-7v4m-11-9.25v14.5m5.5-15.5v16.5"
              />
            </svg>
          )}
        </div>
      </button>

      {RIGHT_TABS.map((item) => (
        <DockTabButton
          key={item.id}
          item={item}
          active={activeTab === item.id}
          onSelect={onTabChange}
        />
      ))}
    </nav>
  )
}
