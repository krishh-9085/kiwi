import React, { useState, useEffect } from 'react'
import {
  Wifi, Key, Mic, Brain, Volume2, Hand, CloudSun, User, Save, RefreshCw, Plus, Trash2, CheckCircle2
} from 'lucide-react'
import FluxSelect from './FluxSelect'
import { Tooltip } from './base/tooltip/tooltip'
import WakeSlider from './ui/WakeSlider'

// 10 Touch Action Options from legacy HTML
const TOUCH_ACTION_OPTIONS = [
  { val: 'listen', lbl: 'Voice Prompt / Mic Record' },
  { val: 'clock', lbl: 'Show Clock Screen' },
  { val: 'weather', lbl: 'Show Weather Screen' },
  { val: 'skin', lbl: 'Cycle OLED Eye Skin' },
  { val: 'pet', lbl: 'Pet Reaction / Cuddle' },
  { val: 'tickle', lbl: 'Tickle Laugh Mode' },
  { val: 'vol_up', lbl: 'Volume Up (+10%)' },
  { val: 'vol_dn', lbl: 'Volume Down (-10%)' },
  { val: 'dismiss', lbl: 'Look Around / Dismiss' },
  { val: 'none', lbl: 'Disabled' }
]

// STT Provider & Language Options
const STT_PROVIDER_OPTIONS = [
  { val: 'sarvam', lbl: 'Sarvam AI' },
  { val: 'groq', lbl: 'Groq Whisper' }
]

const STT_LANG_OPTIONS = [
  { val: 'auto', lbl: 'Auto (Hindi + English)' },
  { val: 'hi', lbl: 'Force Hindi' },
  { val: 'en', lbl: 'Force English' }
]

const STT_SARVAM_MODELS = [
  { val: 'saaras:v4', lbl: 'saaras:v4' },
  { val: 'saaras:v3', lbl: 'saaras:v3' }
]

const STT_GROQ_MODELS = [
  { val: 'whisper-large-v3-turbo', lbl: 'whisper-large-v3-turbo (fast)' },
  { val: 'whisper-large-v3', lbl: 'whisper-large-v3 (accurate)' },
  { val: 'distil-whisper-large-v3-en', lbl: 'distil-whisper-large-v3-en' }
]

// LLM Provider & Model Options
const LLM_PROVIDER_OPTIONS = [
  { val: 'gemini', lbl: 'Google Gemini' },
  { val: 'groq', lbl: 'Groq Cloud' }
]

const LLM_GEMINI_MODELS = [
  { val: 'gemini-2.5-flash', lbl: 'gemini-2.5-flash' },
  { val: 'gemini-3.5-flash-lite', lbl: 'gemini-3.5-flash-lite' },
  { val: 'gemini-3.1-flash-lite', lbl: 'gemini-3.1-flash-lite' },
  { val: 'gemini-3.8-flash', lbl: 'gemini-3.8-flash' }
]

const LLM_GROQ_MODELS = [
  { val: 'openai/gpt-oss-120b', lbl: 'openai/gpt-oss-120b' },
  { val: 'openai/gpt-oss-20b', lbl: 'openai/gpt-oss-20b' }
]

// TTS Provider & Voice Options
const TTS_PROVIDER_OPTIONS = [
  { val: 'sarvam', lbl: 'Sarvam AI' },
  { val: 'fish', lbl: 'Fish Audio' }
]

const TTS_SARVAM_MODELS = [
  { val: 'bulbul:v3', lbl: 'bulbul:v3' },
  { val: 'bulbul:v4-flash', lbl: 'bulbul:v4-flash' }
]

const TTS_SARVAM_VOICES = [
  { val: 'amit', lbl: 'Amit (Male · Clear & Expressive)' },
  { val: 'priya', lbl: 'Priya (Female · Default)' },
  { val: 'aditya', lbl: 'Aditya (Male · Dynamic)' },
  { val: 'ritu', lbl: 'Ritu (Female · Natural)' },
  { val: 'ashutosh', lbl: 'Ashutosh (Male · Deep Voice)' },
  { val: 'neha', lbl: 'Neha (Female · Warm)' },
  { val: 'rahul', lbl: 'Rahul (Male · Casual)' },
  { val: 'pooja', lbl: 'Pooja (Female · Friendly)' },
  { val: 'rohan', lbl: 'Rohan (Male · Young)' },
  { val: 'simran', lbl: 'Simran (Female · Cheerful)' },
  { val: 'kabir', lbl: 'Kabir (Male · Authoritative)' },
  { val: 'shreya', lbl: 'Shreya (Female · Sweet)' },
  { val: 'varun', lbl: 'Varun (Male · Energetic)' },
  { val: 'ishita', lbl: 'Ishita (Female · Professional)' },
  { val: 'kavya', lbl: 'Kavya (Female · Soft)' },
  { val: 'dev', lbl: 'Dev (Male · Modern)' },
  { val: 'ratan', lbl: 'Ratan (Male · Classic)' },
  { val: 'manan', lbl: 'Manan (Male)' },
  { val: 'sumit', lbl: 'Sumit (Male)' },
  { val: 'roopa', lbl: 'Roopa (Female)' },
  { val: 'aayan', lbl: 'Aayan (Male)' },
  { val: 'shubh', lbl: 'Shubh (Male)' },
  { val: 'advait', lbl: 'Advait (Male)' },
  { val: 'anand', lbl: 'Anand (Male)' },
  { val: 'tanya', lbl: 'Tanya (Female)' },
  { val: 'tarun', lbl: 'Tarun (Male)' },
  { val: 'sunny', lbl: 'Sunny (Male)' },
  { val: 'mani', lbl: 'Mani (Male)' },
  { val: 'gokul', lbl: 'Gokul (Male)' }
]

const TTS_FISH_MODELS = [
  { val: 's2.1-pro-free', lbl: 's2.1-pro-free' },
  { val: 's1', lbl: 's1' },
  { val: 'speech-1.5', lbl: 'speech-1.5' }
]

const TTS_FISH_VOICES = [
  { val: '', lbl: 'Default Voice (Fish Audio)' },
  { val: '11a2a8934f7b4f799c6579cc9dce0c8a', lbl: 'Shinchan' },
  { val: '817eba7f1aea469f9802326568d427b9', lbl: 'Jarvis' },
  { val: '2b9e2a84cb664489a7ab9ca71af5dff5', lbl: 'Indian Girl' },
  { val: 'a32f14ad4d66457193d68abb997ed6c1', lbl: 'Doraemon' },
  { val: 'fb05f07c94d941c8be1b1ebb84422ef0', lbl: 'Alexa' },
  { val: '__custom__', lbl: '✏️ Custom Reference ID...' }
]

