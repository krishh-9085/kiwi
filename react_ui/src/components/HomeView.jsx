import React, { useState, useEffect, useMemo, useRef } from 'react'

import KiwiEyes from './KiwiEyes'
import KiwiLogo from './KiwiLogo'

import { Tooltip } from './Tooltip'

import WakeSlider from './ui/WakeSlider'

import {

  Heart, Zap, Activity, Moon, Volume2, VolumeX, Sun,

  Mic, MicOff, Send, SlidersHorizontal,

  Utensils, Coffee, Hand, Eye, Music, Radio, Shield, Sparkles,

  Clock, BellRing, Sparkle, ArrowDown, RotateCw, Settings as SettingsIcon, Check,

  Bot, Smile, Cpu, Wifi, WifiOff

} from 'lucide-react'

import FluxSelect from './FluxSelect'
import RgbControlCard from './RgbControlCard'

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
  { id: 'emo', label: 'EMO' },
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




function formatTime24To12(hhmm) {
  if (!hhmm || !String(hhmm).includes(':')) return '--:--'
  const [hRaw, mRaw] = String(hhmm).split(':')
  const h = Number(hRaw)
  const m = Number(mRaw)
  if (Number.isNaN(h) || Number.isNaN(m)) return '--:--'
  const ap = h >= 12 ? 'PM' : 'AM'
  const h12 = (h % 12) === 0 ? 12 : (h % 12)
  return `${h12}:${String(m).padStart(2, '0')} ${ap}`
}

const HALF_HOUR_OPTIONS = Array.from({ length: 48 }).map((_, i) => {
  const h = Math.floor(i / 2)
  const m = (i % 2) * 30
  const val = `${String(h).padStart(2, '0')}:${String(m).padStart(2, '0')}`
  return { val, lbl: formatTime24To12(val) }
})

const TZ_OPTIONS = [
  { val: '-480', lbl: 'PST (UTC-8:00)' },
  { val: '-420', lbl: 'MST (UTC-7:00)' },
  { val: '-360', lbl: 'CST (UTC-6:00)' },
  { val: '-300', lbl: 'EST (UTC-5:00)' },
  { val: '-240', lbl: 'EDT (UTC-4:00)' },
  { val: '-180', lbl: 'BRT (UTC-3:00)' },
  { val: '0', lbl: 'UTC / GMT (+0:00)' },
  { val: '60', lbl: 'CET / BST (+1:00)' },
  { val: '120', lbl: 'EET / CEST (+2:00)' },
  { val: '180', lbl: 'AST / MSK (+3:00)' },
  { val: '240', lbl: 'GST Dubai (+4:00)' },
  { val: '300', lbl: 'PKT (+5:00)' },
  { val: '330', lbl: 'IST India (+5:30)' },
  { val: '360', lbl: 'BST Dhaka (+6:00)' },
  { val: '420', lbl: 'ICT (+7:00)' },
  { val: '480', lbl: 'SGT / HKT (+8:00)' },
  { val: '540', lbl: 'JST / KST (+9:00)' },
  { val: '600', lbl: 'AEST (+10:00)' },
  { val: '720', lbl: 'NZST (+12:00)' }
]

const OFFLINE_PSRAM_FALLBACK = {
  free: 1951744,
  total: 2097152,
  used: 145408,
  pct: 7
}

function computeVitals(status) {
  const hasLiveHeap = Boolean(status?.heap?.total > 0)
  const freeKb = status?.heap?.free ? Math.round(status.heap.free / 1024) : 178
  const totalKb = status?.heap?.total ? Math.round(status.heap.total / 1024) : 320
  const ramUsedPct = status?.heap?.pct ?? (freeKb ? Math.round(((totalKb - freeKb) / totalKb) * 100) : 44)
  const cpuTemp = status?.temp != null ? Math.round(status.temp) : 46

  // Show 2MB PSRAM fallback when offline (no live /status yet); hide only if live device reports no PSRAM
  const psramSrc = status?.psram ?? (!hasLiveHeap ? OFFLINE_PSRAM_FALLBACK : null)
  const hasPsram = Boolean(psramSrc && psramSrc.total > 0)
  const psramTotalKb = hasPsram ? Math.round(psramSrc.total / 1024) : 0
  const psramFreeKb = hasPsram ? Math.round(psramSrc.free / 1024) : 0
  const psramUsedKb = hasPsram ? Math.max(0, psramTotalKb - psramFreeKb) : 0
  const psramPct = hasPsram && psramTotalKb > 0 ? Math.round((psramUsedKb / psramTotalKb) * 100) : 0
  const psramFreeMb = hasPsram ? (psramFreeKb / 1024).toFixed(2) : '0'
  const psramTotalMb = hasPsram ? (psramTotalKb / 1024).toFixed(0) : '0'

  const flashUsed = status?.flash?.used ?? 1648200
  const flashFree = status?.flash?.free ?? 383416
  const flashTotal = status?.flash?.total ?? (flashUsed + flashFree)
  const flashPct = status?.flash?.pct ?? Math.round((flashUsed / flashTotal) * 100)
  const flashFreeMb = (flashFree / (1024 * 1024)).toFixed(2)
  const flashUsedMb = (flashUsed / (1024 * 1024)).toFixed(2)
  const flashTotalMb = (flashTotal / (1024 * 1024)).toFixed(1)
  const flashChipMb = status?.flash?.chip ? Math.round(status.flash.chip / (1024 * 1024)) : 4

  return {
    freeKb, totalKb, ramUsedPct, cpuTemp,
    hasPsram, psramTotalKb, psramFreeKb, psramUsedKb, psramPct, psramFreeMb, psramTotalMb,
    flashPct, flashFreeMb, flashUsedMb, flashTotalMb, flashChipMb
  }
}



