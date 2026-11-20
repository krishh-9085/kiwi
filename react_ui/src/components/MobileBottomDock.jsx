import React from 'react'
import { LayoutDashboard, Smile, Settings, UploadCloud } from 'lucide-react'

export default function MobileBottomDock({
  activeTab,
  onTabChange,
  isListening,
  onMicClick
}) {
  return (
    <nav className="mobile-bottom-dock" aria-label="Mobile Navigation Dock">
      <button
        onClick={() => onTabChange('live')}
        className={`mobile-dock-btn ${activeTab === 'live' ? 'active' : ''}`}
        aria-label="Dashboard"
      >
        <LayoutDashboard size={20} />
        <span className="mobile-dock-label">Home</span>
      </button>

      <button
        onClick={() => onTabChange('faces')}
        className={`mobile-dock-btn ${activeTab === 'faces' ? 'active' : ''}`}
        aria-label="Faces & Studio"
      >
        <Smile size={20} />
        <span className="mobile-dock-label">Faces</span>
      </button>

      {/* Floating Glowing Center Voice Button matching image */}
      <button
        type="button"
        onClick={onMicClick}
        className={`mobile-dock-mic ${isListening ? 'is-recording' : ''}`}
        aria-label={isListening ? "Cancel Voice" : "Voice AI Assistant"}
      >
        <div className="mobile-mic-glow" />
        <div className="mobile-mic-inner">
          {isListening ? (
            <svg
              xmlns="http://www.w3.org/2000/svg"
              width="28"
              height="28"
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
              width="28"
              height="28"
              viewBox="0 0 24 24"
              fill="none"
              style={{ display: 'block' }}
            >
              <path
                fill="none"
                stroke="var(--accent-cyan, #0EA5E9)"
                strokeLinecap="round"
                strokeLinejoin="round"
                strokeWidth={2.4}
                d="M3.75 10.5v3m2.75-6v9m5.5-8v7M17.5 7v10m2.75-7v4m-11-9.25v14.5m5.5-15.5v16.5"
              />
            </svg>
          )}
        </div>
      </button>

      <button
        onClick={() => onTabChange('settings')}
        className={`mobile-dock-btn ${activeTab === 'settings' ? 'active' : ''}`}
        aria-label="Settings"
      >
        <Settings size={20} />
        <span className="mobile-dock-label">Settings</span>
      </button>

      <button
        onClick={() => onTabChange('ota')}
        className={`mobile-dock-btn ${activeTab === 'ota' ? 'active' : ''}`}
        aria-label="OTA Firmware"
      >
        <UploadCloud size={20} />
        <span className="mobile-dock-label">OTA</span>
      </button>
    </nav>
  )
}