export default function SettingsView({
  config,
  onSaveConfig,
  onTriggerAction,
  onToast,
  getBaseUrl
}) {
  const [formData, setFormData] = useState({
    // CARD 1: WI-FI
    ssid: '',
    pass: '',

    // CARD 2: API KEYS
    llmkey: '',
    ttskey: '',
    groqkey: '',
    fishkey: '',

    // CARD 3: STT
    sttprovider: 'sarvam',
    sttlang: 'auto',
    stturl: 'https://api.sarvam.ai/speech-to-text',
    sttmodel: 'saaras:v4',
    groqstturl: 'https://api.groq.com/openai/v1/audio/transcriptions',
    groqsttmodel: 'whisper-large-v3-turbo',
    stt_trail_ms: 450,
    stt_noise_k: 2.2,
    stt_rel_k: 0.20,
    stt_max_sil: 1200,

    // CARD 4: LLM
    llmprovider: 'gemini',
    llmmodel: 'gemini-2.5-flash',
    llmurl: 'https://generativelanguage.googleapis.com/v1beta/models/gemini-2.5-flash:generateContent',
    groqmodel: 'openai/gpt-oss-120b',
    groqurl: 'https://api.groq.com/openai/v1/chat/completions',

    // CARD 5: TTS
    ttsprovider: 'sarvam',
    ttsurl: 'https://api.sarvam.ai/text-to-speech',
    ttsmodel: 'bulbul:v3',
    ttsvoice: 'priya',
    fishurl: 'https://api.fish.audio/v1/tts',
    fishmodel: 's2.1-pro-free',
    fishvoice: 'default',

    // CARD 6: TOUCH
    tch_snd: '1',
    tch_deb: 30,
    act_top_tap: 'listen',
    act_top_dbl: 'tickle',
    act_top_hld: 'skin',
    act_lt_tap: 'vol_dn',
    act_lt_dbl: 'none',
    act_rt_tap: 'vol_up',
    act_rt_dbl: 'none',
    act_lt_hld: 'pet',
    act_rt_hld: 'weather',

    // CARD 7: WEATHER & PERSONALITY
    wthr_city: '',
    wthr_lat: '',
    wthr_lon: '',
    skin: 'kiwi',
    prompt: '',

    // CARD 8: MEMORY
    mem_name: '',
    mem_friends: '',
    new_mem: '',

    // Hidden Live Preservation Parameters
    vol: 80,
    disp_bright: 100,
    night_bright: 20,
    night_mode: 0,
    circ_start: 23,
    circ_end: 7,
    circ_en: '1',
    greet_en: '1',
    snd_react: '0',
    boredom: '1',
    boredom_mins: 2,
    ww_en: '1',
    ww_model: '0',
    rgb_en: '1',
    rgb_bright: 63
  })

  const [facts, setFacts] = useState([])
  const [showAllKeys, setShowAllKeys] = useState(false)
  const [showPass, setShowPass] = useState(false)
  const [scanningWifi, setScanningWifi] = useState(false)
  const [wifiNetworks, setWifiNetworks] = useState([])
  const [saving, setSaving] = useState(false)
  const [saved, setSaved] = useState(false)
  const [customFishVoice, setCustomFishVoice] = useState(false)

  // Load config into form
  useEffect(() => {
    if (!config) return
    setFormData((prev) => {
      const loadedLlmModel = config.llm?.model || config.llmModel || prev.llmmodel
      let loadedLlmUrl = config.llm?.url || config.llmEndpoint || prev.llmurl
      if (loadedLlmUrl && loadedLlmUrl.includes('generativelanguage.googleapis.com') && loadedLlmModel) {
        loadedLlmUrl = `https://generativelanguage.googleapis.com/v1beta/models/${loadedLlmModel}:generateContent`
      }

      return {
        ...prev,
        ssid: config.wifi?.ssid || config.wifiSSID || config.ssid || prev.ssid,
        pass: config.wifi?.password || config.wifiPassword || config.pass || prev.pass,

        // API Keys
        llmkey: config.llm?.key || config.llmKey || prev.llmkey,
        ttskey: config.tts?.key || config.ttsKey || prev.ttskey,
        groqkey: config.groqKey || config.stt?.groqKey || prev.groqkey,
        fishkey: config.tts?.fishKey || config.fishKey || prev.fishkey,

        // STT
        sttprovider: config.stt?.provider || config.sttProvider || prev.sttprovider,
        sttlang: config.stt?.lang || config.sttLang || prev.sttlang,
        stturl: config.stt?.url || config.sttEndpoint || prev.stturl,
        sttmodel: config.stt?.model || config.sttModel || prev.sttmodel,
        groqstturl: config.stt?.groqUrl || config.groqSttEndpoint || prev.groqstturl,
        groqsttmodel: config.stt?.groqModel || config.groqSttModel || prev.groqsttmodel,
        stt_trail_ms: config.sttVad?.trailingMs ?? prev.stt_trail_ms,
        stt_noise_k: config.sttVad?.noiseK ?? prev.stt_noise_k,
        stt_rel_k: config.sttVad?.relK ?? prev.stt_rel_k,
        stt_max_sil: config.sttVad?.maxSilenceMs ?? prev.stt_max_sil,

        // LLM
        llmprovider: config.llm?.provider || config.llmProvider || prev.llmprovider,
        llmmodel: loadedLlmModel,
        llmurl: loadedLlmUrl,
        groqmodel: config.llm?.groqModel || config.groqModel || prev.groqmodel,
        groqurl: config.llm?.groqUrl || config.groqEndpoint || prev.groqurl,

        // TTS
        ttsprovider: config.tts?.provider || config.ttsProvider || prev.ttsprovider,
        ttsurl: config.tts?.url || config.ttsEndpoint || prev.ttsurl,
        ttsmodel: config.tts?.model || config.ttsModel || prev.ttsmodel,
        ttsvoice: config.tts?.voice || config.ttsVoice || prev.ttsvoice,
        fishurl: config.tts?.fishUrl || config.fishEndpoint || prev.fishurl,
        fishmodel: config.tts?.fishModel || config.fishModel || prev.fishmodel,
        fishvoice: config.tts?.fishVoice || config.fishVoice || prev.fishvoice,

        // Touch
        tch_snd: config.touch?.sound != null ? (config.touch.sound ? '1' : '0') : prev.tch_snd,
        tch_deb: config.touch?.deb ?? prev.tch_deb,
        act_top_tap: config.touch?.topTap || prev.act_top_tap,
        act_top_dbl: config.touch?.topDbl || prev.act_top_dbl,
        act_top_hld: config.touch?.topHld || prev.act_top_hld,
        act_lt_tap: config.touch?.ltTap || prev.act_lt_tap,
        act_lt_dbl: config.touch?.ltDbl || prev.act_lt_dbl,
        act_rt_tap: config.touch?.rtTap || prev.act_rt_tap,
        act_rt_dbl: config.touch?.rtDbl || prev.act_rt_dbl,
        act_lt_hld: config.touch?.ltHld || prev.act_lt_hld,
        act_rt_hld: config.touch?.rtHld || prev.act_rt_hld,

        // Weather & Personality
        wthr_city: config.weather?.city || config.weatherCity || prev.wthr_city,
        wthr_lat: config.weather?.lat || config.weatherLat || prev.wthr_lat,
        wthr_lon: config.weather?.lon || config.weatherLon || prev.wthr_lon,
        skin: config.skin || config.currentEyeSkin || prev.skin,
        prompt: config.prompt != null ? config.prompt : (config.systemPrompt != null ? config.systemPrompt : prev.prompt),

        // Memory
        mem_name: config.memory?.name || config.ownerName || prev.mem_name,
        mem_friends: config.memory?.friends || prev.mem_friends,

        // Hidden live preservation
        vol: config.volume ?? config.speakerVolume ?? prev.vol,
        disp_bright: config.brightness ?? config.displayBrightness ?? prev.disp_bright,
        night_bright: config.nightBrightness ?? prev.night_bright,
        night_mode: config.nightMode ?? prev.night_mode,
        circ_start: config.circadian?.start ?? prev.circ_start,
        circ_end: config.circadian?.end ?? prev.circ_end,
        circ_en: config.circadian?.enabled != null ? (config.circadian.enabled ? '1' : '0') : prev.circ_en,
        greet_en: config.greetings?.enabled != null ? (config.greetings.enabled ? '1' : '0') : prev.greet_en,
        snd_react: config.soundReactive?.enabled != null ? (config.soundReactive.enabled ? '1' : '0') : prev.snd_react,
        boredom: config.boredom?.enabled != null ? (config.boredom.enabled ? '1' : '0') : prev.boredom,
        boredom_mins: config.boredom?.minutes ?? prev.boredom_mins,
        ww_en: config.wakeWord?.enabled != null ? (config.wakeWord.enabled ? '1' : '0') : prev.ww_en,
        ww_model: String(config.wakeWord?.model ?? prev.ww_model),
        rgb_en: config.rgbLed?.enabled != null ? (config.rgbLed.enabled ? '1' : '0') : prev.rgb_en,
        rgb_bright: config.rgbLed?.brightness ?? prev.rgb_bright
      }
    })

    if (config.memory?.facts && Array.isArray(config.memory.facts)) {
      setFacts(config.memory.facts)
    }
  }, [config])

  const handleChange = (e) => {
    const { name, value, type, checked } = e.target
    const val = type === 'checkbox' ? (checked ? '1' : '0') : value
    setFormData((prev) => {
      const updated = {
        ...prev,
        [name]: val
      }

      // Auto-update Gemini Endpoint URL when Gemini Model dropdown is changed
      if (name === 'llmmodel') {
        if (!prev.llmurl || prev.llmurl.includes('generativelanguage.googleapis.com')) {
          updated.llmurl = `https://generativelanguage.googleapis.com/v1beta/models/${val}:generateContent`
        }
      }

      return updated
    })
  }

  const handleScanWifi = async () => {
    setScanningWifi(true)
    try {
      const res = await fetch(`${getBaseUrl()}/scan`)
      const data = await res.json()
      if (Array.isArray(data)) {
        setWifiNetworks(data)
        onToast(`Found ${data.length} Wi-Fi networks!`)
      }
    } catch {
      onToast('Wi-Fi scan failed')
    }
    setScanningWifi(false)
  }

  const handleAddFact = () => {
    const txt = formData.new_mem.trim()
    if (!txt) return
    setFacts((prev) => [...prev, txt])
    setFormData((prev) => ({ ...prev, new_mem: '' }))
    onToast('Fact queued to save!')
  }

  const handleDeleteFact = (idx) => {
    setFacts((prev) => prev.filter((_, i) => i !== idx))
    onToast('Fact removed!')
  }

  const handleSubmit = async (e) => {
    e.preventDefault()
    setSaving(true)
    const payload = new URLSearchParams()
    Object.entries(formData).forEach(([k, v]) => {
      payload.append(k, String(v))
    })
    facts.forEach((f) => payload.append('facts[]', f))

    try {
      await onSaveConfig(payload)
      setSaved(true)
      onToast('Configuration saved & applied successfully!')
      setTimeout(() => setSaved(false), 3000)
    } catch {
      onToast('Failed to save configuration. Please retry.')
    } finally {
      setSaving(false)
    }
  }

  return (
    <form onSubmit={handleSubmit} style={{ display: 'flex', flexDirection: 'column', gap: '20px' }}>
      {/* Hidden Live Parameter Preservation across Form Saves */}
      <input type="hidden" name="vol" value={formData.vol} />
      <input type="hidden" name="disp_bright" value={formData.disp_bright} />
      <input type="hidden" name="night_bright" value={formData.night_bright} />
      <input type="hidden" name="night_mode" value={formData.night_mode} />
      <input type="hidden" name="circ_start" value={formData.circ_start} />
      <input type="hidden" name="circ_end" value={formData.circ_end} />
      <input type="hidden" name="circ_en" value={formData.circ_en} />
      <input type="hidden" name="greet_en" value={formData.greet_en} />
      <input type="hidden" name="snd_react" value={formData.snd_react} />
      <input type="hidden" name="boredom" value={formData.boredom} />
      <input type="hidden" name="boredom_mins" value={formData.boredom_mins} />
      <input type="hidden" name="ww_en" value={formData.ww_en} />
      <input type="hidden" name="ww_model" value={formData.ww_model} />
      <input type="hidden" name="rgb_en" value={formData.rgb_en} />
      <input type="hidden" name="rgb_bright" value={formData.rgb_bright} />

      {/* Top Header Bar */}
      <div className="flux-title-section" style={{ alignItems: 'center' }}>
        <div>
          <h1 className="flux-main-title">System Settings</h1>
        </div>
      </div>

      {/* ================= CARD 1: WI-FI & LOCAL NETWORK SETUP ================= */}
      <div className="flux-white-card">
        <div className="flux-card-header">
          <span className="flux-card-title">
            <Wifi size={16} />
            <span>Wi-Fi</span>
          </span>
          <Tooltip title="Scan Networks">
            <button
              type="button"
              onClick={handleScanWifi}
              disabled={scanningWifi}
              className="flux-btn"
              style={{ padding: '6px 14px', fontSize: '11px', background: 'var(--card-muted)', color: 'var(--text-dark)', border: '1px solid var(--border-light)' }}
            >
              <RefreshCw size={12} className={scanningWifi ? 'pulse-active' : ''} />
              <span>{scanningWifi ? 'Scanning...' : 'Scan'}</span>
            </button>
          </Tooltip>
        </div>

        {wifiNetworks.length > 0 && (
          <div style={{ display: 'flex', gap: '8px', flexWrap: 'wrap', marginBottom: '16px', background: 'var(--card-subtle)', padding: '10px', borderRadius: '12px' }}>
            <span style={{ fontSize: '11px', fontWeight: 700, color: 'var(--text-muted)', alignSelf: 'center', marginRight: '6px' }}>
              DETECTED 2.4GHz NETWORKS:
            </span>
            {wifiNetworks.map((net, i) => (
              <button
                key={i}
                type="button"
                onClick={() => setFormData((prev) => ({ ...prev, ssid: net.ssid }))}
                className="flux-btn"
                style={{ padding: '4px 10px', fontSize: '11px', background: 'var(--card-white)', color: 'var(--text-dark)', border: '1px solid var(--border-light)' }}
              >
                <span>{net.ssid}</span>
                <span style={{ fontSize: '10px', color: 'var(--text-muted)' }}>({net.rssi} dBm)</span>
              </button>
            ))}
          </div>
        )}

        <div className="flux-grid-2">
          <div>
            <label style={{ fontSize: '12px', fontWeight: 600, color: 'var(--text-muted)', display: 'block', marginBottom: '6px' }}>
              WI-FI NETWORK (SSID) *
            </label>
            <input
              type="text"
              name="ssid"
              value={formData.ssid}
              onChange={handleChange}
              placeholder="Your 2.4GHz Wi-Fi Name"
              style={{
                width: '100%', padding: '12px 16px', borderRadius: '12px',
                background: 'var(--card-muted)', border: '1px solid var(--border-light)', color: 'var(--text-dark)', fontSize: '13px', outline: 'none'
              }}
            />
          </div>

          <div>
            <div style={{ display: 'flex', justifyContent: 'space-between', marginBottom: '6px' }}>
              <label style={{ fontSize: '12px', fontWeight: 600, color: 'var(--text-muted)' }}>
                WI-FI PASSWORD
              </label>
              <button
                type="button"
                onClick={() => setShowPass(!showPass)}
                style={{ background: 'transparent', border: 'none', color: 'var(--text-dark)', fontSize: '11px', cursor: 'pointer', fontWeight: 700 }}
              >
                {showPass ? 'Hide' : 'Show'}
              </button>
            </div>
            <input
              type={showPass ? 'text' : 'password'}
              name="pass"
              value={formData.pass}
              onChange={handleChange}
              placeholder="••••••••"
              style={{
                width: '100%', padding: '12px 16px', borderRadius: '12px',
                background: 'var(--card-muted)', border: '1px solid var(--border-light)', color: 'var(--text-dark)', fontSize: '13px', outline: 'none'
              }}
            />
          </div>
        </div>
      </div>

      {/* ================= CARD 2: CLOUD API KEYS & CREDENTIALS ================= */}
      <div className="flux-white-card">
        <div className="flux-card-header">
          <span className="flux-card-title">
            <Key size={16} />
            <span>API Keys</span>
          </span>
          <label className="flux-toggle">
            <input
              type="checkbox"
              checked={showAllKeys}
              onChange={(e) => setShowAllKeys(e.target.checked)}
            />
            <span className="flux-toggle-slider" />
            <span style={{ fontSize: '12px', fontWeight: 600, color: 'var(--text-dark)' }}>
              Unhide / Hide
            </span>
          </label>
        </div>

        <div className="flux-grid-autofit">
          <div>
            <label style={{ fontSize: '11px', fontWeight: 700, color: 'var(--text-muted)', display: 'block', marginBottom: '6px' }}>
              GOOGLE GEMINI API
            </label>
            <input
              type={showAllKeys ? 'text' : 'password'}
              name="llmkey"
              value={formData.llmkey}
              onChange={handleChange}
              placeholder="AIzaSy..."
              style={{
                width: '100%', padding: '11px 14px', borderRadius: '12px',
                background: 'var(--card-muted)', border: '1px solid var(--border-light)', color: 'var(--text-dark)', fontSize: '13px', outline: 'none'
              }}
            />
          </div>

          <div>
            <label style={{ fontSize: '11px', fontWeight: 700, color: 'var(--text-muted)', display: 'block', marginBottom: '6px' }}>
              SARVAM AI API
            </label>
            <input
              type={showAllKeys ? 'text' : 'password'}
              name="ttskey"
              value={formData.ttskey}
              onChange={handleChange}
              placeholder="sk_..."
              style={{
                width: '100%', padding: '11px 14px', borderRadius: '12px',
                background: 'var(--card-muted)', border: '1px solid var(--border-light)', color: 'var(--text-dark)', fontSize: '13px', outline: 'none'
              }}
            />
          </div>

          <div>
            <label style={{ fontSize: '11px', fontWeight: 700, color: 'var(--text-muted)', display: 'block', marginBottom: '6px' }}>
              GROQ CLOUD API
            </label>
            <input
              type={showAllKeys ? 'text' : 'password'}
              name="groqkey"
              value={formData.groqkey}
              onChange={handleChange}
              placeholder="gsk_..."
              style={{
                width: '100%', padding: '11px 14px', borderRadius: '12px',
                background: 'var(--card-muted)', border: '1px solid var(--border-light)', color: 'var(--text-dark)', fontSize: '13px', outline: 'none'
              }}
            />
          </div>

          <div>
            <label style={{ fontSize: '11px', fontWeight: 700, color: 'var(--text-muted)', display: 'block', marginBottom: '6px' }}>
              FISH AUDIO API
            </label>
            <input
              type={showAllKeys ? 'text' : 'password'}
              name="fishkey"
              value={formData.fishkey}
              onChange={handleChange}
              placeholder="fa_..."
              style={{
                width: '100%', padding: '11px 14px', borderRadius: '12px',
                background: 'var(--card-muted)', border: '1px solid var(--border-light)', color: 'var(--text-dark)', fontSize: '13px', outline: 'none'
              }}
            />
          </div>
        </div>
      </div>

      {/* ================= CARD 3: 1. SPEECH-TO-TEXT (STT) ================= */}
      <div className="flux-white-card">
        <div className="flux-card-header">
          <span className="flux-card-title">
            <Mic size={16} />
            <span>1. SPEECH-TO-TEXT (STT)</span>
          </span>
        </div>

        <div className="flux-grid-2" style={{ marginBottom: '16px' }}>
          <div>
            <label style={{ fontSize: '11px', fontWeight: 700, color: 'var(--text-muted)', display: 'block', marginBottom: '6px' }}>
              STT PROVIDER
            </label>
            <FluxSelect
              name="sttprovider"
              value={formData.sttprovider}
              onChange={handleChange}
              options={STT_PROVIDER_OPTIONS}
            />
          </div>

          <div>
            <label style={{ fontSize: '11px', fontWeight: 700, color: 'var(--text-muted)', display: 'block', marginBottom: '6px' }}>
              STT LANGUAGE
            </label>
            <FluxSelect
              name="sttlang"
              value={formData.sttlang}
              onChange={handleChange}
              options={STT_LANG_OPTIONS}
            />
          </div>
        </div>

        {/* Dynamic Provider Box */}
        <div style={{ background: 'var(--card-subtle)', border: '1px solid var(--border-light)', borderRadius: '14px', padding: '16px' }}>
          {formData.sttprovider === 'sarvam' && (
            <div>
              <h3 style={{ fontSize: '13px', fontWeight: 800, color: 'var(--text-dark)', marginBottom: '12px' }}>Sarvam AI Settings</h3>
              <div className="flux-grid-2">
                <div>
                  <label style={{ fontSize: '11px', fontWeight: 600, color: 'var(--text-muted)', display: 'block', marginBottom: '4px' }}>Endpoint</label>
                  <input
                    type="url"
                    name="stturl"
                    value={formData.stturl}
                    onChange={handleChange}
                    placeholder="https://api.sarvam.ai/speech-to-text"
                    style={{ width: '100%', padding: '10px 14px', borderRadius: '10px', background: 'var(--card-white)', border: '1px solid var(--border-light)', color: 'var(--text-dark)', fontSize: '12px', outline: 'none' }}
                  />
                </div>
                <div>
                  <label style={{ fontSize: '11px', fontWeight: 600, color: 'var(--text-muted)', display: 'block', marginBottom: '4px' }}>Model</label>
                  <FluxSelect name="sttmodel" value={formData.sttmodel} onChange={handleChange} options={STT_SARVAM_MODELS} />
                </div>
              </div>
            </div>
          )}

          {formData.sttprovider === 'groq' && (
            <div>
              <h3 style={{ fontSize: '13px', fontWeight: 800, color: 'var(--text-dark)', marginBottom: '12px' }}>Groq Whisper Settings</h3>
              <div className="flux-grid-2">
                <div>
                  <label style={{ fontSize: '11px', fontWeight: 600, color: 'var(--text-muted)', display: 'block', marginBottom: '4px' }}>Endpoint</label>
                  <input
                    type="url"
                    name="groqstturl"
                    value={formData.groqstturl}
                    onChange={handleChange}
                    placeholder="https://api.groq.com/openai/v1/audio/transcriptions"
                    style={{ width: '100%', padding: '10px 14px', borderRadius: '10px', background: 'var(--card-white)', border: '1px solid var(--border-light)', color: 'var(--text-dark)', fontSize: '12px', outline: 'none' }}
                  />
                </div>
                <div>
                  <label style={{ fontSize: '11px', fontWeight: 600, color: 'var(--text-muted)', display: 'block', marginBottom: '4px' }}>Model</label>
                  <FluxSelect name="groqsttmodel" value={formData.groqsttmodel} onChange={handleChange} options={STT_GROQ_MODELS} />
                </div>
              </div>
            </div>
          )}
        </div>

        {/* Adaptive Voice Endpointer (VAD) Controls */}
        <div style={{ marginTop: '16px', background: 'var(--card-subtle)', border: '1px solid var(--border-light)', borderRadius: '14px', padding: '16px' }}>
          <h3 style={{ fontSize: '13px', fontWeight: 800, color: 'var(--text-dark)', marginBottom: '12px' }}>
            Adaptive Voice Detection (VAD) Endpointer
          </h3>
          <div className="flux-grid-2" style={{ gap: '12px' }}>
            <div>
              <label style={{ fontSize: '11px', fontWeight: 600, color: 'var(--text-muted)', display: 'block', marginBottom: '4px' }}>
                Trailing Silence Cutoff (ms)
              </label>
              <input
                type="number"
                name="stt_trail_ms"
                min="100"
                max="3000"
                step="50"
                value={formData.stt_trail_ms}
                onChange={handleChange}
                style={{ width: '100%', padding: '10px 14px', borderRadius: '10px', background: 'var(--card-white)', border: '1px solid var(--border-light)', color: 'var(--text-dark)', fontSize: '12px', outline: 'none' }}
              />
              <span style={{ fontSize: '10px', color: 'var(--text-muted)' }}>Default 450 ms. Cutoff wait after speech finishes.</span>
            </div>
            <div>
              <label style={{ fontSize: '11px', fontWeight: 600, color: 'var(--text-muted)', display: 'block', marginBottom: '4px' }}>
                Noise Floor Multiplier (K_noise)
              </label>
              <input
                type="number"
                name="stt_noise_k"
                min="1.1"
                max="10.0"
                step="0.1"
                value={formData.stt_noise_k}
                onChange={handleChange}
                style={{ width: '100%', padding: '10px 14px', borderRadius: '10px', background: 'var(--card-white)', border: '1px solid var(--border-light)', color: 'var(--text-dark)', fontSize: '12px', outline: 'none' }}
              />
              <span style={{ fontSize: '10px', color: 'var(--text-muted)' }}>Default 2.2x. Speech gate = noiseFloor * K.</span>
            </div>
            <div>
              <label style={{ fontSize: '11px', fontWeight: 600, color: 'var(--text-muted)', display: 'block', marginBottom: '4px' }}>
                Relative Peak Multiplier (K_rel)
              </label>
              <input
                type="number"
                name="stt_rel_k"
                min="0.05"
                max="1.0"
                step="0.05"
                value={formData.stt_rel_k}
                onChange={handleChange}
                style={{ width: '100%', padding: '10px 14px', borderRadius: '10px', background: 'var(--card-white)', border: '1px solid var(--border-light)', color: 'var(--text-dark)', fontSize: '12px', outline: 'none' }}
              />
              <span style={{ fontSize: '10px', color: 'var(--text-muted)' }}>Default 0.20 (20%). Silence threshold above noise.</span>
            </div>
            <div>
              <label style={{ fontSize: '11px', fontWeight: 600, color: 'var(--text-muted)', display: 'block', marginBottom: '4px' }}>
                Max Silence Hard-Cap (ms)
              </label>
              <input
                type="number"
                name="stt_max_sil"
                min="300"
                max="5000"
                step="100"
                value={formData.stt_max_sil}
                onChange={handleChange}
                style={{ width: '100%', padding: '10px 14px', borderRadius: '10px', background: 'var(--card-white)', border: '1px solid var(--border-light)', color: 'var(--text-dark)', fontSize: '12px', outline: 'none' }}
              />
              <span style={{ fontSize: '10px', color: 'var(--text-muted)' }}>Default 1200 ms. Absolute silence cap after speech.</span>
            </div>
          </div>
        </div>
      </div>

      {/* ================= CARD 4: 2. AI INTELLIGENCE (LLM) ================= */}
      <div className="flux-white-card">
        <div className="flux-card-header">
          <span className="flux-card-title">
            <Brain size={16} />
            <span>2. AI INTELLIGENCE (LLM)</span>
          </span>
        </div>

        <div style={{ marginBottom: '16px' }}>
          <label style={{ fontSize: '11px', fontWeight: 700, color: 'var(--text-muted)', display: 'block', marginBottom: '6px' }}>
            LLM PROVIDER
          </label>
          <FluxSelect
            name="llmprovider"
            value={formData.llmprovider}
            onChange={handleChange}
            options={LLM_PROVIDER_OPTIONS}
          />
        </div>

        {/* Dynamic Provider Box */}
        <div style={{ background: 'var(--card-subtle)', border: '1px solid var(--border-light)', borderRadius: '14px', padding: '16px' }}>
          {formData.llmprovider === 'gemini' && (
            <div>
              <h3 style={{ fontSize: '13px', fontWeight: 800, color: 'var(--text-dark)', marginBottom: '12px' }}>Gemini Model &amp; Settings</h3>
              <div className="flux-grid-2">
                <div>
                  <label style={{ fontSize: '11px', fontWeight: 600, color: 'var(--text-muted)', display: 'block', marginBottom: '4px' }}>Model</label>
                  <FluxSelect name="llmmodel" value={formData.llmmodel} onChange={handleChange} options={LLM_GEMINI_MODELS} />
                </div>
                <div>
                  <label style={{ fontSize: '11px', fontWeight: 600, color: 'var(--text-muted)', display: 'block', marginBottom: '4px' }}>Endpoint</label>
                  <input
                    type="url"
                    name="llmurl"
                    value={formData.llmurl}
                    onChange={handleChange}
                    style={{ width: '100%', padding: '10px 14px', borderRadius: '10px', background: 'var(--card-white)', border: '1px solid var(--border-light)', color: 'var(--text-dark)', fontSize: '12px', outline: 'none' }}
                  />
                </div>
              </div>
            </div>
          )}

          {formData.llmprovider === 'groq' && (
            <div>
              <h3 style={{ fontSize: '13px', fontWeight: 800, color: 'var(--text-dark)', marginBottom: '12px' }}>Groq Model &amp; Settings</h3>
              <div className="flux-grid-2">
                <div>
                  <label style={{ fontSize: '11px', fontWeight: 600, color: 'var(--text-muted)', display: 'block', marginBottom: '4px' }}>Model</label>
                  <FluxSelect name="groqmodel" value={formData.groqmodel} onChange={handleChange} options={LLM_GROQ_MODELS} />
                </div>
                <div>
                  <label style={{ fontSize: '11px', fontWeight: 600, color: 'var(--text-muted)', display: 'block', marginBottom: '4px' }}>Endpoint</label>
                  <input
                    type="url"
                    name="groqurl"
                    value={formData.groqurl}
                    onChange={handleChange}
                    style={{ width: '100%', padding: '10px 14px', borderRadius: '10px', background: 'var(--card-white)', border: '1px solid var(--border-light)', color: 'var(--text-dark)', fontSize: '12px', outline: 'none' }}
                  />
                </div>
              </div>
            </div>
          )}
        </div>
      </div>

      {/* ================= CARD 5: 3. TEXT-TO-SPEECH (TTS) ================= */}
      <div className="flux-white-card">
        <div className="flux-card-header">
          <span className="flux-card-title">
            <Volume2 size={16} />
            <span>3. TEXT-TO-SPEECH (TTS)</span>
          </span>
        </div>

        <div style={{ marginBottom: '16px' }}>
          <label style={{ fontSize: '11px', fontWeight: 700, color: 'var(--text-muted)', display: 'block', marginBottom: '6px' }}>
            TTS PROVIDER
          </label>
          <FluxSelect
            name="ttsprovider"
            value={formData.ttsprovider}
            onChange={handleChange}
            options={TTS_PROVIDER_OPTIONS}
          />
        </div>

        {/* Dynamic Provider Box */}
        <div style={{ background: 'var(--card-subtle)', border: '1px solid var(--border-light)', borderRadius: '14px', padding: '16px' }}>
          {formData.ttsprovider === 'sarvam' && (
            <div>
              <h3 style={{ fontSize: '13px', fontWeight: 800, color: 'var(--text-dark)', marginBottom: '12px' }}>Sarvam TTS Settings</h3>
              <div className="flux-grid-autofit">
                <div>
                  <label style={{ fontSize: '11px', fontWeight: 600, color: 'var(--text-muted)', display: 'block', marginBottom: '4px' }}>Endpoint</label>
                  <input
                    type="url"
                    name="ttsurl"
                    value={formData.ttsurl}
                    onChange={handleChange}
                    style={{ width: '100%', padding: '10px 14px', borderRadius: '10px', background: 'var(--card-white)', border: '1px solid var(--border-light)', color: 'var(--text-dark)', fontSize: '12px', outline: 'none' }}
                  />
                </div>
                <div>
                  <label style={{ fontSize: '11px', fontWeight: 600, color: 'var(--text-muted)', display: 'block', marginBottom: '4px' }}>Model</label>
                  <FluxSelect name="ttsmodel" value={formData.ttsmodel} onChange={handleChange} options={TTS_SARVAM_MODELS} />
                </div>
                <div>
                  <label style={{ fontSize: '11px', fontWeight: 600, color: 'var(--text-muted)', display: 'block', marginBottom: '4px' }}>Voice / Speaker</label>
                  <FluxSelect name="ttsvoice" value={formData.ttsvoice} onChange={handleChange} options={TTS_SARVAM_VOICES} />
                </div>
              </div>
            </div>
          )}

          {formData.ttsprovider === 'fish' && (
            <div>
              <h3 style={{ fontSize: '13px', fontWeight: 800, color: 'var(--text-dark)', marginBottom: '12px' }}>Fish Audio Settings</h3>
              <div className="flux-grid-autofit">
                <div>
                  <label style={{ fontSize: '11px', fontWeight: 600, color: 'var(--text-muted)', display: 'block', marginBottom: '4px' }}>Endpoint</label>
                  <input
                    type="url"
                    name="fishurl"
                    value={formData.fishurl}
                    onChange={handleChange}
                    style={{ width: '100%', padding: '10px 14px', borderRadius: '10px', background: 'var(--card-white)', border: '1px solid var(--border-light)', color: 'var(--text-dark)', fontSize: '12px', outline: 'none' }}
                  />
                </div>
                <div>
                  <label style={{ fontSize: '11px', fontWeight: 600, color: 'var(--text-muted)', display: 'block', marginBottom: '4px' }}>Model</label>
                  <FluxSelect name="fishmodel" value={formData.fishmodel} onChange={handleChange} options={TTS_FISH_MODELS} />
                </div>
                <div>
                  <label style={{ fontSize: '11px', fontWeight: 600, color: 'var(--text-muted)', display: 'block', marginBottom: '4px' }}>
                    Voice / Reference ID
                  </label>
                  <FluxSelect
                    name="fishvoice_select"
                    value={
                      customFishVoice
                        ? '__custom__'
                        : (TTS_FISH_VOICES.some((v) => v.val === formData.fishvoice && v.val !== '__custom__' && v.val !== '')
                          ? formData.fishvoice
                          : (formData.fishvoice === 'default' || !formData.fishvoice ? '' : '__custom__'))
                    }
                    onChange={(e) => {
                      const sel = e.target.value
                      if (sel === '__custom__' || sel === 'custom') {
                        setCustomFishVoice(true)
                      } else {
                        setCustomFishVoice(false)
                        handleChange({ target: { name: 'fishvoice', value: sel } })
                      }
                    }}
                    options={TTS_FISH_VOICES}
                  />

                  {/* Preset Reference ID badge */}
                  {TTS_FISH_VOICES.some((v) => v.val === formData.fishvoice && v.val !== '' && v.val !== '__custom__') && !customFishVoice && (
                    <div style={{
                      marginTop: '6px',
                      fontSize: '10px',
                      fontFamily: 'monospace',
                      color: 'var(--text-muted)',
                      display: 'flex',
                      alignItems: 'center',
                      gap: '4px',
                      wordBreak: 'break-all'
                    }}>
                      <span style={{ color: 'var(--text-dim)', fontWeight: 700 }}>ID:</span>
                      <span style={{
                        color: 'var(--text-dark)',
                        background: 'var(--card-muted)',
                        padding: '1px 6px',
                        borderRadius: '4px',
                        border: '1px solid var(--border-light)'
                      }}>
                        {formData.fishvoice}
                      </span>
                    </div>
                  )}

                  {/* Custom Reference ID Hex Input */}
                  {(customFishVoice || (!TTS_FISH_VOICES.some((v) => v.val === formData.fishvoice) && formData.fishvoice && formData.fishvoice !== 'default')) && (
                    <div style={{ marginTop: '8px' }}>
                      <input
                        type="text"
                        name="fishvoice"
                        value={formData.fishvoice === 'default' ? '' : formData.fishvoice}
                        onChange={handleChange}
                        placeholder="Paste 32-character reference ID hex..."
                        style={{
                          width: '100%',
                          padding: '9px 12px',
                          borderRadius: '10px',
                          background: 'var(--card-white)',
                          border: '1px solid var(--border-light)',
                          color: 'var(--text-dark)',
                          fontSize: '11.5px',
                          fontFamily: 'monospace',
                          outline: 'none'
                        }}
                      />
                    </div>
                  )}
                </div>
              </div>
            </div>
          )}
        </div>
      </div>

      {/* ================= CARD 6: CAPACITIVE TOUCH MAPPING & GESTURES ================= */}
      <div className="flux-white-card">
        <div className="flux-card-header">
          <span className="flux-card-title">
            <Hand size={16} />
            <span>Touch &amp; Gestures</span>
          </span>
          <label className="flux-toggle">
            <input
              type="checkbox"
              name="tch_snd"
              checked={formData.tch_snd === '1'}
              onChange={handleChange}
            />
            <span className="flux-toggle-slider" />
            <span style={{ fontSize: '12px', fontWeight: 700, color: 'var(--text-dark)', whiteSpace: 'nowrap' }}>
              Touch Sound
            </span>
          </label>
        </div>

        {/* Debounce Filter Slider with React Bits WakeSlider */}
        <div style={{ marginBottom: '18px' }}>
          <div style={{ display: 'flex', justifyContent: 'space-between', fontSize: '12px', marginBottom: '8px' }}>
            <span style={{ fontWeight: 600, color: 'var(--text-muted)' }}>
              Debounce Filter / Sensitivity: <strong style={{ color: 'var(--text-dark)' }}>{formData.tch_deb} ms</strong>
            </span>
          </div>
          <div style={{ maxWidth: '340px' }}>
            <WakeSlider
              value={Number(formData.tch_deb || 50)}
              min={10}
              max={150}
              step={5}
              bars={20}
              height={32}
              restHeight={16}
              gap={4}
              fillColor="var(--slider-fill)"
              trackColor="var(--slider-track)"
              crestColor="var(--accent-lime)"
              onChange={(v) => setFormData((prev) => ({ ...prev, tch_deb: v }))}
              ariaLabel="Touch Debounce Filter"
            />
          </div>
        </div>

        {/* 9 Touch Gestures in a 3x3 Grid */}
        <div className="flux-grid-autofit">
          <div>
            <label style={{ fontSize: '11px', fontWeight: 600, color: 'var(--text-muted)', display: 'block', marginBottom: '4px' }}>
              Top Head (Single Tap)
            </label>
            <FluxSelect name="act_top_tap" value={formData.act_top_tap} onChange={handleChange} options={TOUCH_ACTION_OPTIONS} />
          </div>

          <div>
            <label style={{ fontSize: '11px', fontWeight: 600, color: 'var(--text-muted)', display: 'block', marginBottom: '4px' }}>
              Top Head (Double Tap)
            </label>
            <FluxSelect name="act_top_dbl" value={formData.act_top_dbl} onChange={handleChange} options={TOUCH_ACTION_OPTIONS} />
          </div>

          <div>
            <label style={{ fontSize: '11px', fontWeight: 600, color: 'var(--text-muted)', display: 'block', marginBottom: '4px' }}>
              Top Head (Hold 3s)
            </label>
            <FluxSelect name="act_top_hld" value={formData.act_top_hld} onChange={handleChange} options={TOUCH_ACTION_OPTIONS} />
          </div>

          <div>
            <label style={{ fontSize: '11px', fontWeight: 600, color: 'var(--text-muted)', display: 'block', marginBottom: '4px' }}>
              Left Cheek (Single Tap)
            </label>
            <FluxSelect name="act_lt_tap" value={formData.act_lt_tap} onChange={handleChange} options={TOUCH_ACTION_OPTIONS} />
          </div>

          <div>
            <label style={{ fontSize: '11px', fontWeight: 600, color: 'var(--text-muted)', display: 'block', marginBottom: '4px' }}>
              Left Cheek (Double Tap)
            </label>
            <FluxSelect name="act_lt_dbl" value={formData.act_lt_dbl} onChange={handleChange} options={TOUCH_ACTION_OPTIONS} />
          </div>

          <div>
            <label style={{ fontSize: '11px', fontWeight: 600, color: 'var(--text-muted)', display: 'block', marginBottom: '4px' }}>
              Left Cheek (Hold 1.5s)
            </label>
            <FluxSelect name="act_lt_hld" value={formData.act_lt_hld} onChange={handleChange} options={TOUCH_ACTION_OPTIONS} />
          </div>

          <div>
            <label style={{ fontSize: '11px', fontWeight: 600, color: 'var(--text-muted)', display: 'block', marginBottom: '4px' }}>
              Right Cheek (Single Tap)
            </label>
            <FluxSelect name="act_rt_tap" value={formData.act_rt_tap} onChange={handleChange} options={TOUCH_ACTION_OPTIONS} />
          </div>

          <div>
            <label style={{ fontSize: '11px', fontWeight: 600, color: 'var(--text-muted)', display: 'block', marginBottom: '4px' }}>
              Right Cheek (Double Tap)
            </label>
            <FluxSelect name="act_rt_dbl" value={formData.act_rt_dbl} onChange={handleChange} options={TOUCH_ACTION_OPTIONS} />
          </div>

          <div>
            <label style={{ fontSize: '11px', fontWeight: 600, color: 'var(--text-muted)', display: 'block', marginBottom: '4px' }}>
              Right Cheek (Hold 1.5s)
            </label>
            <FluxSelect name="act_rt_hld" value={formData.act_rt_hld} onChange={handleChange} options={TOUCH_ACTION_OPTIONS} />
          </div>
        </div>

        <small style={{ display: 'block', marginTop: '12px', color: 'var(--text-muted)', fontSize: '12px' }}>
          Tip: Hold Top Head for 3s to change face skin; hold for 7s to enter Night Sleep.
        </small>
      </div>

      {/* ================= CARD 7: WEATHER & PERSONALITY ================= */}
      <div className="flux-white-card">
        <div className="flux-card-header">
          <span className="flux-card-title">
            <CloudSun size={16} />
            <span>Weather &amp; Personality</span>
          </span>
        </div>

        <div style={{ marginBottom: '14px' }}>
          <label style={{ fontSize: '11px', fontWeight: 600, color: 'var(--text-muted)', display: 'block', marginBottom: '4px' }}>
            City Name
          </label>
          <input
            type="text"
            name="wthr_city"
            value={formData.wthr_city}
            onChange={handleChange}
            placeholder="e.g. Mumbai, New Delhi"
            style={{ width: '100%', padding: '11px 16px', borderRadius: '12px', background: 'var(--card-muted)', border: '1px solid var(--border-light)', fontSize: '13px', outline: 'none' }}
          />
        </div>

        <div className="flux-grid-2" style={{ marginBottom: '14px' }}>
          <div>
            <label style={{ fontSize: '11px', fontWeight: 600, color: 'var(--text-muted)', display: 'block', marginBottom: '4px' }}>
              Latitude (-90 to 90)
            </label>
            <input
              type="number"
              step="any"
              min="-90"
              max="90"
              name="wthr_lat"
              value={formData.wthr_lat}
              onChange={handleChange}
              placeholder="e.g. 19.0760"
              style={{ width: '100%', padding: '11px 16px', borderRadius: '12px', background: 'var(--card-muted)', border: '1px solid var(--border-light)', color: 'var(--text-dark)', fontSize: '13px', outline: 'none' }}
            />
          </div>

          <div>
            <label style={{ fontSize: '11px', fontWeight: 600, color: 'var(--text-muted)', display: 'block', marginBottom: '4px' }}>
              Longitude (-180 to 180)
            </label>
            <input
              type="number"
              step="any"
              min="-180"
              max="180"
              name="wthr_lon"
              value={formData.wthr_lon}
              onChange={handleChange}
              placeholder="e.g. 72.8777"
              style={{ width: '100%', padding: '11px 16px', borderRadius: '12px', background: 'var(--card-muted)', border: '1px solid var(--border-light)', color: 'var(--text-dark)', fontSize: '13px', outline: 'none' }}
            />
          </div>
        </div>

        <div>
          <label style={{ fontSize: '11px', fontWeight: 600, color: 'var(--text-muted)', display: 'block', marginBottom: '4px' }}>
            Robot System Prompt
          </label>
          <textarea
            name="prompt"
            rows={4}
            value={formData.prompt}
            onChange={handleChange}
            placeholder="You are Kiwi, an autonomous companion robot..."
            style={{
              width: '100%', padding: '12px 16px', borderRadius: '14px',
              background: 'var(--card-muted)', border: '1px solid var(--border-light)', color: 'var(--text-dark)', fontSize: '13px', outline: 'none', resize: 'vertical'
            }}
          />
        </div>
      </div>

      {/* ================= CARD 8: PERSONAL LIFE DIARY & LONG-TERM MEMORY ================= */}
      <div className="flux-white-card">
        <div className="flux-card-header">
          <span className="flux-card-title">
            <User size={16} />
            <span>Personal Life Diary &amp; Long-Term Memory</span>
          </span>
        </div>

        <div style={{ marginBottom: '14px' }}>
          <label style={{ fontSize: '11px', fontWeight: 600, color: 'var(--text-muted)', display: 'block', marginBottom: '4px' }}>
            Your Name (Owner)
          </label>
          <input
            type="text"
            name="mem_name"
            value={formData.mem_name}
            onChange={handleChange}
            placeholder="Enter your name (e.g. Rohil)"
            style={{ width: '100%', padding: '11px 16px', borderRadius: '12px', background: 'var(--card-muted)', border: '1px solid var(--border-light)', color: 'var(--text-dark)', fontSize: '13px', outline: 'none' }}
          />
        </div>

        <div style={{ marginBottom: '14px' }}>
          <label style={{ fontSize: '11px', fontWeight: 600, color: 'var(--text-muted)', display: 'block', marginBottom: '4px' }}>
            Friends &amp; Family Members
          </label>
          <input
            type="text"
            name="mem_friends"
            value={formData.mem_friends}
            onChange={handleChange}
            placeholder="e.g. Rohan (best friend), Priya (sister), Papa, Mummy"
            style={{ width: '100%', padding: '11px 16px', borderRadius: '12px', background: 'var(--card-muted)', border: '1px solid var(--border-light)', color: 'var(--text-dark)', fontSize: '13px', outline: 'none' }}
          />
          <small style={{ display: 'block', marginTop: '4px', color: 'var(--text-muted)', fontSize: '12px' }}>
            Kiwi remembers your friends, family and their relationship with you!
          </small>
        </div>

        {/* Facts List */}
        <div style={{ marginBottom: '14px' }}>
          <label style={{ fontSize: '11px', fontWeight: 600, color: 'var(--text-muted)', display: 'block', marginBottom: '6px' }}>
            Facts Kiwi Remembers About You
          </label>
          <div style={{
            background: 'var(--card-subtle)', border: '1px solid var(--border-light)', borderRadius: '12px',
            padding: '10px 14px', maxHeight: '180px', overflowY: 'auto', display: 'flex', flexDirection: 'column', gap: '6px'
          }}>
            {facts.length === 0 ? (
              <div style={{ color: 'var(--text-muted)', fontSize: '12px', padding: '6px 0' }}>
                No memory facts stored yet.
              </div>
            ) : (
              facts.map((fact, idx) => (
                <div key={idx} style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'center', fontSize: '12px', color: 'var(--text-dark)' }}>
                  <span>• {fact}</span>
                  <button
                    type="button"
                    onClick={() => handleDeleteFact(idx)}
                    style={{ background: 'transparent', border: 'none', color: '#F43F5E', cursor: 'pointer', fontWeight: 800, padding: '2px 6px' }}
                  >
                    ✕
                  </button>
                </div>
              ))
            )}
          </div>
        </div>

        {/* Add New Fact */}
        <div>
          <label style={{ fontSize: '11px', fontWeight: 600, color: 'var(--text-muted)', display: 'block', marginBottom: '4px' }}>
            Add New Memory Fact / Event
          </label>
          <div style={{ display: 'flex', gap: '8px' }}>
            <input
              type="text"
              name="new_mem"
              value={formData.new_mem}
              onChange={handleChange}
              placeholder="e.g. Coding test tomorrow / Loves chai"
              style={{ flex: 1, padding: '11px 16px', borderRadius: '999px', background: 'var(--card-muted)', border: '1px solid var(--border-light)', color: 'var(--text-dark)', fontSize: '13px', outline: 'none' }}
            />
            <button type="button" onClick={handleAddFact} className="flux-btn" style={{ padding: '0 18px', borderRadius: '999px' }}>
              <Plus size={14} />
              <span>Add Fact</span>
            </button>
          </div>
          <small style={{ display: 'block', marginTop: '6px', color: 'var(--text-muted)', fontSize: '12px' }}>
            Kiwi will remember this during conversations and refer to it when relevant!
          </small>
        </div>
      </div>

      {/* Bottom Save Button */}
      <div style={{ textAlign: 'center', padding: '10px 0 30px' }}>
        <Tooltip title={saved ? "Settings Saved!" : (saving ? "Saving..." : "Save Configuration & Apply")} placement="top">
          <button
            type="submit"
            disabled={saving}
            className={`flux-save-btn ${saved ? 'is-success' : ''}`}
            style={{ padding: '14px 44px', fontSize: '15px' }}
          >
            {saved ? (
              <>
                <CheckCircle2 size={18} />
                <span>Configuration Saved &amp; Applied!</span>
              </>
            ) : saving ? (
              <>
                <RefreshCw size={18} className="spin-fast" />
                <span>Saving Configuration...</span>
              </>
            ) : (
              <>
                <Save size={18} />
                <span>Save Configuration &amp; Apply</span>
              </>
            )}
          </button>
        </Tooltip>
      </div>
    </form>
  )
}