export default function HomeView({

  status,

  config,

  connected = true,

  onTriggerAction,

  onToast,

  onTabChange,

  onRefresh

}) {

  const [isMobileView, setIsMobileView] = useState(() =>
    typeof window !== 'undefined' && window.matchMedia
      ? window.matchMedia('(max-width: 768px)').matches
      : false
  )

  useEffect(() => {
    if (typeof window === 'undefined' || !window.matchMedia) return
    const mq = window.matchMedia('(max-width: 768px)')
    const onMqChange = (e) => setIsMobileView(e.matches)
    mq.addEventListener?.('change', onMqChange)
  return () => mq.removeEventListener?.('change', onMqChange)
  }, [])

  const [vol, setVol] = useState(70)
  const lastUnmutedVol = useRef(70)
  const draggingSlidersRef = useRef({ vol: false, bright: false, nightBright: false, rgbBright: false })
  const sliderHoldUntilRef = useRef({ vol: 0, bright: 0, nightBright: 0, rgbBright: 0 })
  const configInitRef = useRef({ vol: false, bright: false, nightBright: false, rgbBright: false })

  const sliderCommitThrottleRef = useRef({})

  useEffect(() => () => {
    Object.values(sliderCommitThrottleRef.current).forEach((rec) => {
      if (rec?.timer) clearTimeout(rec.timer)
    })
  }, [])

  const pushSliderValue = (key, url, immediate = false) => {
    const rec = sliderCommitThrottleRef.current[key] || { lastAt: 0, lastUrl: '', timer: null }
    sliderCommitThrottleRef.current[key] = rec
    if (immediate) {
      if (rec.timer) {
        clearTimeout(rec.timer)
        rec.timer = null
      }
      if (rec.lastUrl !== url) {
        rec.lastAt = Date.now()
        rec.lastUrl = url
        onTriggerAction?.(url)
      }
      return
    }
    const now = Date.now()
    const wait = 180 - (now - rec.lastAt)
    if (wait <= 0) {
      if (rec.timer) {
        clearTimeout(rec.timer)
        rec.timer = null
      }
      rec.lastAt = now
      rec.lastUrl = url
      onTriggerAction?.(url)
    } else {
      if (rec.timer) clearTimeout(rec.timer)
      rec.timer = setTimeout(() => {
        rec.timer = null
        rec.lastAt = Date.now()
        rec.lastUrl = url
        onTriggerAction?.(url)
      }, wait)
    }
  }

  const handleToggleMute = () => {
    sliderHoldUntilRef.current.vol = Date.now() + 1500
    if (vol > 0) {
      lastUnmutedVol.current = vol
      setVol(0)
      pushSliderValue('vol', '/do?vol=0', true)
    } else {
      const restoreVol = lastUnmutedVol.current > 0 ? lastUnmutedVol.current : 70
      setVol(restoreVol)
      pushSliderValue('vol', `/do?vol=${restoreVol}`, true)
    }
  }

  const [bright, setBright] = useState(90)
  const [nightBright, setNightBright] = useState(10)

  const [rgbBright, setRgbBright] = useState(65)

  const [promptText, setPromptText] = useState('')

  const [sendingPrompt, setSendingPrompt] = useState(false)



  // Sync state from status / config without fighting active drags or recent commits

  useEffect(() => {
    const now = Date.now()

    if (!draggingSlidersRef.current.vol && now >= sliderHoldUntilRef.current.vol) {
      if (status?.volume != null) {
        setVol(Math.round(status.volume / 5) * 5)
        configInitRef.current.vol = true
      } else if (!configInitRef.current.vol && config?.volume != null) {
        setVol(Math.round(config.volume / 5) * 5)
        configInitRef.current.vol = true
      }
    }

    if (!draggingSlidersRef.current.bright && now >= sliderHoldUntilRef.current.bright) {
      if (status?.brightness != null) {
        setBright(Math.max(5, Math.round(status.brightness / 5) * 5))
        configInitRef.current.bright = true
      } else if (!configInitRef.current.bright && config?.brightness != null) {
        setBright(Math.max(5, Math.round(config.brightness / 5) * 5))
        configInitRef.current.bright = true
      }
    }

    if (!draggingSlidersRef.current.nightBright && now >= sliderHoldUntilRef.current.nightBright) {
      if (status?.nightBrightness != null) {
        setNightBright(Math.max(5, Math.round(status.nightBrightness / 5) * 5))
        configInitRef.current.nightBright = true
      } else if (!configInitRef.current.nightBright && config?.nightBrightness != null) {
        setNightBright(Math.max(5, Math.round(config.nightBrightness / 5) * 5))
        configInitRef.current.nightBright = true
      }
    }

    if (!draggingSlidersRef.current.rgbBright && now >= sliderHoldUntilRef.current.rgbBright) {
      if (status?.rgbLed?.brightness != null) {
        setRgbBright(Math.round(status.rgbLed.brightness / 5) * 5)
        configInitRef.current.rgbBright = true
      } else if (!configInitRef.current.rgbBright && config?.rgbLed?.brightness != null) {
        setRgbBright(Math.round(config.rgbLed.brightness / 5) * 5)
        configInitRef.current.rgbBright = true
      }
    }

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
  const vitals = computeVitals(status)



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

  // Circadian & Timezone state
  const circEnabled = status?.circadian?.enabled ?? config?.circadian?.enabled ?? true
  const circStartH = status?.circadian?.start ?? config?.circadian?.start ?? 23
  const circStartM = status?.circadian?.startMin ?? config?.circadian?.startMin ?? 0
  const circEndH = status?.circadian?.end ?? config?.circadian?.end ?? 7
  const circEndM = status?.circadian?.endMin ?? config?.circadian?.endMin ?? 0
  const circStart = status?.circadian?.startStr || config?.circadian?.startStr ||
    `${String(circStartH).padStart(2, '0')}:${String(circStartM).padStart(2, '0')}`
  const circEnd = status?.circadian?.endStr || config?.circadian?.endStr ||
    `${String(circEndH).padStart(2, '0')}:${String(circEndM).padStart(2, '0')}`
  const ecoSleepEnabled = status?.circadian?.ecoSleep ?? status?.power?.ecoSleep ?? config?.circadian?.ecoSleep ?? false
  const tzMin = String(status?.time?.tzMin ?? config?.tzMin ?? 330)
  const timeSynced = Boolean(status?.time?.synced)
  const localTimeStr = status?.time?.localTime || ''
  const skipTonight = Boolean(status?.circadian?.skipTonight)
  const tempWakeSec = status?.circadian?.tempWakeSec ?? 0
  const circScheduledActive = Boolean(status?.circadian?.active)
  const circManualActive = Boolean(status?.circadian?.manual)

  const startOptions = useMemo(() => {
    if (HALF_HOUR_OPTIONS.some((o) => o.val === circStart)) return HALF_HOUR_OPTIONS
    return [{ val: circStart, lbl: formatTime24To12(circStart) }, ...HALF_HOUR_OPTIONS]
  }, [circStart])

  const endOptions = useMemo(() => {
    if (HALF_HOUR_OPTIONS.some((o) => o.val === circEnd)) return HALF_HOUR_OPTIONS
    return [{ val: circEnd, lbl: formatTime24To12(circEnd) }, ...HALF_HOUR_OPTIONS]
  }, [circEnd])

  const tzOptions = useMemo(() => {
    if (TZ_OPTIONS.some((o) => o.val === tzMin)) return TZ_OPTIONS
    const n = Number(tzMin)
    const sign = n >= 0 ? '+' : '-'
    const ah = Math.floor(Math.abs(n) / 60)
    const am = Math.abs(n) % 60
    return [{ val: tzMin, lbl: `UTC${sign}${ah}:${String(am).padStart(2, '0')}` }, ...TZ_OPTIONS]
  }, [tzMin])

  // Night Mode state (0: Same, 1: Dim, 2: Screen off)
  const nightMode = status?.nightMode ?? config?.nightMode ?? 1



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
    await onTriggerAction(`/do?circ_start=${encodeURIComponent(val)}`)
    onToast(`Sleep time set to ${formatTime24To12(val)}`)
  }

  const handleCircEndChange = async (val) => {
    await onTriggerAction(`/do?circ_end=${encodeURIComponent(val)}`)
    onToast(`Wake time set to ${formatTime24To12(val)}`)
  }

  const handleSnooze = async (mins = 15) => {
    setLocalSleeping(false)
    await onTriggerAction(`/do?snooze=${mins}`)
    onToast(`Snoozed sleep for ${mins} minutes`)
  }

  const handleEcoSleepToggle = async (checked) => {
    await onTriggerAction(`/do?eco_sleep=${checked ? '1' : '0'}`)
    onToast(checked ? 'Eco Hardware Deep Sleep enabled' : 'Desk Mode enabled (Wi-Fi & Zzz live)')
  }

  const handleTzChange = async (val) => {
    await onTriggerAction(`/do?tz_min=${encodeURIComponent(val)}`)
    onToast('Timezone updated & NTP syncing...')
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

  const renderHardwareSliders = () => {
    const isMuted = vol === 0
    const isOledLow = bright <= 25
    return (
      <div style={{ display: 'flex', flexDirection: 'column', gap: '12px' }}>
        {/* Speaker Volume */}
        <div style={{
          background: 'var(--card-subtle)',
          border: '1px solid var(--border-light)',
          borderRadius: '12px',
          padding: '10px 12px'
        }}>
          <div style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'center', marginBottom: '8px', gap: '8px' }}>
            <div style={{ display: 'flex', alignItems: 'center', gap: '6px' }}>
              {isMuted ? <VolumeX size={13} color="#F59E0B" /> : <Volume2 size={13} color="#A3E635" />}
              <span style={{ fontSize: '12px', fontWeight: 700, color: 'var(--text-dark)' }}>
                Speaker Volume
              </span>
            </div>
            <div style={{ display: 'flex', alignItems: 'center', gap: '6px' }}>
              <button
                type="button"
                onClick={handleToggleMute}
                style={{
                  border: '1px solid var(--border-light)',
                  borderRadius: '999px',
                  padding: '2px 8px',
                  fontSize: '10px',
                  fontWeight: 700,
                  cursor: 'pointer',
                  background: isMuted ? 'rgba(245, 158, 11, 0.16)' : 'var(--card-muted)',
                  color: isMuted ? '#F59E0B' : 'var(--text-muted)'
                }}
              >
                {isMuted ? 'Unmute' : 'Mute'}
              </button>
              <strong style={{
                fontSize: '12px',
                fontVariantNumeric: 'tabular-nums',
                minWidth: '34px',
                textAlign: 'right',
                color: isMuted ? '#F59E0B' : 'var(--text-dark)'
              }}>
                {vol}%
              </strong>
            </div>
          </div>
          <WakeSlider
            value={vol}
            min={0}
            max={100}
            step={5}
            bars={20}
            height={30}
            restHeight={15}
            gap={4}
            fillColor={isMuted ? '#64748B' : 'var(--slider-fill)'}
            trackColor="var(--slider-track)"
            crestColor="#D2FA4B"
            onDragStart={() => { draggingSlidersRef.current.vol = true }}
            onDragEnd={() => {
              draggingSlidersRef.current.vol = false
              sliderHoldUntilRef.current.vol = Date.now() + 1500
            }}
            onChange={(v) => {
              sliderHoldUntilRef.current.vol = Date.now() + 1500
              if (v > 0) lastUnmutedVol.current = v
              setVol(v)
              pushSliderValue('vol', `/do?vol=${v}`, false)
            }}
            onCommit={(v) => {
              sliderHoldUntilRef.current.vol = Date.now() + 1500
              if (v > 0) lastUnmutedVol.current = v
              pushSliderValue('vol', `/do?vol=${v}`, true)
            }}
            ariaLabel="Speaker Volume"
          />
        </div>

        {/* OLED Brightness */}
        <div style={{
          background: 'var(--card-subtle)',
          border: '1px solid var(--border-light)',
          borderRadius: '12px',
          padding: '10px 12px'
        }}>
          <div style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'center', marginBottom: '8px', gap: '8px' }}>
            <div style={{ display: 'flex', alignItems: 'center', gap: '6px' }}>
              <Sun size={13} color="#0EA5E9" />
              <span style={{ fontSize: '12px', fontWeight: 700, color: 'var(--text-dark)' }}>
                OLED Brightness
              </span>
            </div>
            <div style={{ display: 'flex', alignItems: 'center', gap: '6px' }}>
              <button
                type="button"
                onClick={() => {
                  const nextB = isOledLow ? 80 : 15
                  sliderHoldUntilRef.current.bright = Date.now() + 1500
                  setBright(nextB)
                  pushSliderValue('bright', `/do?brightness=${nextB}`, true)
                }}
                style={{
                  border: '1px solid var(--border-light)',
                  borderRadius: '999px',
                  padding: '2px 8px',
                  fontSize: '10px',
                  fontWeight: 700,
                  cursor: 'pointer',
                  background: 'var(--card-muted)',
                  color: 'var(--text-muted)'
                }}
              >
                {isOledLow ? 'Bright' : 'Low'}
              </button>
              <strong style={{
                fontSize: '12px',
                fontVariantNumeric: 'tabular-nums',
                minWidth: '34px',
                textAlign: 'right',
                color: 'var(--text-dark)'
              }}>
                {bright}%
              </strong>
            </div>
          </div>
          <WakeSlider
            value={bright}
            min={0}
            max={100}
            step={5}
            bars={20}
            height={30}
            restHeight={15}
            gap={4}
            fillColor="#0ea5e9"
            trackColor="var(--slider-track)"
            crestColor="#38bdf8"
            onDragStart={() => { draggingSlidersRef.current.bright = true }}
            onDragEnd={() => {
              draggingSlidersRef.current.bright = false
              sliderHoldUntilRef.current.bright = Date.now() + 1500
            }}
            onChange={(v) => {
              const clamped = Math.max(5, v)
              sliderHoldUntilRef.current.bright = Date.now() + 1500
              setBright(clamped)
              pushSliderValue('bright', `/do?brightness=${clamped}`, false)
            }}
            onCommit={(v) => {
              const clamped = Math.max(5, v)
              sliderHoldUntilRef.current.bright = Date.now() + 1500
              setBright(clamped)
              pushSliderValue('bright', `/do?brightness=${clamped}`, true)
            }}
            ariaLabel="OLED Brightness"
          />
        </div>

        {/* NeoPixel Status RGB LED */}
        <div style={{
          background: 'var(--card-subtle)',
          border: '1px solid var(--border-light)',
          borderRadius: '12px',
          padding: '10px 12px'
        }}>
          <div style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'center', marginBottom: '8px', gap: '8px' }}>
            <label className="flux-toggle">
              <input
                type="checkbox"
                checked={rgbEnabled}
                onChange={(e) => handleRgbToggle(e.target.checked)}
              />
              <span className="flux-toggle-slider" />
              <span style={{ fontSize: '12px', fontWeight: 700, color: 'var(--text-dark)' }}>
                NeoPixel RGB
              </span>
            </label>
            <span style={{
              fontSize: '10.5px',
              fontWeight: 800,
              fontVariantNumeric: 'tabular-nums',
              padding: '2px 8px',
              borderRadius: '999px',
              background: rgbEnabled ? 'rgba(16, 185, 129, 0.14)' : 'var(--card-muted)',
              color: rgbEnabled ? '#10B981' : 'var(--text-muted)'
            }}>
              {rgbEnabled ? `${rgbBright}%` : 'OFF'}
            </span>
          </div>
          <WakeSlider
            value={rgbBright}
            min={0}
            max={100}
            step={5}
            bars={20}
            height={30}
            restHeight={15}
            gap={4}
            disabled={!rgbEnabled}
            fillColor="#10B981"
            trackColor="var(--slider-track)"
            crestColor="#34D399"
            onDragStart={() => { draggingSlidersRef.current.rgbBright = true }}
            onDragEnd={() => {
              draggingSlidersRef.current.rgbBright = false
              sliderHoldUntilRef.current.rgbBright = Date.now() + 1500
            }}
            onChange={(v) => {
              sliderHoldUntilRef.current.rgbBright = Date.now() + 1500
              setRgbBright(v)
              pushSliderValue('rgbBright', `/do?rgb_bright=${v}`, false)
            }}
            onCommit={(v) => {
              sliderHoldUntilRef.current.rgbBright = Date.now() + 1500
              pushSliderValue('rgbBright', `/do?rgb_bright=${v}`, true)
            }}
            ariaLabel="NeoPixel RGB Brightness"
          />
        </div>
      </div>
    )
  }

  const isEqualSchedule = circEnabled && circStart === circEnd

  let sleepBadgeText = 'AWAKE'
  let sleepBadgeColor = '#15803D'
  let sleepBadgeBg = 'rgba(52, 199, 89, 0.15)'
  if (isSleeping) {
    sleepBadgeText = circManualActive && !circScheduledActive ? 'MANUAL SLEEP' : 'SCHEDULED SLEEP'
    sleepBadgeColor = '#DC2626'
    sleepBadgeBg = '#FEE2E2'
  } else if (tempWakeSec > 0) {
    sleepBadgeText = tempWakeSec >= 60 ? `SNOOZE ${Math.ceil(tempWakeSec / 60)}M` : `PEEK ${tempWakeSec}S`
    sleepBadgeColor = '#B45309'
    sleepBadgeBg = '#FEF3C7'
  } else if (skipTonight) {
    sleepBadgeText = 'OVERRIDE'
    sleepBadgeColor = '#0284C7'
    sleepBadgeBg = 'rgba(14, 165, 233, 0.15)'
  }

  const renderSleepSchedulePanel = (isCompact = true) => (
    <div style={{ display: 'flex', flexDirection: 'column', gap: isCompact ? '8px' : '10px' }}>
      {/* Live Clock Sync & Power State Row */}
      <div style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'center', gap: '6px', flexWrap: 'wrap' }}>
        <span style={{
          display: 'inline-flex',
          alignItems: 'center',
          gap: '4px',
          fontSize: '10px',
          fontWeight: 700,
          padding: '2px 8px',
          borderRadius: '999px',
          background: timeSynced ? 'var(--card-muted)' : '#FEF3C7',
          color: timeSynced ? 'var(--text-dark)' : '#B45309',
          border: '1px solid var(--border-light)'
        }}>
          <Clock size={10} />
          <span>{timeSynced ? formatTime24To12(localTimeStr) : 'Waiting NTP...'}</span>
        </span>
        <span style={{
          fontSize: '9.5px',
          fontWeight: 800,
          padding: '2px 8px',
          borderRadius: '999px',
          background: sleepBadgeBg,
          color: sleepBadgeColor,
          letterSpacing: '0.03em'
        }}>
          {sleepBadgeText}
        </span>
      </div>

      {/* Put to Sleep / Wake Up + Snooze 15m */}
      <div style={{ display: 'flex', gap: '6px' }}>
        <button
          type="button"
          onClick={handleSleepToggle}
          className="flux-btn"
          style={{
            flex: 1,
            padding: isCompact ? '10px 12px' : '11px 16px',
            borderRadius: '12px',
            background: isSleeping ? 'linear-gradient(135deg, #0EA5E9 0%, #0284C7 100%)' : 'var(--card-dark)',
            color: '#FFFFFF',
            fontWeight: 800,
            fontSize: isCompact ? '12px' : '13px',
            display: 'flex',
            alignItems: 'center',
            justifyContent: 'center',
            gap: '7px',
            cursor: 'pointer',
            border: isSleeping ? '1px solid #38BDF8' : 'none',
            boxShadow: isSleeping ? '0 4px 14px rgba(14, 165, 233, 0.35)' : '0 4px 14px rgba(0, 0, 0, 0.14)',
            transition: 'all 0.2s ease'
          }}
        >
          {isSleeping ? <Sun size={isCompact ? 14 : 16} /> : <Moon size={isCompact ? 14 : 16} />}
          <span>{isSleeping ? 'Wake Kiwi Up' : 'Put to Sleep'}</span>
        </button>
        {isSleeping && (
          <button
            type="button"
            onClick={() => handleSnooze(15)}
            className="flux-btn"
            title="Wake briefly for 15 minutes without disabling schedule"
            style={{
              padding: isCompact ? '10px 11px' : '11px 14px',
              borderRadius: '12px',
              background: 'var(--card-subtle)',
              color: 'var(--text-dark)',
              border: '1px solid var(--border-light)',
              fontWeight: 800,
              fontSize: '11px',
              whiteSpace: 'nowrap',
              cursor: 'pointer'
            }}
          >
            +15m
          </button>
        )}
      </div>

      {/* Sleep Schedule (Circadian) Box */}
      <div style={{
        background: 'var(--card-subtle)',
        border: '1px solid var(--border-light)',
        borderRadius: '12px',
        padding: isCompact ? '10px 12px' : '12px 14px',
        display: 'flex',
        flexDirection: 'column',
        gap: '8px'
      }}>
        <div style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'center' }}>
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
            fontSize: '10px',
            fontWeight: 800,
            color: circEnabled ? '#15803D' : 'var(--text-muted)',
            background: circEnabled ? 'rgba(52, 199, 89, 0.15)' : 'var(--card-muted)',
            padding: '2px 8px',
            borderRadius: '999px'
          }}>
            {circEnabled ? 'SCHEDULE ON' : 'OFF'}
          </span>
        </div>

        <div style={{ display: 'grid', gridTemplateColumns: 'minmax(0, 1fr) minmax(0, 1fr)', gap: '8px' }}>
          <div style={{ minWidth: 0 }}>
            <label style={{ fontSize: '10px', color: 'var(--text-muted)', display: 'block', marginBottom: '3px', fontWeight: 600 }}>Sleep At</label>
            <FluxSelect
              compact={isCompact}
              dark={false}
              value={circStart}
              onChange={(e) => handleCircStartChange(e.target.value)}
              options={startOptions}
            />
          </div>
          <div style={{ minWidth: 0 }}>
            <label style={{ fontSize: '10px', color: 'var(--text-muted)', display: 'block', marginBottom: '3px', fontWeight: 600 }}>Wake At</label>
            <FluxSelect
              compact={isCompact}
              dark={false}
              value={circEnd}
              onChange={(e) => handleCircEndChange(e.target.value)}
              options={endOptions}
            />
          </div>
        </div>

        {isEqualSchedule && (
          <div style={{ fontSize: '10px', color: '#B45309', background: '#FEF3C7', padding: '4px 8px', borderRadius: '8px', fontWeight: 600 }}>
            Sleep At and Wake At are identical — schedule is paused until changed.
          </div>
        )}

        {/* Night Display Mode */}
        <div style={{ borderTop: '1px solid var(--border-light)', paddingTop: '8px' }}>
          <div style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'center', gap: '6px', marginBottom: nightMode === 1 ? '8px' : '0' }}>
            <span style={{ fontSize: '11px', fontWeight: 600, color: 'var(--text-muted)' }}>Night Display</span>
            <div style={{ display: 'flex', gap: '4px', background: 'var(--card-muted)', padding: '2px', borderRadius: '8px', border: '1px solid var(--border-light)' }}>
              {[
                { m: 0, l: 'Same' },
                { m: 1, l: 'Dim' },
                { m: 2, l: 'Off' }
              ].map((opt) => (
                <button
                  key={opt.m}
                  type="button"
                  onClick={() => handleNightModeChange(opt.m)}
                  style={{
                    border: 'none',
                    borderRadius: '6px',
                    padding: '3px 8px',
                    fontSize: '10px',
                    fontWeight: 700,
                    cursor: 'pointer',
                    background: nightMode === opt.m ? 'var(--card-dark)' : 'transparent',
                    color: nightMode === opt.m ? '#FFFFFF' : 'var(--text-muted)'
                  }}
                >
                  {opt.l}
                </button>
              ))}
            </div>
          </div>
          {nightMode === 1 && (
            <div>
              <div style={{ display: 'flex', justifyContent: 'space-between', fontSize: '10.5px', marginBottom: '4px' }}>
                <span style={{ color: 'var(--text-muted)', fontWeight: 600 }}>Night Brightness</span>
                <strong style={{ color: 'var(--text-dark)' }}>{nightBright}%</strong>
              </div>
              <WakeSlider
                value={nightBright}
                min={5}
                max={100}
                step={5}
                bars={16}
                height={24}
                restHeight={12}
                gap={3}
                fillColor="#6366F1"
                trackColor="var(--slider-track)"
                crestColor="#818CF8"
                onDragStart={() => { draggingSlidersRef.current.nightBright = true }}
                onDragEnd={() => {
                  draggingSlidersRef.current.nightBright = false
                  sliderHoldUntilRef.current.nightBright = Date.now() + 1500
                }}
                onChange={(v) => {
                  sliderHoldUntilRef.current.nightBright = Date.now() + 1500
                  setNightBright(v)
                }}
                onCommit={(v) => {
                  sliderHoldUntilRef.current.nightBright = Date.now() + 1500
                  onTriggerAction(`/do?night_bright=${v}`)
                }}
                ariaLabel="Night OLED Brightness"
              />
            </div>
          )}
        </div>

        {/* Desk Mode vs Eco Deep Sleep + Timezone */}
        <div style={{ borderTop: '1px solid var(--border-light)', paddingTop: '8px', display: 'flex', flexDirection: 'column', gap: '8px' }}>
          <div style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'center', gap: '8px' }}>
            <label className="flux-toggle">
              <input
                type="checkbox"
                checked={ecoSleepEnabled}
                onChange={(e) => handleEcoSleepToggle(e.target.checked)}
              />
              <span className="flux-toggle-slider" />
              <span style={{ fontSize: '11.5px', fontWeight: 700, color: 'var(--text-dark)' }}>
                Eco Deep Sleep
              </span>
            </label>
            <span style={{ fontSize: '9.5px', fontWeight: 700, color: 'var(--text-muted)' }}>
              {ecoSleepEnabled ? 'Battery (Wi-Fi Off)' : 'Desk (Zzz + Web)'}
            </span>
          </div>

          <div style={{ display: 'flex', alignItems: 'center', justifyContent: 'space-between', gap: '8px' }}>
            <span style={{ fontSize: '10.5px', color: 'var(--text-muted)', fontWeight: 600, whiteSpace: 'nowrap' }}>Timezone</span>
            <div style={{ flex: 1, minWidth: 0, maxWidth: '175px' }}>
              <FluxSelect
                compact
                dark={false}
                value={tzMin}
                onChange={(e) => handleTzChange(e.target.value)}
                options={tzOptions}
              />
            </div>
          </div>
        </div>
      </div>
    </div>
  )

  return (

    <div>

      {/* ================= DESKTOP VIEWPORT (Visible ONLY on > 768px) ================= */}
      {!isMobileView ? (
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
                const {
                  freeKb, totalKb, ramUsedPct, cpuTemp,
                  hasPsram, psramFreeKb, psramUsedKb, psramPct, psramFreeMb, psramTotalMb,
                  flashPct, flashFreeMb, flashUsedMb, flashTotalMb, flashChipMb
                } = vitals;

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
                        background: hasPsram ? 'rgba(52, 211, 153, 0.18)' : 'rgba(255, 255, 255, 0.18)',
                        color: hasPsram ? '#34D399' : '#FFFFFF',
                        border: hasPsram ? '1px solid rgba(52, 211, 153, 0.4)' : 'none',
                        padding: '2px 8px',
                        borderRadius: '999px',
                        letterSpacing: '0.04em',
                        whiteSpace: 'nowrap'
                      }}>
                        {hasPsram ? `ESP32-S3 • +${psramTotalMb}MB PSRAM ACTIVE` : 'ESP32-S3'}
                      </span>
                    </div>

                    {/* Stats Grid */}
                    <div style={{
                      display: 'grid',
                      gridTemplateColumns: hasPsram ? '72px 1fr 1fr 1fr' : '85px 1fr 1fr',
                      gap: '12px',
                      marginTop: '12px',
                      alignItems: 'center'
                    }}>
                      {/* CPU Core Temp */}
                      <div>
                        <span style={{ fontSize: '11px', opacity: 0.8, fontWeight: 600, display: 'block' }}>CPU Temp</span>
                        <span style={{ fontSize: '22px', fontWeight: 800, letterSpacing: '-0.02em', color: '#FFFFFF' }}>
                          {cpuTemp}°<span style={{ fontSize: '13px', fontWeight: 600, opacity: 0.85 }}>C</span>
                        </span>
                      </div>

                      {/* Internal SRAM Usage */}
                      <div>
                        <div style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'baseline', marginBottom: '3px' }}>
                          <span style={{ fontSize: '11px', opacity: 0.8, fontWeight: 600 }}>SRAM (Heap)</span>
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

                      {/* External PSRAM Usage (2MB High-Speed) */}
                      {hasPsram && (
                        <div>
                          <div style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'baseline', marginBottom: '3px' }}>
                            <span style={{ fontSize: '11px', opacity: 0.8, fontWeight: 600, color: '#34D399' }}>2MB PSRAM</span>
                            <span style={{ fontSize: '11px', fontWeight: 700, color: '#34D399' }}>
                              {psramFreeKb >= 1024 ? `${psramFreeMb} MB` : `${psramFreeKb} KB`} <span style={{ opacity: 0.75, fontWeight: 500, color: '#FFFFFF' }}>free</span>
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
                              width: `${Math.min(100, Math.max(0, psramPct))}%`,
                              background: '#34D399',
                              borderRadius: '3px',
                              transition: 'width 0.4s ease'
                            }} />
                          </div>
                          <div style={{ display: 'flex', justifyContent: 'space-between', marginTop: '3px' }}>
                            <span style={{ fontSize: '9px', opacity: 0.7 }}>{psramPct}% used ({psramUsedKb} KB)</span>
                            <span style={{ fontSize: '9px', opacity: 0.7 }}>{psramTotalMb} MB Chip</span>
                          </div>
                        </div>
                      )}

                      {/* Flash Storage Space */}
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
                <RgbControlCard
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
                <span className="flux-progress-label">Free Heap ({status?.heap?.free ? Math.round(status.heap.free / 1024) : 178} KB)</span>
              </div>

              {vitals.hasPsram ? (
                <div className="flux-progress-row">
                  <span className="flux-progress-val" style={{ color: '#34d399' }}>{`${100 - vitals.psramPct}%`}</span>
                  <div className="flux-progress-track">
                    <div style={{ background: '#34d399', height: '100%', borderRadius: '4px', width: `${100 - vitals.psramPct}%` }} />
                  </div>
                  <span className="flux-progress-label" style={{ color: '#34d399' }}>Free PSRAM ({vitals.psramFreeKb} KB / {vitals.psramTotalMb}MB)</span>
                </div>
              ) : null}

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



              {/* Hardware Sliders: Volume, OLED Brightness & NeoPixel RGB */}
              <div style={{ marginBottom: '16px' }}>
                {renderHardwareSliders()}
              </div>

            </div>

          </div>



          {/* ================= COLUMN 3: GREETINGS, CHATTER & SKINS ================= */}

          <div className="flux-white-card" style={{ justifyContent: 'space-between' }}>

            <div>

              <div className="flux-card-header" style={{ marginBottom: '14px' }}>

                <span className="flux-card-title">

                  <Sparkles size={15} color="#38BDF8" />

                  <span>Chatter & Greetings</span>

                </span>

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



            {/* PUT TO SLEEP / SCHEDULE PANEL */}
            {renderSleepSchedulePanel(true)}

          </div>

        </div>

      </div>
      ) : (
      /* ================= MOBILE PHONE VIEW (Visible ONLY on <= 768px) ================= */

      <div className="mobile-console-layout">
        {/* Hero Greeting: Welcome back, {ownerName} */}

        <div className="mobile-greeting-section" style={{ marginBottom: '16px' }}>

          <h1 className="mobile-greeting-title">

            Welcome<br />back, {ownerName}

          </h1>

        </div>



        {/* Bento 2-Column Grid (Top Left: Vitals, Top Right: Online | Bottom Left: Color, Bottom Right: Suggestions) */}
        <div className="mobile-bento-grid">
          {/* Left Column: Vitals (Top) + Color Picker (Bottom) */}
          <div className="mobile-bento-col">
            {/* Card 1: Live Hardware Telemetry (RAM, Flash & CPU Temp) */}
            {(() => {
              const {
                freeKb, totalKb, ramUsedPct, cpuTemp,
                hasPsram, psramFreeKb, psramUsedKb, psramPct, psramFreeMb, psramTotalMb,
                flashPct, flashFreeMb, flashUsedMb, flashTotalMb, flashChipMb
              } = vitals;

  return (
                <div style={{
                  background: 'var(--card-white)',
                  border: '1px solid var(--border-light)',
                  borderRadius: '22px',
                  padding: '13px 14px',
                  color: 'var(--text-dark)',
                  display: 'flex',
                  flexDirection: 'column',
                  justifyContent: 'space-between',
                  gap: '8px',
                  boxShadow: 'var(--shadow-card)',
                  position: 'relative',
                  boxSizing: 'border-box'
                }}>
                  {/* Header */}
                  <div style={{ display: 'flex', alignItems: 'center', justifyContent: 'space-between', width: '100%' }}>
                    <span style={{ fontSize: '11px', fontWeight: 700, color: 'var(--text-dark)', letterSpacing: '0.04em', textTransform: 'uppercase' }}>
                      Vitals
                    </span>
                    <span style={{
                      fontSize: '9px',
                      fontWeight: 800,
                      background: 'rgba(16, 185, 129, 0.14)',
                      color: '#059669',
                      border: '1px solid rgba(16, 185, 129, 0.3)',
                      padding: '2px 7px',
                      borderRadius: '999px',
                      letterSpacing: '0.04em',
                      whiteSpace: 'nowrap'
                    }}>
                      ESP32-S3
                    </span>
                  </div>

                  {/* CPU Core Temp - Sleek Compact Bar */}
                  <div style={{
                    display: 'flex',
                    justifyContent: 'space-between',
                    alignItems: 'center',
                    background: 'var(--card-muted)',
                    border: '1px solid var(--border-light)',
                    padding: '3px 8px',
                    borderRadius: '8px'
                  }}>
                    <span style={{ fontSize: '10.5px', color: 'var(--text-muted)', fontWeight: 600 }}>CPU Temp</span>
                    <span style={{ fontSize: '12px', fontWeight: 800, color: cpuTemp > 75 ? '#EF4444' : 'var(--text-dark)' }}>
                      {cpuTemp}°C
                    </span>
                  </div>

                  {/* Telemetry Metrics */}
                  <div style={{ display: 'flex', flexDirection: 'column', gap: '7px' }}>
                    {/* SRAM (Heap) Usage */}
                    <div>
                      <div style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'baseline', marginBottom: '3px' }}>
                        <span style={{ fontSize: '10.5px', color: 'var(--text-dark)', fontWeight: 600 }}>SRAM</span>
                        <span style={{ fontSize: '10.5px', fontWeight: 700, color: '#0284C7' }}>
                          {freeKb} KB <span style={{ opacity: 0.65, fontWeight: 500, color: 'var(--text-muted)' }}>free</span>
                        </span>
                      </div>
                      <div style={{
                        width: '100%',
                        height: '4px',
                        borderRadius: '2px',
                        background: 'var(--card-muted)',
                        border: '1px solid var(--border-light)',
                        overflow: 'hidden'
                      }}>
                        <div style={{
                          height: '100%',
                          width: `${Math.min(100, Math.max(0, ramUsedPct))}%`,
                          background: ramUsedPct > 85 ? '#EF4444' : '#0284C7',
                          borderRadius: '2px',
                          transition: 'width 0.4s ease'
                        }} />
                      </div>
                      <div style={{ display: 'flex', justifyContent: 'space-between', marginTop: '2px' }}>
                        <span style={{ fontSize: '8.5px', color: 'var(--text-muted)' }}>{ramUsedPct}% used</span>
                        <span style={{ fontSize: '8.5px', color: 'var(--text-muted)' }}>{totalKb} KB</span>
                      </div>
                    </div>

                    {/* PSRAM (External 2MB RAM) */}
                    {hasPsram && (
                      <div>
                        <div style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'baseline', marginBottom: '3px' }}>
                          <span style={{ fontSize: '10.5px', fontWeight: 600, color: '#059669' }}>2MB PSRAM</span>
                          <span style={{ fontSize: '10.5px', fontWeight: 700, color: '#059669' }}>
                            {psramFreeKb >= 1024 ? `${psramFreeMb} MB` : `${psramFreeKb} KB`} <span style={{ opacity: 0.65, fontWeight: 500, color: 'var(--text-muted)' }}>free</span>
                          </span>
                        </div>
                        <div style={{
                          width: '100%',
                          height: '4px',
                          borderRadius: '2px',
                          background: 'var(--card-muted)',
                          border: '1px solid var(--border-light)',
                          overflow: 'hidden'
                        }}>
                          <div style={{
                            height: '100%',
                            width: `${Math.min(100, Math.max(0, psramPct))}%`,
                            background: '#10B981',
                            borderRadius: '2px',
                            transition: 'width 0.4s ease'
                          }} />
                        </div>
                        <div style={{ display: 'flex', justifyContent: 'space-between', marginTop: '2px' }}>
                          <span style={{ fontSize: '8.5px', color: 'var(--text-muted)' }}>{psramPct}% used ({psramUsedKb} KB)</span>
                          <span style={{ fontSize: '8.5px', color: 'var(--text-muted)' }}>{psramTotalMb} MB Chip</span>
                        </div>
                      </div>
                    )}

                    {/* Flash Storage Usage */}
                    <div>
                      <div style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'baseline', marginBottom: '3px' }}>
                        <span style={{ fontSize: '10.5px', color: 'var(--text-dark)', fontWeight: 600 }}>Flash Storage</span>
                        <span style={{ fontSize: '10.5px', fontWeight: 700, color: '#7C3AED' }}>
                          {flashFreeMb} MB <span style={{ opacity: 0.65, fontWeight: 500, color: 'var(--text-muted)' }}>free</span>
                        </span>
                      </div>
                      <div style={{
                        width: '100%',
                        height: '4px',
                        borderRadius: '2px',
                        background: 'var(--card-muted)',
                        border: '1px solid var(--border-light)',
                        overflow: 'hidden'
                      }}>
                        <div style={{
                          height: '100%',
                          width: `${Math.min(100, Math.max(0, flashPct))}%`,
                          background: flashPct > 90 ? '#EF4444' : '#8B5CF6',
                          borderRadius: '2px',
                          transition: 'width 0.4s ease'
                        }} />
                      </div>
                      <div style={{ display: 'flex', justifyContent: 'space-between', marginTop: '2px' }}>
                        <span style={{ fontSize: '8.5px', color: 'var(--text-muted)' }}>{flashPct}% used</span>
                        <span style={{ fontSize: '8.5px', color: 'var(--text-muted)' }}>{flashChipMb} MB Chip</span>
                      </div>
                    </div>
                  </div>
                </div>
              );
            })()}

            {/* Card 2: Bottom Left - ESP32-S3 Pin 48 RGB LED Mood Light */}
            <RgbControlCard
              onColorChange={(hex) => onTriggerAction(`/do?rgb_color=${encodeURIComponent(hex.replace('#', ''))}`)}
              onClearOverride={() => onTriggerAction('/do?rgb_clear=1')}
              initialColor="#38BDF8"
              currentEmotion={status?.emotion}
              rgbEnabled={status?.rgbLed?.enabled ?? true}
              showTitle={false}
              showHex={false}
              style={{ flex: 1, minHeight: '92px', boxSizing: 'border-box' }}
            />
          </div>

          {/* Right Column: Online Link (Top) + Suggestions (Bottom) */}
          <div className="mobile-bento-col">
            {/* Card 3: Top Right - Online / Offline Link Status */}
            <div
              onClick={() => onRefresh?.()}
              style={{
                background: connected
                  ? 'linear-gradient(145deg, #10B981 0%, #059669 100%)'
                  : 'linear-gradient(145deg, #EF4444 0%, #DC2626 100%)',
                borderRadius: '22px',
                padding: '13px 14px',
                color: '#FFFFFF',
                cursor: 'pointer',
                display: 'flex',
                alignItems: 'center',
                gap: '10px',
                boxShadow: connected
                  ? '0 8px 22px rgba(16, 185, 129, 0.28)'
                  : '0 8px 22px rgba(239, 68, 68, 0.28)',
                border: '1px solid rgba(255, 255, 255, 0.2)',
                transition: 'all 0.2s cubic-bezier(0.16, 1, 0.3, 1)'
              }}
            >
              <div style={{
                width: '32px',
                height: '32px',
                borderRadius: '50%',
                background: 'rgba(255, 255, 255, 0.22)',
                backdropFilter: 'blur(6px)',
                display: 'flex',
                alignItems: 'center',
                justifyContent: 'center',
                flexShrink: 0
              }}>
                {connected ? <Wifi size={16} color="#FFFFFF" /> : <WifiOff size={16} color="#FFFFFF" />}
              </div>
              <div style={{ flex: 1, minWidth: 0 }}>
                <span style={{ fontSize: '10px', opacity: 0.85, fontWeight: 600, display: 'block' }}>
                  Robot Link
                </span>
                <span style={{ fontSize: '13px', fontWeight: 800, textTransform: 'uppercase', letterSpacing: '0.04em' }}>
                  {connected ? 'ONLINE' : 'OFFLINE'}
                </span>
              </div>
            </div>

            {/* Card 4: Bottom Right - Suggestions / Smart Controls */}
            <div style={{
              background: 'var(--card-white)',
              border: '1px solid var(--border-light)',
              borderRadius: '22px',
              padding: '13px 14px',
              display: 'flex',
              flexDirection: 'column',
              justifyContent: 'space-between',
              gap: '6px',
              boxShadow: 'var(--shadow-card)',
              width: '100%',
              flex: 1,
              boxSizing: 'border-box'
            }}>
              {/* Header */}
              <div style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'center' }}>
                <span style={{ fontSize: '11px', fontWeight: 700, color: 'var(--text-dark)', textTransform: 'uppercase', letterSpacing: '0.04em' }}>
                  Suggestions
                </span>
                <span style={{
                  fontSize: '9px', fontWeight: 800, padding: '2px 7px',
                  borderRadius: '999px', background: 'var(--card-muted)', color: 'var(--text-muted)',
                  border: '1px solid var(--border-light)'
                }}>
                  Smart
                </span>
              </div>

              {/* Row 1: Daily Greetings Tile */}
              <div style={{
                background: 'var(--card-muted)',
                border: '1px solid var(--border-light)',
                borderRadius: '12px',
                padding: '6px 9px',
                display: 'flex',
                alignItems: 'center',
                justifyContent: 'space-between',
                gap: '6px',
                boxSizing: 'border-box'
              }}>
                <div style={{ flex: 1, minWidth: 0 }}>
                  <div style={{ fontSize: '10.5px', fontWeight: 700, color: 'var(--text-dark)', lineHeight: 1.2 }}>
                    Greetings
                  </div>
                  <div style={{
                    fontSize: '9px',
                    color: greetingsEnabled ? '#059669' : 'var(--text-muted)',
                    fontWeight: greetingsEnabled ? 700 : 500,
                    marginTop: '2px'
                  }}>
                    {greetingsEnabled ? 'Active' : 'Disabled'}
                  </div>
                </div>
                <button
                  type="button"
                  onClick={() => handleGreetingsToggle(!greetingsEnabled)}
                  style={{
                    width: '38px',
                    minWidth: '38px',
                    height: '24px',
                    boxSizing: 'border-box',
                    padding: 0,
                    fontSize: '10px',
                    fontWeight: 800,
                    borderRadius: '999px',
                    border: '1px solid ' + (greetingsEnabled ? 'var(--accent-lime)' : 'var(--border-light)'),
                    background: greetingsEnabled ? 'var(--accent-lime)' : 'var(--card-white)',
                    color: greetingsEnabled ? '#12141A' : 'var(--text-muted)',
                    cursor: 'pointer',
                    display: 'inline-flex',
                    alignItems: 'center',
                    justifyContent: 'center',
                    textAlign: 'center',
                    flexShrink: 0,
                    boxShadow: greetingsEnabled ? '0 2px 6px rgba(210, 250, 75, 0.3)' : 'none',
                    transition: 'all 0.18s ease'
                  }}
                >
                  {greetingsEnabled ? 'ON' : 'OFF'}
                </button>
              </div>

              {/* Row 2: Wake Word (Alexa) Tile */}
              <div style={{
                background: 'var(--card-muted)',
                border: '1px solid var(--border-light)',
                borderRadius: '12px',
                padding: '6px 9px',
                display: 'flex',
                alignItems: 'center',
                justifyContent: 'space-between',
                gap: '6px',
                boxSizing: 'border-box'
              }}>
                <div style={{ flex: 1, minWidth: 0 }}>
                  <div style={{ fontSize: '10.5px', fontWeight: 700, color: 'var(--text-dark)', lineHeight: 1.2 }}>
                    Wake Word
                  </div>
                  <div style={{
                    fontSize: '9px',
                    color: wwEnabled ? '#0284C7' : 'var(--text-muted)',
                    fontWeight: wwEnabled ? 700 : 500,
                    marginTop: '2px'
                  }}>
                    {wwEnabled ? '“Alexa”' : 'Disabled'}
                  </div>
                </div>
                <button
                  type="button"
                  onClick={() => handleWakeWordToggle(!wwEnabled)}
                  style={{
                    width: '38px',
                    minWidth: '38px',
                    height: '24px',
                    boxSizing: 'border-box',
                    padding: 0,
                    fontSize: '10px',
                    fontWeight: 800,
                    borderRadius: '999px',
                    border: '1px solid ' + (wwEnabled ? 'var(--accent-lime)' : 'var(--border-light)'),
                    background: wwEnabled ? 'var(--accent-lime)' : 'var(--card-white)',
                    color: wwEnabled ? '#12141A' : 'var(--text-muted)',
                    cursor: 'pointer',
                    display: 'inline-flex',
                    alignItems: 'center',
                    justifyContent: 'center',
                    textAlign: 'center',
                    flexShrink: 0,
                    boxShadow: wwEnabled ? '0 2px 6px rgba(210, 250, 75, 0.3)' : 'none',
                    transition: 'all 0.18s ease'
                  }}
                >
                  {wwEnabled ? 'ON' : 'OFF'}
                </button>
              </div>

              {/* Row 3: Boredom Idle Chatter Tile */}
              <div style={{
                background: 'var(--card-muted)',
                border: '1px solid var(--border-light)',
                borderRadius: '12px',
                padding: '6px 9px',
                display: 'flex',
                alignItems: 'center',
                justifyContent: 'space-between',
                gap: '6px',
                boxSizing: 'border-box'
              }}>
                <div style={{ flex: 1, minWidth: 0 }}>
                  <div style={{ fontSize: '10.5px', fontWeight: 700, color: 'var(--text-dark)', lineHeight: 1.2 }}>
                    Idle Chatter
                  </div>
                  <div style={{ width: '54px', marginTop: '3px' }}>
                    <FluxSelect
                      micro={true}
                      value={boredomMins}
                      onChange={(e) => handleBoredomInterval(e.target.value)}
                      options={MOBILE_BOREDOM_OPTIONS}
                    />
                  </div>
                </div>
                <button
                  type="button"
                  onClick={() => handleBoredomToggle(!boredomEnabled)}
                  style={{
                    width: '38px',
                    minWidth: '38px',
                    height: '24px',
                    boxSizing: 'border-box',
                    padding: 0,
                    fontSize: '10px',
                    fontWeight: 800,
                    borderRadius: '999px',
                    border: '1px solid ' + (boredomEnabled ? 'var(--accent-lime)' : 'var(--border-light)'),
                    background: boredomEnabled ? 'var(--accent-lime)' : 'var(--card-white)',
                    color: boredomEnabled ? '#12141A' : 'var(--text-muted)',
                    cursor: 'pointer',
                    display: 'inline-flex',
                    alignItems: 'center',
                    justifyContent: 'center',
                    textAlign: 'center',
                    flexShrink: 0,
                    boxShadow: boredomEnabled ? '0 2px 6px rgba(210, 250, 75, 0.3)' : 'none',
                    transition: 'all 0.18s ease'
                  }}
                >
                  {boredomEnabled ? 'ON' : 'OFF'}
                </button>
              </div>

              {/* Row 4: Speaker Audio (Mute / Unmute) Tile */}
              <div style={{
                background: 'var(--card-muted)',
                border: '1px solid var(--border-light)',
                borderRadius: '12px',
                padding: '6px 9px',
                display: 'flex',
                alignItems: 'center',
                justifyContent: 'space-between',
                gap: '6px',
                boxSizing: 'border-box'
              }}>
                <div style={{ flex: 1, minWidth: 0 }}>
                  <div style={{ fontSize: '10.5px', fontWeight: 700, color: 'var(--text-dark)', lineHeight: 1.2 }}>
                    Speaker
                  </div>
                  <div style={{
                    fontSize: '9px',
                    color: vol > 0 ? '#10B981' : '#EF4444',
                    fontWeight: 700,
                    marginTop: '2px'
                  }}>
                    {vol > 0 ? `Active (${vol}%)` : 'Muted (0%)'}
                  </div>
                </div>
                <button
                  type="button"
                  onClick={handleToggleMute}
                  style={{
                    width: '38px',
                    minWidth: '38px',
                    height: '24px',
                    boxSizing: 'border-box',
                    padding: 0,
                    fontSize: '10px',
                    fontWeight: 800,
                    borderRadius: '999px',
                    border: '1px solid ' + (vol > 0 ? 'var(--accent-lime)' : 'var(--border-light)'),
                    background: vol > 0 ? 'var(--accent-lime)' : 'var(--card-white)',
                    color: vol > 0 ? '#12141A' : 'var(--text-muted)',
                    cursor: 'pointer',
                    display: 'inline-flex',
                    alignItems: 'center',
                    justifyContent: 'center',
                    textAlign: 'center',
                    flexShrink: 0,
                    boxShadow: vol > 0 ? '0 2px 6px rgba(210, 250, 75, 0.3)' : 'none',
                    transition: 'all 0.18s ease'
                  }}
                >
                  {vol > 0 ? 'ON' : 'OFF'}
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



        {/* Hardware Sliders: Volume, Brightness & NeoPixel RGB */}
        <div className="flux-white-card" style={{ padding: '16px' }}>
          <div className="flux-card-header" style={{ marginBottom: '12px' }}>
            <span className="flux-card-title">
              <SlidersHorizontal size={15} />
              <span>Hardware Controls</span>
            </span>
            <span style={{
              display: 'inline-flex',
              alignItems: 'center',
              gap: '5px',
              fontSize: '10px',
              fontWeight: 800,
              padding: '2px 8px',
              borderRadius: '999px',
              background: connected ? 'rgba(52, 199, 89, 0.14)' : 'var(--card-muted)',
              color: connected ? '#15803D' : 'var(--text-muted)'
            }}>
              <span style={{
                width: '6px',
                height: '6px',
                borderRadius: '50%',
                background: connected ? '#22C55E' : 'var(--text-muted)'
              }} />
              <span>{connected ? 'LIVE' : 'OFFLINE'}</span>
            </span>
          </div>

          {renderHardwareSliders()}
        </div>



        {/* Sleep Engine & Night Schedule for Mobile (Matches Desktop) */}
        <div className="flux-white-card" style={{ padding: '16px', display: 'flex', flexDirection: 'column', gap: '12px' }}>
          <div className="flux-card-header" style={{ marginBottom: '0' }}>
            <span className="flux-card-title">
              <Moon size={16} color="var(--text-dark)" />
              <span>Sleep &amp; Night Schedule</span>
            </span>
          </div>

          {renderSleepSchedulePanel(false)}



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
      )}

    </div>

  )

}

