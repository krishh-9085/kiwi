import React, { useState, useEffect, useMemo } from 'react'

import KiwiEyes from './KiwiEyes'
import KiwiLogo from './KiwiLogo'

import { Tooltip } from './base/tooltip/tooltip'

import WakeSlider from './ui/WakeSlider'

import {

  Heart, Zap, Activity, Moon, Volume2, Sun,

  Mic, MicOff, Send,

  Utensils, Coffee, Hand, Eye, Music, Radio, Shield, Sparkles,

  Clock, BellRing, Sparkle, ArrowDown, RotateCw, Settings as SettingsIcon, Check,

  Bot, Smile, Cpu, Wifi, WifiOff

} from 'lucide-react'

import FluxSelect from './FluxSelect'
import Esp32LedColorPicker from './Esp32LedColorPicker'

import { OLED_FACES, SKIN_FACES } from '../data/faceData'



const QUICK_EMOTIONS = [

  { id: 'happy', label: 'Happy' },

  { id: 'wink', label: 'Wink' },

  { id: 'hearts', label: 'Hearts' },

  { id: 'curious', label: 'Curious' },

  { id: 'cool', label: 'Cool' },

  { id: 'excited', label: 'Excited' },

  { id: 'laughing', label: 'Laugh' },

  { id: 'party', label: 'Party' }

]



const QUICK_SKINS = [

  { id: 'kiwi', label: 'Kiwi' },

  { id: 'vector', label: 'Vector' },

  { id: 'mecha', label: 'Mecha' },

  { id: 'pixel', label: 'Pixel' }

]



const SOUND_EFFECTS = [

  { id: 'wake', label: 'Wake Chime' },

  { id: '1', label: 'Pet Giggle' },

  { id: '3', label: 'Sparkle' },

  { id: 'laugh', label: 'Joy Laugh' }

]







const BOREDOM_INTERVAL_OPTIONS = [

  { val: '1', lbl: 'Every 1m (Lively)' },

  { val: '2', lbl: 'Every 2m (Default)' },

  { val: '3', lbl: 'Every 3m' },

  { val: '5', lbl: 'Every 5m' },

  { val: '10', lbl: 'Every 10m' },

  { val: '15', lbl: 'Every 15m' },

  { val: '30', lbl: 'Every 30m' }

]



const MOBILE_BOREDOM_OPTIONS = [
  { val: '1', lbl: '1m' },
  { val: '2', lbl: '2m' },
  { val: '3', lbl: '3m' },
  { val: '5', lbl: '5m' },
  { val: '10', lbl: '10m' },
  { val: '15', lbl: '15m' },
  { val: '30', lbl: '30m' }
]




const HOUR_OPTIONS = Array.from({ length: 24 }).map((_, i) => ({
  val: i,
  lbl: i === 0 ? '12 AM' : i < 12 ? `${i} AM` : i === 12 ? '12 PM' : `${i - 12} PM`
}))



export default function LiveConsoleView({

  status,

  config,

  connected = true,

  onTriggerAction,

  onToast,

  onTabChange,

  onRefresh

}) {

  const [vol, setVol] = useState(70)

  const [bright, setBright] = useState(90)

  const [rgbBright, setRgbBright] = useState(65)

  const [promptText, setPromptText] = useState('')

  const [sendingPrompt, setSendingPrompt] = useState(false)



  // Sync state from status / config (snap to clean 5-by-5 increments)

  useEffect(() => {

    if (status?.volume != null) setVol(Math.round(status.volume / 5) * 5)

    else if (config?.volume != null) setVol(Math.round(config.volume / 5) * 5)



    if (status?.brightness != null) setBright(Math.max(5, Math.round(status.brightness / 5) * 5))

    else if (config?.brightness != null) setBright(Math.max(5, Math.round(config.brightness / 5) * 5))



    if (status?.rgbLed?.brightness != null) setRgbBright(Math.round(status.rgbLed.brightness / 5) * 5)

    else if (config?.rgbLed?.brightness != null) setRgbBright(Math.round(config.rgbLed.brightness / 5) * 5)

  }, [status, config])



  const isBusy = status?.busy

  const isListening = status?.state?.toLowerCase().includes('listen') || status?.emotion === 'listening'

  const isSpeaking = status?.state?.toLowerCase().includes('speak') || status?.emotion === 'speaking'

  const [localSleeping, setLocalSleeping] = useState(null)

  const serverSleeping = Boolean(
    status?.sleeping === true ||
    status?.sleeping === 1 ||
    status?.sleeping === 'true' ||
    status?.sleeping === '1' ||
    status?.emotion === 'sleeping' ||
    status?.state === 'sleep' ||
    status?.state === 'sleeping'
  )

  const isSleeping = localSleeping !== null ? localSleeping : serverSleeping

  const currentEmotion = status?.emotion || 'idle'

  const currentSkin = status?.skin || 'kiwi'

  const hunger = status?.hunger || 'satisfied'

  const meal = status?.meal || 'Snack'

  const ownerName = status?.owner || config?.memory?.name || config?.ownerName || config?.mem_name || 'Friend'

  const hostName = status?.host || 'kiwi.local'



  const [localGreetings, setLocalGreetings] = useState(null)
  const [localWwEnabled, setLocalWwEnabled] = useState(null)
  const [localWwModel, setLocalWwModel] = useState(null)
  const [localBoredomEnabled, setLocalBoredomEnabled] = useState(null)
  const [localBoredomMins, setLocalBoredomMins] = useState(null)

  useEffect(() => {
    if (status?.greetings?.enabled != null) setLocalGreetings(status.greetings.enabled)
    else if (config?.greetings?.enabled != null && localGreetings === null) setLocalGreetings(config.greetings.enabled)

    if (status?.wakeWord?.enabled != null) setLocalWwEnabled(status.wakeWord.enabled)
    else if (config?.wakeWord?.enabled != null && localWwEnabled === null) setLocalWwEnabled(config.wakeWord.enabled)

    if (status?.wakeWord?.model != null) setLocalWwModel(String(status.wakeWord.model))
    else if (config?.wakeWord?.model != null && localWwModel === null) setLocalWwModel(String(config.wakeWord.model))

    if (status?.boredom != null) setLocalBoredomEnabled(status.boredom)
    else if (config?.boredom?.enabled != null && localBoredomEnabled === null) setLocalBoredomEnabled(config.boredom.enabled)

    if (status?.boredom_mins != null) setLocalBoredomMins(String(status.boredom_mins))
    else if (config?.boredom?.minutes != null && localBoredomMins === null) setLocalBoredomMins(String(config.boredom.minutes))

    if (status?.sleeping != null) setLocalSleeping(null)
  }, [status, config])

  const handleSleepToggle = async () => {
    const nextSleeping = !isSleeping
    setLocalSleeping(nextSleeping)
    if (onTriggerAction) {
      await onTriggerAction(`/do?sleep=${nextSleeping ? '1' : '0'}`)
    }
    if (onToast) {
      onToast(nextSleeping ? 'Putting Kiwi to sleep...' : 'Waking Kiwi up...')
    }
  }

  // Wake Word state
  const wwEnabled = localWwEnabled ?? status?.wakeWord?.enabled ?? config?.wakeWord?.enabled ?? true
  const wwModel = String(localWwModel ?? status?.wakeWord?.model ?? config?.wakeWord?.model ?? 0)

  const getWakeWordName = () => 'Alexa'

  const currentWakeWordName = status?.wakeWord?.name || getWakeWordName(wwModel)

  // Boredom state
  const boredomEnabled = localBoredomEnabled ?? status?.boredom ?? config?.boredom?.enabled ?? true
  const boredomMins = String(localBoredomMins ?? status?.boredom_mins ?? config?.boredom?.minutes ?? 2)

  // Greetings state (CRITICAL)
  const greetingsEnabled = localGreetings ?? status?.greetings?.enabled ?? config?.greetings?.enabled ?? true

  // Circadian state
  const circEnabled = status?.circadian?.enabled ?? config?.circadian?.enabled ?? true

  const circStart = String(config?.circadian?.start ?? 23)

  const circEnd = String(config?.circadian?.end ?? 7)



  // Night Mode state (0: Same, 1: Dim, 2: Screen off)

  const nightMode = status?.nightMode ?? config?.nightMode ?? 0



  // Sound Reactive state

  const sndReactEnabled = status?.soundReactive?.enabled ?? config?.soundReactive?.enabled ?? false



  // RGB LED state

  const rgbEnabled = status?.rgbLed?.enabled ?? config?.rgbLed?.enabled ?? true



  const handleMicToggle = async () => {
    if (status?.busy || status?.emotion === 'listening' || status?.state?.toLowerCase().includes('listen')) {
      await onTriggerAction('/do?record=0')
      onToast('Listening stopped')
    } else {
      await onTriggerAction('/do?record=1')
      onToast('Kiwi is listening...')
    }
  }



  const handleSendPrompt = async (textToSend) => {

    const q = (textToSend || promptText).trim()

    if (!q) return

    setSendingPrompt(true)

    await onTriggerAction(`/do?ask=${encodeURIComponent(q)}`)

    setPromptText('')

    setSendingPrompt(false)

    onToast(`Sent prompt: "${q}"`)

  }



  const handleWakeWordToggle = async (checked) => {
    setLocalWwEnabled(checked)
    await onTriggerAction(`/do?ww_en=${checked ? '1' : '0'}`)
    onToast(`Wake Word ${checked ? 'Activated' : 'Disabled'}`)
  }

  const handleBoredomToggle = async (checked) => {
    setLocalBoredomEnabled(checked)
    await onTriggerAction(`/do?boredom=${checked ? '1' : '0'}`)
    onToast(`Autonomous Idle Chatter ${checked ? 'Enabled' : 'Disabled'}`)
  }

  const handleBoredomInterval = async (mins) => {
    setLocalBoredomMins(String(mins))
    await onTriggerAction(`/do?boredom_mins=${mins}`)
    onToast(`Idle interval set to every ${mins} minute(s)`)
  }

  const handleGreetingsToggle = async (checked) => {
    setLocalGreetings(checked)
    await onTriggerAction(`/do?greet_en=${checked ? '1' : '0'}`)
    onToast(`Greetings ${checked ? 'Enabled' : 'Disabled'}`)
  }

  const handleCircadianToggle = async (checked) => {

    await onTriggerAction(`/do?circ_en=${checked ? '1' : '0'}`)

    onToast(`Sleep Schedule ${checked ? 'Enabled' : 'Disabled'}`)

  }



  const handleCircStartChange = async (val) => {

    await onTriggerAction(`/do?circ_start=${val}`)

    onToast(`Sleep time set to ${val}:00`)

  }



  const handleCircEndChange = async (val) => {

    await onTriggerAction(`/do?circ_end=${val}`)

    onToast(`Wake time set to ${val}:00`)

  }



  const handleSoundReactToggle = async (checked) => {

    await onTriggerAction(`/do?snd_react=${checked ? '1' : '0'}`)

    onToast(`Sound-reactive face ${checked ? 'Enabled' : 'Disabled'}`)

  }



  const handleNightModeChange = async (mode) => {

    await onTriggerAction(`/do?night_mode=${mode}`)

    onToast(`Night Mode set to ${mode === 0 ? 'Same' : (mode === 1 ? 'Dim' : 'Screen Off')}`)

  }



  const handleRgbToggle = async (checked) => {

    await onTriggerAction(`/do?rgb_en=${checked ? '1' : '0'}`)

    onToast(`NeoPixel LED ${checked ? 'Enabled' : 'Disabled'}`)

  }



  let statusBadge = 'READY'

  if (isListening) statusBadge = 'LISTENING'
  else if (isSpeaking) statusBadge = 'SPEAKING'
  else if (isBusy) statusBadge = 'THINKING'
  else if (isSleeping) statusBadge = 'SLEEPING'

  return (

    <div>

      {/* ================= DESKTOP VIEWPORT (Visible ONLY on > 768px) ================= */}

      <div className="desktop-console-layout">

        {/* 3 EQUALLY DIVIDED COLUMNS GRID */}
        <div className="desktop-3col-grid">

          {/* ================= COLUMN 1: KIWI CORE & EYES ================= */}

          <div className="flux-white-card" style={{ justifyContent: 'space-between' }}>

            <div>

              <div className="flux-card-header">

                <span className="flux-card-title">

                  <Zap size={15} />

                  <span>Kiwi Live Console</span>

                </span>

                <span style={{

                  background: isSleeping ? '#8E8E93' : 'var(--accent-lime)',

                  color: '#16181D',

                  fontSize: '11px',

                  fontWeight: 800,

                  padding: '2px 8px',

                  borderRadius: '999px'

                }}>

                  {statusBadge}

                </span>

              </div>



              {/* Desktop Vitals Deck (Live Hardware Telemetry) */}
              {(() => {
                const freeKb = status?.heap?.free ? Math.round(status.heap.free / 1024) : 178;
                const totalKb = status?.heap?.total ? Math.round(status.heap.total / 1024) : 320;
                const ramUsedPct = status?.heap?.pct ?? (freeKb ? Math.round(((totalKb - freeKb) / totalKb) * 100) : 44);
                const cpuTemp = status?.temp != null ? Math.round(status.temp) : 46;

                // Flash storage calculations
                const flashUsed = status?.flash?.used ?? 1648200;
                const flashFree = status?.flash?.free ?? 383416;
                const flashTotal = status?.flash?.total ?? (flashUsed + flashFree);
                const flashPct = status?.flash?.pct ?? Math.round((flashUsed / flashTotal) * 100);
                const flashFreeMb = (flashFree / (1024 * 1024)).toFixed(2);
                const flashUsedMb = (flashUsed / (1024 * 1024)).toFixed(2);
                const flashTotalMb = (flashTotal / (1024 * 1024)).toFixed(1);
                const flashChipMb = status?.flash?.chip ? Math.round(status.flash.chip / (1024 * 1024)) : 4;

                return (
                  <div style={{
                    background: 'linear-gradient(145deg, #181B24 0%, #101218 100%)',
                    borderRadius: '18px',
                    padding: '16px 18px',
                    color: '#FFFFFF',
                    border: '1px solid rgba(255, 255, 255, 0.08)',
                    boxShadow: '0 8px 24px rgba(0, 0, 0, 0.22)',
                    margin: '14px 0 16px',
                    position: 'relative',
                    overflow: 'hidden'
                  }}>
                    {/* Header */}
                    <div style={{ display: 'flex', alignItems: 'center', justifyContent: 'space-between', width: '100%' }}>
                      <span style={{ fontSize: '12px', opacity: 0.9, fontWeight: 700, letterSpacing: '0.04em', textTransform: 'uppercase' }}>
                        Vitals
                      </span>
                      <span style={{
                        fontSize: '9px',
                        fontWeight: 800,
                        background: 'rgba(255, 255, 255, 0.18)',
                        padding: '2px 8px',
                        borderRadius: '999px',
                        letterSpacing: '0.04em',
                        whiteSpace: 'nowrap'
                      }}>
                        ESP32-S3
                      </span>
                    </div>

                    {/* Stats Grid */}
                    <div style={{ display: 'grid', gridTemplateColumns: '85px 1fr 1fr', gap: '14px', marginTop: '12px', alignItems: 'center' }}>
                      {/* CPU Core Temp */}
                      <div>
                        <span style={{ fontSize: '11px', opacity: 0.8, fontWeight: 600, display: 'block' }}>CPU Temp</span>
                        <span style={{ fontSize: '24px', fontWeight: 800, letterSpacing: '-0.02em', color: '#FFFFFF' }}>
                          {cpuTemp}°<span style={{ fontSize: '13px', fontWeight: 600, opacity: 0.85 }}>C</span>
                        </span>
                      </div>

                      {/* RAM / Heap Usage */}
                      <div>
                        <div style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'baseline', marginBottom: '3px' }}>
                          <span style={{ fontSize: '11px', opacity: 0.8, fontWeight: 600 }}>RAM Usage</span>
                          <span style={{ fontSize: '11px', fontWeight: 700, color: '#38BDF8' }}>
                            {freeKb} KB <span style={{ opacity: 0.75, fontWeight: 500, color: '#FFFFFF' }}>free</span>
                          </span>
                        </div>
                        {/* Progress Bar */}
                        <div style={{
                          width: '100%',
                          height: '6px',
                          borderRadius: '3px',
                          background: 'rgba(255, 255, 255, 0.2)',
                          overflow: 'hidden'
                        }}>
                          <div style={{
                            height: '100%',
                            width: `${Math.min(100, Math.max(0, ramUsedPct))}%`,
                            background: ramUsedPct > 85 ? '#F43F5E' : '#38BDF8',
                            borderRadius: '3px',
                            transition: 'width 0.4s ease'
                          }} />
                        </div>
                        <div style={{ display: 'flex', justifyContent: 'space-between', marginTop: '3px' }}>
                          <span style={{ fontSize: '9px', opacity: 0.7 }}>{ramUsedPct}% used</span>
                          <span style={{ fontSize: '9px', opacity: 0.7 }}>{totalKb} KB total</span>
                        </div>
                      </div>

                      {/* Flash Storage Usage */}
                      <div>
                        <div style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'baseline', marginBottom: '3px' }}>
                          <span style={{ fontSize: '11px', opacity: 0.8, fontWeight: 600 }}>Flash Storage</span>
                          <span style={{ fontSize: '11px', fontWeight: 700, color: '#A78BFA' }}>
                            {flashFreeMb} MB <span style={{ opacity: 0.75, fontWeight: 500, color: '#FFFFFF' }}>free</span>
                          </span>
                        </div>
                        {/* Progress Bar */}
                        <div style={{
                          width: '100%',
                          height: '6px',
                          borderRadius: '3px',
                          background: 'rgba(255, 255, 255, 0.2)',
                          overflow: 'hidden'
                        }}>
                          <div style={{
                            height: '100%',
                            width: `${Math.min(100, Math.max(0, flashPct))}%`,
                            background: flashPct > 90 ? '#F43F5E' : '#A78BFA',
                            borderRadius: '3px',
                            transition: 'width 0.4s ease'
                          }} />
                        </div>
                        <div style={{ display: 'flex', justifyContent: 'space-between', marginTop: '3px' }}>
                          <span style={{ fontSize: '9px', opacity: 0.7 }}>{flashPct}% used ({flashUsedMb}M)</span>
                          <span style={{ fontSize: '9px', opacity: 0.7 }}>{flashTotalMb}M ({flashChipMb}M Chip)</span>
                        </div>
                      </div>
                    </div>
                  </div>
                );
              })()}



              {/* Prompt Box + Glowing Mic */}
              <form onSubmit={(e) => { e.preventDefault(); handleSendPrompt(); }} style={{ display: 'flex', alignItems: 'center', gap: '8px', marginBottom: '12px' }}>
                <input
                  type="text"
                  value={promptText}
                  onChange={(e) => setPromptText(e.target.value)}
                  placeholder={isListening ? 'Listening...' : 'Ask Kiwi anything...'}
                  style={{
                    flex: 1,
                    height: '38px',
                    boxSizing: 'border-box',
                    padding: '0 16px',
                    borderRadius: '999px',
                    background: 'var(--card-muted)',
                    border: '1px solid var(--border-light)',
                    color: 'var(--text-dark)',
                    fontSize: '13px',
                    outline: 'none'
                  }}
                />
                <button
                  type="submit"
                  disabled={sendingPrompt || !promptText.trim()}
                  className="flux-btn"
                  style={{
                    width: '38px',
                    height: '38px',
                    padding: 0,
                    borderRadius: '50%',
                    display: 'flex',
                    alignItems: 'center',
                    justifyContent: 'center',
                    flexShrink: 0
                  }}
                >
                  <Send size={14} />
                </button>
              </form>



              {/* Last Conversation Bubble */}

              {(status?.question || status?.answer) && (

                <div style={{

                  background: 'var(--card-subtle)', border: '1px solid var(--border-light)',

                  borderRadius: '12px', padding: '10px 12px', marginBottom: '14px',

                  fontSize: '12px', display: 'flex', flexDirection: 'column', gap: '3px'

                }}>

                  {status.question && (

                    <div><strong style={{ color: 'var(--text-muted)' }}>You:</strong> {status.question}</div>

                  )}

                  {status.answer && (

                    <div><strong style={{ color: 'var(--text-dark)' }}>Kiwi:</strong> {status.answer}</div>

                  )}

                </div>

              )}

              {/* Desktop WS2812 RGB LED (Pin 48) HeroUI ColorPicker */}
              <div style={{ marginTop: '14px', marginBottom: '8px' }}>
                <Esp32LedColorPicker
                  onColorChange={(hex) => onTriggerAction(`/do?rgb_color=${encodeURIComponent(hex.replace('#', ''))}`)}
                  onClearOverride={() => onTriggerAction('/do?rgb_clear=1')}
                  initialColor="#38BDF8"
                  currentEmotion={status?.emotion}
                  rgbEnabled={status?.rgbLed?.enabled ?? true}
                  showTitle={false}
                  showHex={false}
                />
              </div>

            </div>



            {/* Telemetry Progress Bars */}

            <div style={{ borderTop: '1px solid var(--border-subtle)', paddingTop: '12px', marginTop: '10px' }}>

              <div className="flux-progress-row">

                <span className="flux-progress-val">{status?.heap?.pct != null ? `${100 - status.heap.pct}%` : '55%'}</span>

                <div className="flux-progress-track">

                  <div className="flux-progress-fill-purple" style={{ width: status?.heap?.pct != null ? `${100 - status.heap.pct}%` : '55%' }} />

                </div>

                <span className="flux-progress-label">Free Heap</span>

              </div>



              <div className="flux-progress-row">

                <span className="flux-progress-val">85%</span>

                <div className="flux-progress-track">

                  <div className="flux-progress-fill-dark" style={{ width: '85%' }} />

                </div>

                <span className="flux-progress-label">Wi-Fi Signal</span>

              </div>



              <div className="flux-progress-row">

                <span className="flux-progress-val">{vol}%</span>

                <div className="flux-progress-track">

                  <div className="flux-progress-fill-lime" style={{ width: `${vol}%` }} />

                </div>

                <span className="flux-progress-label">Volume</span>

              </div>

            </div>

          </div>



          {/* ================= COLUMN 2: WAKE WORD, SPEAKER & LIGHTS ================= */}

          <div className="flux-white-card" style={{ justifyContent: 'space-between' }}>

            <div>

              {/* Header: Wake Word & Audio Sensing */}

              <div className="flux-card-header">

                <span className="flux-card-title">

                  <Radio size={15} />

                  <span>Wake Word & Sensing</span>

                </span>

                <span style={{

                  background: wwEnabled ? 'rgba(52,199,89,0.15)' : '#F3F4F6',

                  color: wwEnabled ? '#15803D' : 'var(--text-muted)',

                  fontSize: '10px',

                  fontWeight: 800,

                  padding: '2px 8px',

                  borderRadius: '999px'

                }}>

                  {wwEnabled ? 'ACTIVE' : 'DISABLED'}

                </span>

              </div>



              {/* Wake Word Controls Card Panel (UNTITLED UI ROUNDED DROPDOWN) */}

              <div style={{

                background: 'var(--card-subtle)',

                border: '1px solid var(--border-light)',

                borderRadius: '14px',

                padding: '12px 14px',

                marginBottom: '16px'

              }}>

                <div style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'center', marginBottom: '10px' }}>

                  <label className="flux-toggle">

                    <input

                      type="checkbox"

                      checked={wwEnabled}

                      onChange={(e) => handleWakeWordToggle(e.target.checked)}

                    />

                    <span className="flux-toggle-slider" />

                    <span style={{ fontSize: '13px', fontWeight: 700, color: 'var(--text-dark)' }}>

                      Wake Word ("{currentWakeWordName}")

                    </span>

                  </label>

                </div>



                {/* Wake Word Model Badge */}
                <div style={{ display: 'flex', alignItems: 'center', gap: '8px' }}>
                  <label style={{ fontSize: '11px', fontWeight: 600, color: 'var(--text-muted)', flexShrink: 0 }}>
                    Model:
                  </label>
                  <span style={{
                    fontSize: '11px',
                    fontWeight: 700,
                    color: 'var(--text-dark)',
                    background: 'var(--card-subtle)',
                    border: '1px solid var(--border-light)',
                    padding: '3px 9px',
                    borderRadius: '6px'
                  }}>
                    Alexa
                  </span>
                </div>



                <div style={{ fontSize: '11px', color: 'var(--text-muted)', marginTop: '8px', lineHeight: 1.4 }}>

                  Bina touch kare <strong style={{ color: 'var(--text-dark)' }}>"{currentWakeWordName}"</strong> bol kar Kiwi ko activate karein.

                </div>

              </div>



              {/* Hardware Sliders: Volume & OLED Brightness (React Bits WakeSlider) */}

              <div style={{ display: 'flex', flexDirection: 'column', gap: '14px', marginBottom: '16px' }}>

                <div>

                  <div style={{ display: 'flex', justifyContent: 'space-between', fontSize: '12px', marginBottom: '6px' }}>

                    <span style={{ color: 'var(--text-muted)', fontWeight: 600 }}>Speaker Volume:</span>

                    <strong style={{ color: 'var(--text-dark)' }}>{vol}%</strong>

                  </div>

                  <WakeSlider
                    value={vol}
                    min={0}
                    max={100}
                    step={5}
                    bars={20}
                    height={32}
                    restHeight={16}
                    gap={4}
                    fillColor="var(--slider-fill)"
                    trackColor="var(--slider-track)"
                    crestColor="#D2FA4B"
                    onChange={(v) => setVol(v)}
                    onCommit={(v) => onTriggerAction(`/do?vol=${v}`)}
                    ariaLabel="Speaker Volume"
                  />
                </div>

                <div>
                  <div style={{ display: 'flex', justifyContent: 'space-between', fontSize: '12px', marginBottom: '6px' }}>
                    <span style={{ color: 'var(--text-muted)', fontWeight: 600 }}>OLED Brightness:</span>
                    <strong style={{ color: 'var(--text-dark)' }}>{bright}%</strong>
                  </div>
                  <WakeSlider
                    value={bright}
                    min={5}
                    max={100}
                    step={5}
                    bars={20}
                    height={32}
                    restHeight={16}
                    gap={4}
                    fillColor="#0ea5e9"
                    trackColor="var(--slider-track)"
                    crestColor="#38bdf8"
                    onChange={(v) => setBright(v)}
                    onCommit={(v) => onTriggerAction(`/do?brightness=${v}`)}
                    ariaLabel="OLED Brightness"
                  />
                </div>

                {/* Pin 48 NeoPixel RGB LED */}
                <div style={{
                  background: 'var(--card-subtle)',
                  border: '1px solid var(--border-light)',
                  borderRadius: '12px',
                  padding: '10px 12px'
                }}>
                  <div style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'center', marginBottom: '8px' }}>
                    <label className="flux-toggle">
                      <input
                        type="checkbox"
                        checked={rgbEnabled}
                        onChange={(e) => handleRgbToggle(e.target.checked)}
                      />
                      <span className="flux-toggle-slider" />
                      <span style={{ fontSize: '12px', fontWeight: 600, color: 'var(--text-dark)' }}>
                        Pin 48 NeoPixel RGB LED
                      </span>
                    </label>
                    <span style={{ fontSize: '11px', fontWeight: 700, color: 'var(--text-muted)' }}>
                      {rgbBright}%
                    </span>
                  </div>
                  <WakeSlider
                    value={rgbBright}
                    min={0}
                    max={100}
                    step={5}
                    bars={20}
                    height={32}
                    restHeight={16}
                    gap={4}
                    disabled={!rgbEnabled}

                    fillColor="#10B981"

                    trackColor="var(--slider-track)"

                    crestColor="#34D399"

                    onChange={(v) => setRgbBright(v)}

                    onCommit={(v) => onTriggerAction(`/do?rgb_bright=${v}`)}

                    ariaLabel="NeoPixel RGB Brightness"

                  />

                </div>

              </div>

            </div>



            {/* Quick Sound Effects Soundboard */}

            <div style={{ borderTop: '1px solid var(--border-subtle)', paddingTop: '12px' }}>

              <span style={{ fontSize: '11px', color: 'var(--text-muted)', display: 'block', marginBottom: '6px', fontWeight: 600 }}>

                Audio Soundboard:

              </span>

              <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr', gap: '6px' }}>

                {SOUND_EFFECTS.map((snd) => (

                  <button

                    key={snd.id}

                    onClick={() => onTriggerAction(`/do?sound=${snd.id}`)}

                    className="flux-btn"

                    style={{ padding: '6px 8px', fontSize: '11px', background: 'var(--card-muted)', color: 'var(--text-dark)', border: '1px solid var(--border-light)', justifyContent: 'center' }}

                  >

                    <Music size={11} />

                    <span>{snd.label}</span>

                  </button>

                ))}

              </div>

            </div>

          </div>



          {/* ================= COLUMN 3: PET MOOD, GREETINGS & SKINS ================= */}

          <div className="flux-white-card" style={{ justifyContent: 'space-between' }}>

            <div>

              <div className="flux-card-header">

                <span className="flux-card-title">

                  <Heart size={15} color="#F43F5E" />

                  <span>Pet Engine & Mood</span>

                </span>

                <span style={{ fontSize: '11px', color: 'var(--text-muted)' }}>

                  Target: <strong>{meal}</strong>

                </span>

              </div>



              <div style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'baseline', marginBottom: '10px' }}>

                <div className="flux-huge-metric">

                  <span style={{ textTransform: 'capitalize' }}>{hunger}</span>

                </div>

                <span style={{

                  background: hunger === 'satisfied' ? 'var(--accent-lime)' : '#F59E0B',

                  color: '#16181D', fontSize: '10px', fontWeight: 800, padding: '2px 8px', borderRadius: '999px'

                }}>

                  {hunger === 'satisfied' ? 'WELL FED' : 'NEEDS CARE'}

                </span>

              </div>



              {/* 4 Pet Action Buttons (Feed, Hungry, Apologize, Pat) */}

              <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr', gap: '6px', marginBottom: '14px' }}>

                <button

                  onClick={() => onTriggerAction('/do?feed=1')}

                  className="flux-btn"

                  style={{ padding: '8px 10px', fontSize: '11px', background: 'var(--card-muted)', color: 'var(--text-dark)', border: '1px solid var(--border-light)', justifyContent: 'center' }}

                >

                  <Utensils size={12} />

                  <span>Feed Snack</span>

                </button>



                <button

                  onClick={() => onTriggerAction('/do?hunger=1')}

                  className="flux-btn"

                  style={{ padding: '8px 10px', fontSize: '11px', background: 'var(--card-muted)', color: 'var(--text-dark)', border: '1px solid var(--border-light)', justifyContent: 'center' }}

                >

                  <Coffee size={12} />

                  <span>Make Hungry</span>

                </button>



                <button

                  onClick={() => onTriggerAction('/do?apologize=1')}

                  className="flux-btn"

                  style={{ padding: '8px 10px', fontSize: '11px', background: 'var(--card-muted)', color: 'var(--text-dark)', border: '1px solid var(--border-light)', justifyContent: 'center' }}

                >

                  <Heart size={12} />

                  <span>Apologize</span>

                </button>



                <button

                  onClick={() => onTriggerAction('/do?pat=1')}

                  className="flux-btn"

                  style={{ padding: '8px 10px', fontSize: '11px', background: 'var(--card-muted)', color: 'var(--text-dark)', border: '1px solid var(--border-light)', justifyContent: 'center' }}

                >

                  <Hand size={12} />

                  <span>Pat Head</span>

                </button>

              </div>



              {/* Autonomous Idle Chatter (Boredom) Panel (UNTITLED UI ROUNDED DROPDOWN) */}

              <div style={{

                background: 'var(--card-subtle)',

                border: '1px solid var(--border-light)',

                borderRadius: '12px',

                padding: '10px 12px',

                marginBottom: '10px'

              }}>

                <div style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'center', marginBottom: '8px' }}>

                  <label className="flux-toggle">

                    <input

                      type="checkbox"

                      checked={boredomEnabled}

                      onChange={(e) => handleBoredomToggle(e.target.checked)}

                    />

                    <span className="flux-toggle-slider" />

                    <span style={{ fontSize: '12px', fontWeight: 700, color: 'var(--text-dark)' }}>

                      Autonomous Idle Chatter

                    </span>

                  </label>

                </div>

                <div style={{ display: 'flex', alignItems: 'center', gap: '8px' }}>

                  <label style={{ fontSize: '11px', fontWeight: 600, color: 'var(--text-muted)', flexShrink: 0 }}>

                    Interval:

                  </label>

                  <FluxSelect

                    value={boredomMins}

                    onChange={(e) => handleBoredomInterval(e.target.value)}

                    options={BOREDOM_INTERVAL_OPTIONS}

                  />

                </div>

              </div>

            </div>



            {/* GREETINGS TOGGLE PANEL (PROMINENT - MATCHING LEGACY HTML) */}

            {/* GREETINGS & SOUND-REACTIVE FACE PANEL */}

            <div style={{

              background: 'var(--card-subtle)',

              border: '1px solid var(--border-light)',

              borderRadius: '12px',

              padding: '10px 12px',

              marginBottom: '10px'

            }}>

              <div style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'center', marginBottom: '4px' }}>

                <label className="flux-toggle">

                  <input

                    type="checkbox"

                    checked={greetingsEnabled}

                    onChange={(e) => handleGreetingsToggle(e.target.checked)}

                  />

                  <span className="flux-toggle-slider" />

                  <span style={{ fontSize: '12px', fontWeight: 700, color: 'var(--text-dark)' }}>

                    Greetings

                  </span>

                </label>

                <span style={{

                  fontSize: '10px',

                  fontWeight: 800,

                  padding: '2px 6px',

                  borderRadius: '999px',

                  background: greetingsEnabled ? 'rgba(52,199,89,0.15)' : 'var(--card-muted)',

                  color: greetingsEnabled ? '#15803D' : 'var(--text-muted)'

                }}>

                  {greetingsEnabled ? 'ACTIVE' : 'OFF'}

                </span>

              </div>

              <div style={{ fontSize: '11px', color: 'var(--text-muted)', lineHeight: 1.35, marginBottom: '8px' }}>

                Kiwi bolega Good morning / afternoon / night, din mein ek baar.

              </div>



              {/* Sound-Reactive Face Toggle */}

              <div style={{ borderTop: '1px solid var(--border-light)', paddingTop: '8px', display: 'flex', justifyContent: 'space-between', alignItems: 'center' }}>

                <label className="flux-toggle">

                  <input

                    type="checkbox"

                    checked={sndReactEnabled}

                    onChange={(e) => handleSoundReactToggle(e.target.checked)}

                  />

                  <span className="flux-toggle-slider" />

                  <span style={{ fontSize: '12px', fontWeight: 700, color: 'var(--text-dark)' }}>

                    Sound-Reactive Face

                  </span>

                </label>

                <span style={{

                  fontSize: '9px',

                  fontWeight: 800,

                  padding: '2px 6px',

                  borderRadius: '999px',

                  background: sndReactEnabled ? 'rgba(52,199,89,0.15)' : 'var(--card-muted)',

                  color: sndReactEnabled ? '#15803D' : 'var(--text-muted)'

                }}>

                  {sndReactEnabled ? 'ACTIVE' : 'OFF'}

                </span>

              </div>

            </div>



            {/* PUT TO SLEEP / WAKE UP ACTION */}
            <div style={{ marginBottom: '10px' }}>
              <button
                type="button"
                onClick={handleSleepToggle}
                className="flux-btn"
                style={{
                  width: '100%',
                  padding: '11px 16px',
                  borderRadius: '12px',
                  background: isSleeping ? 'linear-gradient(135deg, #0EA5E9 0%, #0284C7 100%)' : 'var(--card-dark)',
                  color: '#FFFFFF',
                  fontWeight: 800,
                  fontSize: '12px',
                  display: 'flex',
                  alignItems: 'center',
                  justifyContent: 'center',
                  gap: '8px',
                  cursor: 'pointer',
                  border: isSleeping ? '1px solid #38BDF8' : 'none',
                  boxShadow: isSleeping ? '0 4px 14px rgba(14, 165, 233, 0.4)' : '0 4px 14px rgba(0, 0, 0, 0.15)',
                  transition: 'all 0.2s ease'
                }}
              >
                {isSleeping ? <Sun size={15} /> : <Moon size={15} />}
                <span>{isSleeping ? 'Wake Kiwi Up' : 'Put to Sleep'}</span>
              </button>
            </div>



            {/* SLEEP SCHEDULE (CIRCADIAN) PANEL */}

            <div style={{

              background: 'var(--card-subtle)',

              border: '1px solid var(--border-light)',

              borderRadius: '12px',

              padding: '10px 12px'

            }}>

              <div style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'center', marginBottom: '8px' }}>

                <label className="flux-toggle">

                  <input

                    type="checkbox"

                    checked={circEnabled}

                    onChange={(e) => handleCircadianToggle(e.target.checked)}

                  />

                  <span className="flux-toggle-slider" />

                  <span style={{ fontSize: '12px', fontWeight: 700, color: 'var(--text-dark)' }}>

                    Sleep Schedule

                  </span>

                </label>

                <span style={{

                  fontSize: '10px', fontWeight: 800,

                  color: circEnabled ? '#15803D' : 'var(--text-muted)',

                  background: circEnabled ? 'rgba(52, 199, 89, 0.15)' : 'var(--card-muted)',

                  padding: '2px 8px', borderRadius: '999px'

                }}>

                  {circEnabled ? 'SCHEDULE ON' : 'OFF'}

                </span>

              </div>

              <div style={{ display: 'grid', gridTemplateColumns: 'minmax(0, 1fr) minmax(0, 1fr)', gap: '8px' }}>

                <div style={{ minWidth: 0 }}>

                  <label style={{ fontSize: '10px', color: 'var(--text-muted)', display: 'block', marginBottom: '3px', fontWeight: 600 }}>Sleep At</label>

                  <FluxSelect

                    compact

                    dark={false}

                    value={circStart}

                    onChange={(e) => handleCircStartChange(e.target.value)}

                    options={HOUR_OPTIONS}

                  />

                </div>

                <div style={{ minWidth: 0 }}>

                  <label style={{ fontSize: '10px', color: 'var(--text-muted)', display: 'block', marginBottom: '3px', fontWeight: 600 }}>Wake At</label>

                  <FluxSelect

                    compact

                    dark={false}

                    value={circEnd}

                    onChange={(e) => handleCircEndChange(e.target.value)}

                    options={HOUR_OPTIONS}

                  />

                </div>

              </div>

            </div>

          </div>

        </div>

      </div>



      {/* ================= MOBILE PHONE VIEW (Visible ONLY on < 768px) ================= */}

      <div className="mobile-console-layout">
        {/* Hero Greeting: Welcome back, {ownerName} */}

        <div className="mobile-greeting-section" style={{ marginBottom: '16px' }}>

          <h1 className="mobile-greeting-title">

            Welcome<br />back, {ownerName}

          </h1>

        </div>



        {/* Bento 2-Column Grid matching reference image */}

        <div className="mobile-bento-grid">

          {/* Left Column: Vitals (Purple) + Sleep Action (Coral) */}

          <div className="mobile-bento-col">

            {/* Card 1: Live Hardware Telemetry (RAM, Flash & CPU Temp) */}
            {(() => {
              const freeKb = status?.heap?.free ? Math.round(status.heap.free / 1024) : 178;
              const totalKb = status?.heap?.total ? Math.round(status.heap.total / 1024) : 320;
              const ramUsedPct = status?.heap?.pct ?? (freeKb ? Math.round(((totalKb - freeKb) / totalKb) * 100) : 44);
              const cpuTemp = status?.temp != null ? Math.round(status.temp) : 46;

              // Flash storage calculations
              const flashUsed = status?.flash?.used ?? 1648200;
              const flashFree = status?.flash?.free ?? 383416;
              const flashTotal = status?.flash?.total ?? (flashUsed + flashFree);
              const flashPct = status?.flash?.pct ?? Math.round((flashUsed / flashTotal) * 100);
              const flashFreeMb = (flashFree / (1024 * 1024)).toFixed(2);
              const flashUsedMb = (flashUsed / (1024 * 1024)).toFixed(2);
              const flashTotalMb = (flashTotal / (1024 * 1024)).toFixed(1);
              const flashChipMb = status?.flash?.chip ? Math.round(status.flash.chip / (1024 * 1024)) : 4;

              return (
                <div style={{
                  background: 'linear-gradient(145deg, #181B24 0%, #101218 100%)',
                  borderRadius: '22px',
                  padding: '13px 14px',
                  color: '#FFFFFF',
                  border: '1px solid rgba(255, 255, 255, 0.08)',
                  display: 'flex',
                  flexDirection: 'column',
                  gap: '8px',
                  boxShadow: '0 8px 24px rgba(0, 0, 0, 0.22)',
                  position: 'relative',
                  overflow: 'hidden'
                }}>
                  {/* Header */}
                  <div style={{ display: 'flex', alignItems: 'center', justifyContent: 'space-between', width: '100%' }}>
                    <span style={{ fontSize: '11px', opacity: 0.9, fontWeight: 700, letterSpacing: '0.04em', textTransform: 'uppercase' }}>
                      Vitals
                    </span>
                    <span style={{
                      fontSize: '9px',
                      fontWeight: 800,
                      background: 'rgba(255, 255, 255, 0.18)',
                      padding: '2px 7px',
                      borderRadius: '999px',
                      letterSpacing: '0.03em',
                      whiteSpace: 'nowrap'
                    }}>
                      ESP32-S3
                    </span>
                  </div>

                  {/* Telemetry Metrics */}
                  <div style={{ display: 'flex', flexDirection: 'column', gap: '8px' }}>
                    {/* CPU Core Temp */}
                    <div style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'baseline' }}>
                      <span style={{ fontSize: '11px', opacity: 0.8, fontWeight: 600 }}>CPU Temp</span>
                      <span style={{ fontSize: '18px', fontWeight: 800, letterSpacing: '-0.02em', color: '#FFFFFF' }}>
                        {cpuTemp}°<span style={{ fontSize: '11px', fontWeight: 600, opacity: 0.85 }}>C</span>
                      </span>
                    </div>

                    {/* RAM / Heap Usage */}
                    <div>
                      <div style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'baseline', marginBottom: '3px' }}>
                        <span style={{ fontSize: '11px', opacity: 0.8, fontWeight: 600 }}>RAM Usage</span>
                        <span style={{ fontSize: '11px', fontWeight: 700, color: '#38BDF8' }}>
                          {freeKb} KB <span style={{ opacity: 0.75, fontWeight: 500, color: '#FFFFFF' }}>free</span>
                        </span>
                      </div>
                      {/* Progress Bar */}
                      <div style={{
                        width: '100%',
                        height: '5px',
                        borderRadius: '3px',
                        background: 'rgba(255, 255, 255, 0.2)',
                        overflow: 'hidden'
                      }}>
                        <div style={{
                          height: '100%',
                          width: `${Math.min(100, Math.max(0, ramUsedPct))}%`,
                          background: ramUsedPct > 85 ? '#F43F5E' : '#38BDF8',
                          borderRadius: '3px',
                          transition: 'width 0.4s ease'
                        }} />
                      </div>
                      <div style={{ display: 'flex', justifyContent: 'space-between', marginTop: '3px' }}>
                        <span style={{ fontSize: '9px', opacity: 0.7 }}>{ramUsedPct}% used</span>
                        <span style={{ fontSize: '9px', opacity: 0.7 }}>{totalKb} KB total</span>
                      </div>
                    </div>

                    {/* Flash Storage Usage */}
                    <div>
                      <div style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'baseline', marginBottom: '3px' }}>
                        <span style={{ fontSize: '11px', opacity: 0.8, fontWeight: 600 }}>Flash Storage</span>
                        <span style={{ fontSize: '11px', fontWeight: 700, color: '#A78BFA' }}>
                          {flashFreeMb} MB <span style={{ opacity: 0.75, fontWeight: 500, color: '#FFFFFF' }}>free</span>
                        </span>
                      </div>
                      {/* Progress Bar */}
                      <div style={{
                        width: '100%',
                        height: '5px',
                        borderRadius: '3px',
                        background: 'rgba(255, 255, 255, 0.2)',
                        overflow: 'hidden'
                      }}>
                        <div style={{
                          height: '100%',
                          width: `${Math.min(100, Math.max(0, flashPct))}%`,
                          background: flashPct > 90 ? '#F43F5E' : '#A78BFA',
                          borderRadius: '3px',
                          transition: 'width 0.4s ease'
                        }} />
                      </div>
                      <div style={{ display: 'flex', justifyContent: 'space-between', marginTop: '3px' }}>
                        <span style={{ fontSize: '9px', opacity: 0.7 }}>{flashPct}% used ({flashUsedMb}M)</span>
                        <span style={{ fontSize: '9px', opacity: 0.7 }}>{flashTotalMb}M ({flashChipMb}M Chip)</span>
                      </div>
                    </div>
                  </div>
                </div>
              );
            })()}



            {/* Card 2: ESP32-S3 Built-in Pin 48 RGB LED HeroUI ColorPicker */}
            <Esp32LedColorPicker
              onColorChange={(hex) => onTriggerAction(`/do?rgb_color=${encodeURIComponent(hex.replace('#', ''))}`)}
              onClearOverride={() => onTriggerAction('/do?rgb_clear=1')}
              initialColor="#38BDF8"
              currentEmotion={status?.emotion}
              rgbEnabled={status?.rgbLed?.enabled ?? true}
              showTitle={false}
              showHex={false}
              style={{ flex: 1, minHeight: 0 }}
            />

          </div>



          {/* Right Column: Connection Status (Green/Red) + Suggestions (White) */}
          <div className="mobile-bento-col">
            {/* Card 3: Online / Offline Link Status */}
            <div
              onClick={() => onRefresh?.()}
              style={{
                background: connected ? '#10B981' : '#EF4444',
                borderRadius: '20px',
                padding: '14px 14px',
                color: '#FFFFFF',
                cursor: 'pointer',
                display: 'flex',
                alignItems: 'center',
                gap: '10px',
                boxShadow: connected
                  ? '0 6px 18px rgba(16, 185, 129, 0.28)'
                  : '0 6px 18px rgba(239, 68, 68, 0.28)',
                transition: 'all 0.2s cubic-bezier(0.16, 1, 0.3, 1)'
              }}
            >
              <div style={{
                width: '32px', height: '32px', borderRadius: '50%',
                background: 'rgba(255, 255, 255, 0.2)',
                display: 'flex', alignItems: 'center', justifyContent: 'center',
                flexShrink: 0
              }}>
                {connected ? (
                  <Wifi size={16} color="#FFFFFF" />
                ) : (
                  <WifiOff size={16} color="#FFFFFF" />
                )}
              </div>
              <div>
                <span style={{ fontSize: '10px', opacity: 0.85, fontWeight: 600, display: 'block' }}>
                  Robot Link
                </span>
                <span style={{ fontSize: '13px', fontWeight: 800, textTransform: 'uppercase', letterSpacing: '0.04em' }}>
                  {connected ? 'ONLINE' : 'OFFLINE'}
                </span>
              </div>
            </div>



            {/* Card 4: Suggestions / Smart Controls */}
            <div style={{
              background: 'var(--card-white)',
              border: '1px solid var(--border-light)',
              borderRadius: '22px',
              padding: '12px 10px',
              display: 'flex',
              flexDirection: 'column',
              justifyContent: 'space-between',
              gap: '10px',
              boxShadow: 'var(--shadow-card)',
              width: '100%',
              boxSizing: 'border-box',
              flex: 1,
              minHeight: 0
            }}>
              <div style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'center' }}>
                <span style={{ fontSize: '12px', fontWeight: 700, color: 'var(--text-dark)' }}>
                  Suggestions
                </span>
                <span style={{
                  fontSize: '9px', fontWeight: 800, padding: '2px 7px',
                  borderRadius: '999px', background: 'var(--card-muted)', color: 'var(--text-muted)'
                }}>
                  All
                </span>
              </div>

              {/* Row 1: Daily Greetings */}
              <div style={{
                display: 'flex',
                alignItems: 'center',
                justifyContent: 'space-between',
                gap: '6px',
                padding: '2px 0',
                minHeight: '34px',
                boxSizing: 'border-box'
              }}>
                <div style={{ display: 'flex', alignItems: 'center', gap: '6px', flex: 1, minWidth: 0 }}>
                  <div style={{
                    width: '26px', height: '26px', borderRadius: '7px',
                    background: 'rgba(99, 102, 241, 0.15)', color: '#818CF8',
                    display: 'flex', alignItems: 'center', justifyContent: 'center', flexShrink: 0
                  }}>
                    <svg
                      xmlns="http://www.w3.org/2000/svg"
                      width="14"
                      height="14"
                      viewBox="0 0 24 24"
                      style={{ display: 'block' }}
                    >
                      <path
                        fill="currentColor"
                        d="M23 17c0 3.31-2.69 6-6 6v-1.5c2.5 0 4.5-2 4.5-4.5zM1 7c0-3.31 2.69-6 6-6v1.5c-2.5 0-4.5 2-4.5 4.5zm7-2.68l-4.59 4.6c-3.22 3.22-3.22 8.45 0 11.67s8.45 3.22 11.67 0l7.07-7.09c.49-.47.49-1.26 0-1.75a1.25 1.25 0 0 0-1.77 0l-4.42 4.42l-.71-.71l6.54-6.54c.49-.49.49-1.28 0-1.77s-1.29-.49-1.79 0L14.19 13l-.69-.73l6.87-6.89c.49-.49.49-1.28 0-1.77s-1.28-.49-1.77 0l-6.89 6.89l-.71-.7l5.5-5.48c.5-.49.5-1.28 0-1.77s-1.28-.49-1.77 0l-7.62 7.62a4 4 0 0 1-.33 5.28l-.71-.71a3 3 0 0 0 0-4.24l-.35-.35l4.07-4.07c.49-.49.49-1.28 0-1.77c-.5-.48-1.29-.48-1.79.01"
                      />
                    </svg>
                  </div>
                  <div style={{ flex: 1, minWidth: 0 }}>
                    <div style={{ fontSize: '11px', fontWeight: 700, color: 'var(--text-dark)', whiteSpace: 'nowrap', overflow: 'hidden', textOverflow: 'ellipsis' }}>
                      Greetings
                    </div>
                    <div style={{ fontSize: '9px', color: 'var(--text-muted)', whiteSpace: 'nowrap', overflow: 'hidden', textOverflow: 'ellipsis' }}>
                      {greetingsEnabled ? 'Active' : 'Disabled'}
                    </div>
                  </div>
                </div>
                <button
                  type="button"
                  onClick={() => handleGreetingsToggle(!greetingsEnabled)}
                  style={{
                    width: '38px',
                    minWidth: '38px',
                    maxWidth: '38px',
                    height: '24px',
                    boxSizing: 'border-box',
                    padding: 0,
                    fontSize: '10px',
                    fontWeight: 700,
                    borderRadius: '999px',
                    border: '1px solid ' + (greetingsEnabled ? 'var(--accent-lime)' : 'var(--border-light)'),
                    background: greetingsEnabled ? 'var(--accent-lime)' : 'var(--card-muted)',
                    color: greetingsEnabled ? '#12141A' : 'var(--text-dark)',
                    cursor: 'pointer',
                    display: 'inline-flex',
                    alignItems: 'center',
                    justifyContent: 'center',
                    textAlign: 'center',
                    flexShrink: 0
                  }}
                >
                  {greetingsEnabled ? 'ON' : 'OFF'}
                </button>
              </div>

              {/* Row 2: Wake Word (FluxSelect Dropdown + ON/OFF) */}
              <div style={{
                display: 'flex',
                alignItems: 'center',
                justifyContent: 'space-between',
                gap: '6px',
                padding: '2px 0',
                minHeight: '34px',
                boxSizing: 'border-box'
              }}>
                <div style={{ display: 'flex', alignItems: 'center', gap: '6px', flex: 1, minWidth: 0 }}>
                  <div style={{
                    width: '26px', height: '26px', borderRadius: '7px',
                    background: 'rgba(16, 185, 129, 0.15)', color: '#34D399',
                    display: 'flex', alignItems: 'center', justifyContent: 'center', flexShrink: 0
                  }}>
                    <Mic size={13} />
                  </div>
                  <div style={{ flex: 1, minWidth: 0 }}>
                    <div style={{ fontSize: '11px', fontWeight: 700, color: 'var(--text-dark)', whiteSpace: 'nowrap', overflow: 'hidden', textOverflow: 'ellipsis', marginBottom: '2px' }}>
                      Wake Word
                    </div>
                    <div style={{ fontSize: '10px', fontWeight: 600, color: 'var(--text-muted)', lineHeight: '14px' }}>
                      Alexa
                    </div>
                  </div>
                </div>
                <button
                  type="button"
                  onClick={() => handleWakeWordToggle(!wwEnabled)}
                  style={{
                    width: '38px',
                    minWidth: '38px',
                    maxWidth: '38px',
                    height: '24px',
                    boxSizing: 'border-box',
                    padding: 0,
                    fontSize: '10px',
                    fontWeight: 700,
                    borderRadius: '999px',
                    border: '1px solid ' + (wwEnabled ? 'var(--accent-lime)' : 'var(--border-light)'),
                    background: wwEnabled ? 'var(--accent-lime)' : 'var(--card-muted)',
                    color: wwEnabled ? '#12141A' : 'var(--text-dark)',
                    cursor: 'pointer',
                    display: 'inline-flex',
                    alignItems: 'center',
                    justifyContent: 'center',
                    textAlign: 'center',
                    flexShrink: 0
                  }}
                >
                  {wwEnabled ? 'ON' : 'OFF'}
                </button>
              </div>

              {/* Row 3: Boredom Idle Chatter (FluxSelect Dropdown + ON/OFF) */}
              <div style={{
                display: 'flex',
                alignItems: 'center',
                justifyContent: 'space-between',
                gap: '6px',
                padding: '2px 0',
                minHeight: '34px',
                boxSizing: 'border-box'
              }}>
                <div style={{ display: 'flex', alignItems: 'center', gap: '6px', flex: 1, minWidth: 0 }}>
                  <div style={{
                    width: '26px', height: '26px', borderRadius: '7px',
                    background: 'rgba(168, 85, 247, 0.15)', color: '#C084FC',
                    display: 'flex', alignItems: 'center', justifyContent: 'center', flexShrink: 0
                  }}>
                    <Coffee size={13} />
                  </div>
                  <div style={{ flex: 1, minWidth: 0 }}>
                    <div style={{ fontSize: '11px', fontWeight: 700, color: 'var(--text-dark)', whiteSpace: 'nowrap', overflow: 'hidden', textOverflow: 'ellipsis', marginBottom: '2px' }}>
                      Idle Chatter
                    </div>
                    <div style={{ width: '100%', maxWidth: '60px' }}>
                      <FluxSelect
                        micro={true}
                        value={boredomMins}
                        onChange={(e) => handleBoredomInterval(e.target.value)}
                        options={MOBILE_BOREDOM_OPTIONS}
                      />
                    </div>
                  </div>
                </div>
                <button
                  type="button"
                  onClick={() => handleBoredomToggle(!boredomEnabled)}
                  style={{
                    width: '38px',
                    minWidth: '38px',
                    maxWidth: '38px',
                    height: '24px',
                    boxSizing: 'border-box',
                    padding: 0,
                    fontSize: '10px',
                    fontWeight: 700,
                    borderRadius: '999px',
                    border: '1px solid ' + (boredomEnabled ? 'var(--accent-lime)' : 'var(--border-light)'),
                    background: boredomEnabled ? 'var(--accent-lime)' : 'var(--card-muted)',
                    color: boredomEnabled ? '#12141A' : 'var(--text-dark)',
                    cursor: 'pointer',
                    display: 'inline-flex',
                    alignItems: 'center',
                    justifyContent: 'center',
                    textAlign: 'center',
                    flexShrink: 0
                  }}
                >
                  {boredomEnabled ? 'ON' : 'OFF'}
                </button>
              </div>
            </div>

          </div>

        </div>

        {/* Voice Prompt Box for Mobile */}
        <div className="flux-white-card" style={{ padding: '16px' }}>
          <form onSubmit={(e) => { e.preventDefault(); handleSendPrompt(); }} style={{ display: 'flex', alignItems: 'center', gap: '8px' }}>
            <input
              type="text"
              value={promptText}
              onChange={(e) => setPromptText(e.target.value)}
              placeholder={isListening ? 'Listening...' : 'Ask Kiwi anything...'}
              style={{
                flex: 1,
                height: '42px',
                boxSizing: 'border-box',
                padding: '0 18px',
                borderRadius: '999px',
                background: 'var(--card-muted)',
                border: '1px solid var(--border-light)',
                color: 'var(--text-dark)',
                fontSize: '13px',
                outline: 'none'
              }}
            />
            <button
              type="submit"
              disabled={sendingPrompt || !promptText.trim()}
              className="flux-btn"
              style={{
                background: 'linear-gradient(135deg, #0EA5E9 0%, #0284C7 100%)',
                color: '#FFFFFF',
                padding: 0,
                borderRadius: '50%',
                width: '42px',
                height: '42px',
                display: 'flex',
                alignItems: 'center',
                justifyContent: 'center',
                flexShrink: 0,
                opacity: (!promptText.trim() || sendingPrompt) ? 0.4 : 1
              }}
            >
              <Send size={16} />
            </button>
          </form>



          {/* Quick Chat Bubble Preview */}

          {(status?.lastPrompt || status?.lastReply) && (

            <div style={{ marginTop: '12px', fontSize: '12px', lineHeight: 1.4 }}>

              {status?.lastPrompt && (

                <div style={{ color: 'var(--text-muted)', marginBottom: '4px' }}>

                  <strong>You:</strong> {status.lastPrompt}

                </div>

              )}

              {status?.lastReply && (

                <div style={{ color: 'var(--text-dark)', fontWeight: 600 }}>

                  <strong>Kiwi:</strong> {status.lastReply}

                </div>

              )}

            </div>

          )}

        </div>



        {/* Hardware Sliders: Volume, Brightness & Pin 48 NeoPixel */}

        <div className="flux-white-card" style={{ padding: '16px' }}>

          <div className="flux-card-header" style={{ marginBottom: '12px' }}>

            <span className="flux-card-title">

              <Volume2 size={15} />

              <span>Hardware Controls</span>

            </span>

            <span style={{ fontSize: '11px', color: 'var(--text-muted)' }}>

              Live

            </span>

          </div>



          <div style={{ display: 'flex', flexDirection: 'column', gap: '14px' }}>

            <div>

              <div style={{ display: 'flex', justifyContent: 'space-between', fontSize: '12px', marginBottom: '6px' }}>

                <span style={{ color: 'var(--text-muted)', fontWeight: 600 }}>Speaker Volume:</span>

                <strong style={{ color: 'var(--text-dark)' }}>{vol}%</strong>

              </div>

              <WakeSlider
                value={vol}
                min={0}
                max={100}
                step={5}
                bars={20}
                height={32}
                restHeight={16}
                gap={4}
                fillColor="var(--slider-fill)"
                trackColor="var(--slider-track)"
                crestColor="#D2FA4B"
                onChange={(v) => setVol(v)}
                onCommit={(v) => onTriggerAction(`/do?vol=${v}`)}
                ariaLabel="Speaker Volume"
              />
            </div>

            <div>
              <div style={{ display: 'flex', justifyContent: 'space-between', fontSize: '12px', marginBottom: '6px' }}>
                <span style={{ color: 'var(--text-muted)', fontWeight: 600 }}>OLED Brightness:</span>
                <strong style={{ color: 'var(--text-dark)' }}>{bright}%</strong>
              </div>
              <WakeSlider
                value={bright}
                min={5}
                max={100}
                step={5}
                bars={20}
                height={32}
                restHeight={16}
                gap={4}
                fillColor="#0ea5e9"
                trackColor="var(--slider-track)"
                crestColor="#38bdf8"
                onChange={(v) => setBright(v)}
                onCommit={(v) => onTriggerAction(`/do?brightness=${v}`)}
                ariaLabel="OLED Brightness"
              />
            </div>

            {/* Pin 48 NeoPixel LED */}
            <div style={{
              background: 'var(--card-subtle)', border: '1px solid var(--border-light)',
              borderRadius: '12px', padding: '10px 12px'
            }}>
              <div style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'center', marginBottom: '8px' }}>
                <label className="flux-toggle">
                  <input
                    type="checkbox"
                    checked={rgbEnabled}
                    onChange={(e) => handleRgbToggle(e.target.checked)}
                  />
                  <span className="flux-toggle-slider" />
                  <span style={{ fontSize: '12px', fontWeight: 600, color: 'var(--text-dark)' }}>
                    Pin 48 NeoPixel RGB
                  </span>
                </label>
                <span style={{ fontSize: '11px', fontWeight: 700, color: 'var(--text-muted)' }}>
                  {rgbBright}%
                </span>
              </div>
              <WakeSlider
                value={rgbBright}
                min={0}
                max={100}
                step={5}
                bars={20}
                height={32}
                restHeight={16}
                gap={4}
                disabled={!rgbEnabled}

                fillColor="#10B981"

                trackColor="var(--slider-track)"

                crestColor="#34D399"

                onChange={(v) => setRgbBright(v)}

                onCommit={(v) => onTriggerAction(`/do?rgb_bright=${v}`)}

                ariaLabel="NeoPixel RGB Brightness"

              />

            </div>

          </div>

        </div>



        {/* Sleep Engine & Night Schedule for Mobile (Matches Desktop) */}

        <div className="flux-white-card" style={{ padding: '16px', display: 'flex', flexDirection: 'column', gap: '12px' }}>

          <div className="flux-card-header" style={{ marginBottom: '2px' }}>

            <span className="flux-card-title">

              <Moon size={16} color="var(--text-dark)" />

              <span>Sleep &amp; Night Schedule</span>

            </span>

            <span style={{

              background: isSleeping ? '#FEE2E2' : 'rgba(52, 199, 89, 0.15)',

              color: isSleeping ? '#DC2626' : '#15803D',

              padding: '3px 10px', borderRadius: '999px',

              fontSize: '10px', fontWeight: 800

            }}>

              {isSleeping ? 'SLEEPING' : 'AWAKE'}

            </span>

          </div>



          {/* Put Kiwi to Sleep / Wake Up Action */}
          <div>
            <button
              type="button"
              onClick={handleSleepToggle}
              className="flux-btn"
              style={{
                width: '100%',
                padding: '12px 18px',
                borderRadius: '12px',
                background: isSleeping ? 'linear-gradient(135deg, #0EA5E9 0%, #0284C7 100%)' : 'var(--card-dark)',
                color: '#FFFFFF',
                fontWeight: 800,
                fontSize: '13px',
                display: 'flex',
                alignItems: 'center',
                justifyContent: 'center',
                gap: '8px',
                cursor: 'pointer',
                border: isSleeping ? '1px solid #38BDF8' : 'none',
                boxShadow: isSleeping ? '0 4px 14px rgba(14, 165, 233, 0.4)' : '0 4px 14px rgba(0, 0, 0, 0.15)',
                transition: 'all 0.2s ease'
              }}
            >
              {isSleeping ? <Sun size={16} /> : <Moon size={16} />}
              <span>{isSleeping ? 'Wake Kiwi Up' : 'Put to Sleep'}</span>
            </button>
          </div>



          {/* Sleep Schedule (Circadian) Panel */}

          <div style={{

            background: 'var(--card-subtle)',

            border: '1px solid var(--border-light)',

            borderRadius: '14px',

            padding: '12px 14px'

          }}>

            <div style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'center', marginBottom: '8px' }}>

              <label className="flux-toggle">

                <input

                  type="checkbox"

                  checked={circEnabled}

                  onChange={(e) => handleCircadianToggle(e.target.checked)}

                />

                <span className="flux-toggle-slider" />

                <span style={{ fontSize: '12px', fontWeight: 700, color: 'var(--text-dark)' }}>

                  Sleep Schedule

                </span>

              </label>

              <span style={{

                fontSize: '10px', fontWeight: 800,

                color: circEnabled ? '#15803D' : 'var(--text-muted)',

                background: circEnabled ? 'rgba(52, 199, 89, 0.15)' : 'var(--card-muted)',

                padding: '2px 8px', borderRadius: '999px'

              }}>

                {circEnabled ? 'SCHEDULE ON' : 'OFF'}

              </span>

            </div>

            <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr', gap: '8px' }}>

              <div>

                <label style={{ fontSize: '10px', color: 'var(--text-muted)', display: 'block', marginBottom: '3px', fontWeight: 600 }}>Sleep At</label>

                <FluxSelect

                  dark={false}

                  value={circStart}

                  onChange={(e) => handleCircStartChange(e.target.value)}

                  options={HOUR_OPTIONS}

                />

              </div>

              <div>

                <label style={{ fontSize: '10px', color: 'var(--text-muted)', display: 'block', marginBottom: '3px', fontWeight: 600 }}>Wake At</label>

                <FluxSelect

                  dark={false}

                  value={circEnd}

                  onChange={(e) => handleCircEndChange(e.target.value)}

                  options={HOUR_OPTIONS}

                />

              </div>

            </div>

          </div>



          {/* Sound-Reactive Face Toggle Panel */}

          <div style={{

            background: 'var(--card-subtle)',

            border: '1px solid var(--border-light)',

            borderRadius: '14px',

            padding: '12px 14px'

          }}>

            <div style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'center' }}>

              <label className="flux-toggle">

                <input

                  type="checkbox"

                  checked={sndReactEnabled}

                  onChange={(e) => handleSoundReactToggle(e.target.checked)}

                />

                <span className="flux-toggle-slider" />

                <span style={{ fontSize: '12px', fontWeight: 700, color: 'var(--text-dark)' }}>

                  Sound-Reactive Face

                </span>

              </label>

              <span style={{

                fontSize: '9px', fontWeight: 800, padding: '2px 6px', borderRadius: '999px',

                background: sndReactEnabled ? 'rgba(52, 199, 89, 0.15)' : 'var(--card-muted)',

                color: sndReactEnabled ? '#15803D' : 'var(--text-muted)'

              }}>

                {sndReactEnabled ? 'ACTIVE' : 'OFF'}

              </span>

            </div>

          </div>



        </div>



      </div>

    </div>

  )

}

