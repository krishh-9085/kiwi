import React, { useState, useEffect, useRef } from 'react'
import {
  Wifi, Key, Mic, Brain, Volume2, Hand, CloudSun, User, Save, RefreshCw,
  Plus, Trash2, CheckCircle2, Eye, EyeOff, ChevronDown, X
} from 'lucide-react'
import FluxSelect from './FluxSelect'
import { Tooltip } from './Tooltip'
import WakeSlider from './ui/WakeSlider'

const SETTING_TABS = [
  { id: 'all', lbl: 'All' },
  { id: 'network', lbl: 'Wi-Fi & Keys' },
  { id: 'voice', lbl: 'Voice AI' },
  { id: 'touch', lbl: 'Touch' },
  { id: 'persona', lbl: 'Weather & Prompt' },
  { id: 'diary', lbl: 'Memory' }
]

const API_KEY_FIELDS = [
  { name: 'llmkey', label: 'GOOGLE GEMINI API', placeholder: 'AIzaSy...' },
  { name: 'ttskey', label: 'SARVAM AI API', placeholder: 'sk_...' },
  { name: 'groqkey', label: 'GROQ CLOUD API', placeholder: 'gsk_...' },
  { name: 'fishkey', label: 'FISH AUDIO API', placeholder: 'fa_...' }
]

const VAD_FIELDS = [
  { name: 'stt_trail_ms', label: 'Trailing Silence Cutoff (ms)', min: 100, max: 3000, step: 50, hint: 'Default 450 ms wait after speech finishes.' },
  { name: 'stt_noise_k', label: 'Noise Floor Multiplier (K_noise)', min: 1.1, max: 10.0, step: 0.1, hint: 'Default 2.2x. Speech gate = noiseFloor * K.' },
  { name: 'stt_rel_k', label: 'Relative Peak Multiplier (K_rel)', min: 0.05, max: 1.0, step: 0.05, hint: 'Default 0.20 (20%) silence threshold.' },
  { name: 'stt_max_sil', label: 'Max Silence Hard-Cap (ms)', min: 300, max: 5000, step: 100, hint: 'Default 1200 ms hard cutoff.' }
]

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

const TOUCH_ZONES = [
  {
    zone: 'Top Head',
    fields: [
      { name: 'act_top_tap', label: 'Single Tap' },
      { name: 'act_top_dbl', label: 'Double Tap' },
      { name: 'act_top_hld', label: 'Hold 3s' }
    ]
  },
  {
    zone: 'Left Cheek',
    fields: [
      { name: 'act_lt_tap', label: 'Single Tap' },
      { name: 'act_lt_dbl', label: 'Double Tap' },
      { name: 'act_lt_hld', label: 'Hold 1.5s' }
    ]
  },
  {
    zone: 'Right Cheek',
    fields: [
      { name: 'act_rt_tap', label: 'Single Tap' },
      { name: 'act_rt_dbl', label: 'Double Tap' },
      { name: 'act_rt_hld', label: 'Hold 1.5s' }
    ]
  }
]

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
  { val: '__custom__', lbl: 'Custom Reference ID...' }
]

const fieldLabelStyle = {
  fontSize: '11px',
  fontWeight: 700,
  color: 'var(--text-muted)',
  display: 'block',
  marginBottom: '6px'
}

const subPanelStyle = {
  background: 'var(--card-subtle)',
  border: '1px solid var(--border-light)',
  borderRadius: '14px',
  padding: '15px'
}

const neutralBtnStyle = {
  background: 'var(--card-muted)',
  border: '1px solid var(--border-light)',
  color: 'var(--text-dark)',
  borderRadius: '999px'
}

function PasswordEyeBtn({ visible, onClick, label }) {
  return (
    <button
      type="button"
      onClick={onClick}
      aria-label={label}
      style={{
        position: 'absolute',
        right: '8px',
        top: '50%',
        transform: 'translateY(-50%)',
        background: 'transparent',
        border: 'none',
        color: 'var(--text-muted)',
        cursor: 'pointer',
        padding: '4px',
        display: 'flex',
        alignItems: 'center',
        justifyContent: 'center'
      }}
    >
      {visible ? <EyeOff size={15} /> : <Eye size={15} />}
    </button>
  )
}

export default function SettingsView({
  config,
  onSaveConfig,
  onTriggerAction,
  onToast,
  getBaseUrl
}) {
  const [formData, setFormData] = useState({
    ssid: '',
    pass: '',
    llmkey: '',
    ttskey: '',
    groqkey: '',
    fishkey: '',
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
    llmprovider: 'gemini',
    llmmodel: 'gemini-2.5-flash',
    llmurl: 'https://generativelanguage.googleapis.com/v1beta/models/gemini-2.5-flash:generateContent',
    groqmodel: 'openai/gpt-oss-120b',
    groqurl: 'https://api.groq.com/openai/v1/chat/completions',
    ttsprovider: 'sarvam',
    ttsurl: 'https://api.sarvam.ai/text-to-speech',
    ttsmodel: 'bulbul:v3',
    ttsvoice: 'priya',
    fishurl: 'https://api.fish.audio/v1/tts',
    fishmodel: 's2.1-pro-free',
    fishvoice: 'default',
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
    wthr_city: '',
    wthr_lat: '',
    wthr_lon: '',
    prompt: '',
    mem_name: '',
    mem_friends: '',
    new_mem: ''
  })

  const [activeSection, setActiveSection] = useState('all')
  const [facts, setFacts] = useState([])
  const [newFamName, setNewFamName] = useState('')
  const [newFamRel, setNewFamRel] = useState('')
  const [editingFamIdx, setEditingFamIdx] = useState(null)
  const [pendingFollowUp, setPendingFollowUp] = useState('')
  const [confirmClearMem, setConfirmClearMem] = useState(false)
  const [showAllKeys, setShowAllKeys] = useState(false)
  const [revealedKeys, setRevealedKeys] = useState({})
  const [showPass, setShowPass] = useState(false)
  const [showVad, setShowVad] = useState(false)
  const [scanningWifi, setScanningWifi] = useState(false)
  const [wifiNetworks, setWifiNetworks] = useState([])
  const [saving, setSaving] = useState(false)
  const [saved, setSaved] = useState(false)
  const [customFishVoice, setCustomFishVoice] = useState(false)
  const dirtyFieldsRef = useRef(new Set())
  const dirtyFactsRef = useRef(false)

  useEffect(() => {
    if (!config) return
    const dirty = dirtyFieldsRef.current
    const pick = (name, nextVal) => (dirty.has(name) ? undefined : nextVal)

    setFormData((prev) => {
      const loadedLlmModel = config.llm?.model || config.llmModel || prev.llmmodel
      let loadedLlmUrl = config.llm?.url || config.llmEndpoint || prev.llmurl
      if (loadedLlmUrl && loadedLlmUrl.includes('generativelanguage.googleapis.com') && loadedLlmModel) {
        loadedLlmUrl = `https://generativelanguage.googleapis.com/v1beta/models/${loadedLlmModel}:generateContent`
      }

      const incoming = {
        ssid: config.wifi?.ssid || config.wifiSSID || config.ssid || prev.ssid,
        pass: config.wifi?.password || config.wifiPassword || config.pass || prev.pass,
        llmkey: config.llm?.key || config.llmKey || prev.llmkey,
        ttskey: config.tts?.key || config.ttsKey || prev.ttskey,
        groqkey: config.groqKey || config.stt?.groqKey || prev.groqkey,
        fishkey: config.tts?.fishKey || config.fishKey || prev.fishkey,
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
        llmprovider: config.llm?.provider || config.llmProvider || prev.llmprovider,
        llmmodel: loadedLlmModel,
        llmurl: loadedLlmUrl,
        groqmodel: config.llm?.groqModel || config.groqModel || prev.groqmodel,
        groqurl: config.llm?.groqUrl || config.groqEndpoint || prev.groqurl,
        ttsprovider: config.tts?.provider || config.ttsProvider || prev.ttsprovider,
        ttsurl: config.tts?.url || config.ttsEndpoint || prev.ttsurl,
        ttsmodel: config.tts?.model || config.ttsModel || prev.ttsmodel,
        ttsvoice: config.tts?.voice || config.ttsVoice || prev.ttsvoice,
        fishurl: config.tts?.fishUrl || config.fishEndpoint || prev.fishurl,
        fishmodel: config.tts?.fishModel || config.fishModel || prev.fishmodel,
        fishvoice: config.tts?.fishVoice || config.fishVoice || prev.fishvoice,
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
        wthr_city: config.weather?.city || config.weatherCity || prev.wthr_city,
        wthr_lat: config.weather?.lat || config.weatherLat || prev.wthr_lat,
        wthr_lon: config.weather?.lon || config.weatherLon || prev.wthr_lon,
        prompt: config.prompt != null ? config.prompt : (config.systemPrompt != null ? config.systemPrompt : prev.prompt),
        mem_name: config.memory?.name ?? config.ownerName ?? prev.mem_name,
        mem_friends: config.memory?.friends ?? prev.mem_friends
      }

      const next = { ...prev }
      Object.entries(incoming).forEach(([k, v]) => {
        const chosen = pick(k, v)
        if (chosen !== undefined) next[k] = chosen
      })
      return next
    })

    if (!dirtyFactsRef.current && config.memory?.facts && Array.isArray(config.memory.facts)) {
      setFacts(config.memory.facts)
    }
    setPendingFollowUp(config.memory?.followUp || '')
  }, [config])

  const handleChange = (e) => {
    const { name, value, type, checked } = e.target
    const val = type === 'checkbox' ? (checked ? '1' : '0') : value
    dirtyFieldsRef.current.add(name)
    setFormData((prev) => {
      const updated = { ...prev, [name]: val }
      if (name === 'llmmodel' && (!prev.llmurl || prev.llmurl.includes('generativelanguage.googleapis.com'))) {
        updated.llmurl = `https://generativelanguage.googleapis.com/v1beta/models/${val}:generateContent`
        dirtyFieldsRef.current.add('llmurl')
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

  const familyMembers = String(formData.mem_friends || '')
    .split(',')
    .map((s) => s.trim())
    .filter(Boolean)

  const formatFamilyMember = (raw) => {
    const m = raw.match(/^(.+?)\s*\(([^)]+)\)\s*$/)
    return m ? { name: m[1].trim(), rel: m[2].trim(), raw } : { name: raw, rel: '', raw }
  }

  const resetFamilyDraft = () => {
    setEditingFamIdx(null)
    setNewFamName('')
    setNewFamRel('')
  }

  const handleEditFamily = (idx) => {
    if (editingFamIdx === idx) {
      resetFamilyDraft()
      return
    }
    const parsed = formatFamilyMember(familyMembers[idx] || '')
    setEditingFamIdx(idx)
    setNewFamName(parsed.name)
    setNewFamRel(parsed.rel)
  }

  const handleAddFamily = () => {
    const name = newFamName.trim().replace(/,/g, '')
    const rel = newFamRel.trim().replace(/[(),]/g, '')
    if (!name) return
    const entry = rel ? `${name} (${rel})` : name
    const key = name.toLowerCase()
    const nextList = [...familyMembers]
    const existingIdx = (editingFamIdx !== null && editingFamIdx >= 0 && editingFamIdx < nextList.length)
      ? editingFamIdx
      : nextList.findIndex((item) => formatFamilyMember(item).name.toLowerCase() === key)

    if (existingIdx >= 0) nextList[existingIdx] = entry
    else nextList.push(entry)

    const updated = nextList.join(', ')
    dirtyFieldsRef.current.add('mem_friends')
    setFormData((prev) => ({ ...prev, mem_friends: updated }))
    resetFamilyDraft()
    if (onTriggerAction) onTriggerAction('/do?setfriends=' + encodeURIComponent(updated))
    onToast(existingIdx >= 0 ? 'Updated family/friend!' : 'Added to family & friends!')
  }

  const handleRemoveFamily = (idx) => {
    const updated = familyMembers.filter((_, i) => i !== idx).join(', ')
    dirtyFieldsRef.current.add('mem_friends')
    setFormData((prev) => ({ ...prev, mem_friends: updated }))
    if (editingFamIdx !== null) resetFamilyDraft()
    if (onTriggerAction) onTriggerAction('/do?setfriends=' + encodeURIComponent(updated))
    onToast('Removed member!')
  }

  const handleAddFact = () => {
    const txt = formData.new_mem.trim()
    if (!txt) return
    const strippedNew = txt.replace(/^\[\d{1,2}\s+[A-Za-z]{3}\]\s*/, '').toLowerCase()
    if (facts.some((f) => f.replace(/^\[\d{1,2}\s+[A-Za-z]{3}\]\s*/, '').toLowerCase() === strippedNew)) {
      onToast('Already in memory!')
      return
    }
    dirtyFactsRef.current = true
    dirtyFieldsRef.current.add('new_mem')
    setFacts((prev) => [...prev, txt])
    setFormData((prev) => ({ ...prev, new_mem: '' }))
    if (onTriggerAction) onTriggerAction('/do?addmem=' + encodeURIComponent(txt))
    onToast('Fact saved to Kiwi memory!')
  }

  const handleDeleteFact = (idx) => {
    dirtyFactsRef.current = true
    setFacts((prev) => prev.filter((_, i) => i !== idx))
    if (onTriggerAction) onTriggerAction('/do?delmem=' + idx)
    onToast('Fact removed!')
  }

  const handleClearFollowUp = () => {
    setPendingFollowUp('')
    if (onTriggerAction) onTriggerAction('/do?clearfollowup=1')
    onToast('Follow-up dismissed!')
  }

  const handleClearAllMemory = () => {
    if (!confirmClearMem) {
      setConfirmClearMem(true)
      setTimeout(() => setConfirmClearMem(false), 4000)
      return
    }
    setConfirmClearMem(false)
    resetFamilyDraft()
    dirtyFieldsRef.current.delete('mem_name')
    dirtyFieldsRef.current.delete('mem_friends')
    dirtyFieldsRef.current.delete('new_mem')
    dirtyFactsRef.current = false
    setFormData((prev) => ({ ...prev, mem_name: '', mem_friends: '', new_mem: '' }))
    setFacts([])
    setPendingFollowUp('')
    if (onTriggerAction) onTriggerAction('/do?clearmem=1')
    onToast('All diary memories cleared!')
  }

  const handleSubmit = async (e) => {
    e.preventDefault()
    if (!config) {
      onToast('Device configuration not loaded yet — check connection and try again.')
      return
    }
    setSaving(true)
    const payload = new URLSearchParams()
    Object.entries(formData).forEach(([k, v]) => {
      payload.append(k, String(v))
    })
    payload.append('facts_present', '1')
    facts.forEach((f) => payload.append('facts[]', f))

    const oldSsid = config?.wifi?.ssid || config?.wifiSSID || config?.ssid || ''
    const oldPass = config?.wifi?.password || config?.wifiPassword || config?.pass || ''
    const nextSsid = String(formData.ssid || '').trim()
    const nextPass = String(formData.pass || '').trim()
    const expectsReboot = Boolean(
      (nextSsid && nextSsid !== oldSsid) ||
      (nextPass && nextPass !== oldPass && !/^\*+$/.test(nextPass))
    )

    try {
      await onSaveConfig(payload, expectsReboot)
      dirtyFieldsRef.current.clear()
      dirtyFactsRef.current = false
      if (formData.new_mem) {
        setFormData((prev) => ({ ...prev, new_mem: '' }))
      }
      setSaved(true)
      onToast('Configuration saved & applied successfully!')
      setTimeout(() => setSaved(false), 3000)
    } catch {
      onToast('Failed to save configuration. Check device connection.')
    } finally {
      setSaving(false)
    }
  }

  const showTab = (tabId) => activeSection === 'all' || activeSection === tabId

  return (
    <form onSubmit={handleSubmit} style={{ display: 'flex', flexDirection: 'column', gap: '18px' }}>

      {/* Top Header Bar & Filter Tabs */}
      <div style={{ display: 'flex', flexDirection: 'column', gap: '12px' }}>
        <div style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'center', gap: '12px', flexWrap: 'wrap' }}>
          <h1 className="flux-main-title">System Settings</h1>
          <button
            type="submit"
            disabled={saving}
            className="flux-btn"
            style={{
              ...neutralBtnStyle,
              padding: '7px 14px',
              fontSize: '12px',
              fontWeight: 700,
              borderColor: 'var(--border-dark)'
            }}
          >
            {saved ? <CheckCircle2 size={14} /> : (saving ? <RefreshCw size={14} className="spin-fast" /> : <Save size={14} />)}
            <span>{saved ? 'Saved' : (saving ? 'Saving...' : 'Save')}</span>
          </button>
        </div>

        <div className="flux-chip-row">
          {SETTING_TABS.map((tab) => {
            const active = activeSection === tab.id
            return (
              <button
                key={tab.id}
                type="button"
                onClick={() => setActiveSection(tab.id)}
                style={{
                  padding: '6px 12px',
                  borderRadius: '999px',
                  fontSize: '11.5px',
                  fontWeight: active ? 700 : 600,
                  fontFamily: 'inherit',
                  whiteSpace: 'nowrap',
                  flexShrink: 0,
                  cursor: 'pointer',
                  background: active ? 'var(--card-muted)' : 'var(--card-white)',
                  color: active ? 'var(--text-dark)' : 'var(--text-muted)',
                  border: active ? '1px solid var(--border-dark)' : '1px solid var(--border-light)',
                  transition: 'all 0.14s ease'
                }}
              >
                {tab.lbl}
              </button>
            )
          })}
        </div>
      </div>

      {/* ================= CARD 1: WI-FI & LOCAL NETWORK SETUP ================= */}
      {showTab('network') && (
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
                style={{ ...neutralBtnStyle, padding: '6px 13px', fontSize: '11.5px' }}
              >
                <RefreshCw size={12} className={scanningWifi ? 'spin-fast' : ''} />
                <span>{scanningWifi ? 'Scanning...' : 'Scan'}</span>
              </button>
            </Tooltip>
          </div>

          {wifiNetworks.length > 0 && (
            <div style={{ ...subPanelStyle, marginBottom: '14px', display: 'flex', flexDirection: 'column', gap: '8px' }}>
              <div style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'center' }}>
                <span style={{ fontSize: '10.5px', fontWeight: 700, color: 'var(--text-muted)' }}>
                  DETECTED 2.4GHz NETWORKS
                </span>
                <button
                  type="button"
                  onClick={() => setWifiNetworks([])}
                  style={{ background: 'transparent', border: 'none', color: 'var(--text-muted)', cursor: 'pointer', padding: 0, display: 'flex' }}
                  aria-label="Clear scan results"
                >
                  <X size={13} />
                </button>
              </div>
              <div style={{ display: 'flex', gap: '6px', flexWrap: 'wrap' }}>
                {wifiNetworks.map((net, i) => (
                  <button
                    key={i}
                    type="button"
                    onClick={() => setFormData((prev) => ({ ...prev, ssid: net.ssid }))}
                    className="flux-btn"
                    style={{
                      padding: '4px 10px',
                      fontSize: '11px',
                      borderRadius: '999px',
                      background: net.ssid === formData.ssid ? 'var(--card-muted)' : 'var(--card-white)',
                      color: 'var(--text-dark)',
                      border: net.ssid === formData.ssid ? '1px solid var(--border-dark)' : '1px solid var(--border-light)'
                    }}
                  >
                    <span>{net.ssid}</span>
                    <span style={{ fontSize: '10px', color: 'var(--text-muted)' }}>({net.rssi} dBm)</span>
                  </button>
                ))}
              </div>
            </div>
          )}

          <div className="flux-grid-2">
            <div>
              <label style={fieldLabelStyle}>WI-FI NETWORK (SSID) *</label>
              <input
                type="text"
                name="ssid"
                value={formData.ssid}
                onChange={handleChange}
                autoComplete="off"
                placeholder="Your 2.4GHz Wi-Fi Name"
                className="flux-field-input"
              />
            </div>

            <div>
              <label style={fieldLabelStyle}>WI-FI PASSWORD</label>
              <div style={{ position: 'relative' }}>
                <input
                  type={showPass ? 'text' : 'password'}
                  name="pass"
                  value={formData.pass}
                  onChange={handleChange}
                  autoComplete="new-password"
                  placeholder="Enter password"
                  className="flux-field-input"
                  style={{ paddingRight: '38px' }}
                />
                <PasswordEyeBtn
                  visible={showPass}
                  onClick={() => setShowPass(!showPass)}
                  label={showPass ? 'Hide password' : 'Show password'}
                />
              </div>
            </div>
          </div>
        </div>
      )}

      {/* ================= CARD 2: CLOUD API KEYS & CREDENTIALS ================= */}
      {showTab('network') && (
        <div className="flux-white-card">
          <div className="flux-card-header">
            <span className="flux-card-title">
              <Key size={16} />
              <span>API Keys</span>
            </span>
            <label className="flux-toggle flux-toggle-sm">
              <input
                type="checkbox"
                checked={showAllKeys}
                onChange={(e) => setShowAllKeys(e.target.checked)}
              />
              <span className="flux-toggle-slider" />
              <span style={{ fontSize: '11.5px', fontWeight: 600, color: 'var(--text-muted)' }}>
                Show All
              </span>
            </label>
          </div>

          <div className="flux-grid-autofit">
            {API_KEY_FIELDS.map((field) => {
              const isVisible = showAllKeys || Boolean(revealedKeys[field.name])
              return (
                <div key={field.name}>
                  <label style={fieldLabelStyle}>{field.label}</label>
                  <div style={{ position: 'relative' }}>
                    <input
                      type={isVisible ? 'text' : 'password'}
                      name={field.name}
                      value={formData[field.name]}
                      onChange={handleChange}
                      placeholder={field.placeholder}
                      className="flux-field-input"
                      style={{
                        paddingRight: '38px',
                        fontFamily: isVisible ? 'monospace' : 'inherit',
                        fontSize: isVisible ? '12px' : '13px'
                      }}
                    />
                    <PasswordEyeBtn
                      visible={isVisible}
                      onClick={() => setRevealedKeys((prev) => ({ ...prev, [field.name]: !prev[field.name] }))}
                      label={`${isVisible ? 'Hide' : 'Show'} ${field.label}`}
                    />
                  </div>
                </div>
              )
            })}
          </div>
        </div>
      )}

      {/* ================= CARD 3: SPEECH-TO-TEXT (STT) ================= */}
      {showTab('voice') && (
        <div className="flux-white-card">
          <div className="flux-card-header">
            <span className="flux-card-title">
              <Mic size={16} />
              <span>Speech-to-Text (STT)</span>
            </span>
          </div>

          <div className="flux-grid-2" style={{ marginBottom: '14px' }}>
            <div>
              <label style={fieldLabelStyle}>STT PROVIDER</label>
              <FluxSelect
                name="sttprovider"
                value={formData.sttprovider}
                onChange={handleChange}
                options={STT_PROVIDER_OPTIONS}
              />
            </div>

            <div>
              <label style={fieldLabelStyle}>STT LANGUAGE</label>
              <FluxSelect
                name="sttlang"
                value={formData.sttlang}
                onChange={handleChange}
                options={STT_LANG_OPTIONS}
              />
            </div>
          </div>

          <div style={{ ...subPanelStyle, marginBottom: '12px' }}>
            {formData.sttprovider === 'sarvam' && (
              <div>
                <h3 style={{ fontSize: '12.5px', fontWeight: 700, color: 'var(--text-dark)', marginBottom: '10px' }}>Sarvam AI Settings</h3>
                <div className="flux-grid-2">
                  <div>
                    <label style={{ ...fieldLabelStyle, marginBottom: '4px', fontWeight: 600 }}>Endpoint</label>
                    <input
                      type="url"
                      name="stturl"
                      value={formData.stturl}
                      onChange={handleChange}
                      placeholder="https://api.sarvam.ai/speech-to-text"
                      className="flux-field-input flux-field-input-card"
                    />
                  </div>
                  <div>
                    <label style={{ ...fieldLabelStyle, marginBottom: '4px', fontWeight: 600 }}>Model</label>
                    <FluxSelect name="sttmodel" value={formData.sttmodel} onChange={handleChange} options={STT_SARVAM_MODELS} />
                  </div>
                </div>
              </div>
            )}

            {formData.sttprovider === 'groq' && (
              <div>
                <h3 style={{ fontSize: '12.5px', fontWeight: 700, color: 'var(--text-dark)', marginBottom: '10px' }}>Groq Whisper Settings</h3>
                <div className="flux-grid-2">
                  <div>
                    <label style={{ ...fieldLabelStyle, marginBottom: '4px', fontWeight: 600 }}>Endpoint</label>
                    <input
                      type="url"
                      name="groqstturl"
                      value={formData.groqstturl}
                      onChange={handleChange}
                      placeholder="https://api.groq.com/openai/v1/audio/transcriptions"
                      className="flux-field-input flux-field-input-card"
                    />
                  </div>
                  <div>
                    <label style={{ ...fieldLabelStyle, marginBottom: '4px', fontWeight: 600 }}>Model</label>
                    <FluxSelect name="groqsttmodel" value={formData.groqsttmodel} onChange={handleChange} options={STT_GROQ_MODELS} />
                  </div>
                </div>
              </div>
            )}
          </div>

          <div style={subPanelStyle}>
            <button
              type="button"
              onClick={() => setShowVad(!showVad)}
              style={{
                width: '100%',
                display: 'flex',
                alignItems: 'center',
                justifyContent: 'space-between',
                gap: '8px',
                background: 'transparent',
                border: 'none',
                padding: 0,
                cursor: 'pointer',
                color: 'var(--text-dark)',
                fontFamily: 'inherit'
              }}
            >
              <span style={{ fontSize: '12.5px', fontWeight: 700 }}>Adaptive Voice Detection (VAD)</span>
              <span style={{ display: 'inline-flex', alignItems: 'center', gap: '6px', fontSize: '11px', color: 'var(--text-muted)', fontWeight: 600 }}>
                <span>{formData.stt_trail_ms} ms</span>
                <ChevronDown
                  size={14}
                  style={{ transform: showVad ? 'rotate(180deg)' : 'rotate(0deg)', transition: 'transform 0.16s ease' }}
                />
              </span>
            </button>

            {showVad && (
              <div className="flux-grid-2" style={{ gap: '12px', marginTop: '12px', paddingTop: '12px', borderTop: '1px solid var(--border-light)' }}>
                {VAD_FIELDS.map((v) => (
                  <div key={v.name}>
                    <label style={{ ...fieldLabelStyle, marginBottom: '4px', fontWeight: 600 }}>{v.label}</label>
                    <input
                      type="number"
                      name={v.name}
                      min={v.min}
                      max={v.max}
                      step={v.step}
                      value={formData[v.name]}
                      onChange={handleChange}
                      className="flux-field-input flux-field-input-card"
                    />
                    <span style={{ fontSize: '10px', color: 'var(--text-muted)', display: 'block', marginTop: '3px' }}>{v.hint}</span>
                  </div>
                ))}
              </div>
            )}
          </div>
        </div>
      )}

      {/* ================= CARD 4: AI INTELLIGENCE (LLM) ================= */}
      {showTab('voice') && (
        <div className="flux-white-card">
          <div className="flux-card-header">
            <span className="flux-card-title">
              <Brain size={16} />
              <span>AI Intelligence (LLM)</span>
            </span>
          </div>

          <div style={{ marginBottom: '14px' }}>
            <label style={fieldLabelStyle}>LLM PROVIDER</label>
            <FluxSelect
              name="llmprovider"
              value={formData.llmprovider}
              onChange={handleChange}
              options={LLM_PROVIDER_OPTIONS}
            />
          </div>

          <div style={subPanelStyle}>
            {formData.llmprovider === 'gemini' && (
              <div>
                <h3 style={{ fontSize: '12.5px', fontWeight: 700, color: 'var(--text-dark)', marginBottom: '10px' }}>Gemini Model &amp; Settings</h3>
                <div className="flux-grid-2">
                  <div>
                    <label style={{ ...fieldLabelStyle, marginBottom: '4px', fontWeight: 600 }}>Model</label>
                    <FluxSelect name="llmmodel" value={formData.llmmodel} onChange={handleChange} options={LLM_GEMINI_MODELS} />
                  </div>
                  <div>
                    <label style={{ ...fieldLabelStyle, marginBottom: '4px', fontWeight: 600 }}>Endpoint</label>
                    <input
                      type="url"
                      name="llmurl"
                      value={formData.llmurl}
                      onChange={handleChange}
                      className="flux-field-input flux-field-input-card"
                    />
                  </div>
                </div>
              </div>
            )}

            {formData.llmprovider === 'groq' && (
              <div>
                <h3 style={{ fontSize: '12.5px', fontWeight: 700, color: 'var(--text-dark)', marginBottom: '10px' }}>Groq Model &amp; Settings</h3>
                <div className="flux-grid-2">
                  <div>
                    <label style={{ ...fieldLabelStyle, marginBottom: '4px', fontWeight: 600 }}>Model</label>
                    <FluxSelect name="groqmodel" value={formData.groqmodel} onChange={handleChange} options={LLM_GROQ_MODELS} />
                  </div>
                  <div>
                    <label style={{ ...fieldLabelStyle, marginBottom: '4px', fontWeight: 600 }}>Endpoint</label>
                    <input
                      type="url"
                      name="groqurl"
                      value={formData.groqurl}
                      onChange={handleChange}
                      className="flux-field-input flux-field-input-card"
                    />
                  </div>
                </div>
              </div>
            )}
          </div>
        </div>
      )}

      {/* ================= CARD 5: TEXT-TO-SPEECH (TTS) ================= */}
      {showTab('voice') && (
        <div className="flux-white-card">
          <div className="flux-card-header">
            <span className="flux-card-title">
              <Volume2 size={16} />
              <span>Text-to-Speech (TTS)</span>
            </span>
          </div>

          <div style={{ marginBottom: '14px' }}>
            <label style={fieldLabelStyle}>TTS PROVIDER</label>
            <FluxSelect
              name="ttsprovider"
              value={formData.ttsprovider}
              onChange={handleChange}
              options={TTS_PROVIDER_OPTIONS}
            />
          </div>

          <div style={subPanelStyle}>
            {formData.ttsprovider === 'sarvam' && (
              <div>
                <h3 style={{ fontSize: '12.5px', fontWeight: 700, color: 'var(--text-dark)', marginBottom: '10px' }}>Sarvam TTS Settings</h3>
                <div className="flux-grid-autofit">
                  <div>
                    <label style={{ ...fieldLabelStyle, marginBottom: '4px', fontWeight: 600 }}>Endpoint</label>
                    <input
                      type="url"
                      name="ttsurl"
                      value={formData.ttsurl}
                      onChange={handleChange}
                      className="flux-field-input flux-field-input-card"
                    />
                  </div>
                  <div>
                    <label style={{ ...fieldLabelStyle, marginBottom: '4px', fontWeight: 600 }}>Model</label>
                    <FluxSelect name="ttsmodel" value={formData.ttsmodel} onChange={handleChange} options={TTS_SARVAM_MODELS} />
                  </div>
                  <div>
                    <label style={{ ...fieldLabelStyle, marginBottom: '4px', fontWeight: 600 }}>Voice / Speaker</label>
                    <FluxSelect name="ttsvoice" value={formData.ttsvoice} onChange={handleChange} options={TTS_SARVAM_VOICES} />
                  </div>
                </div>
              </div>
            )}

            {formData.ttsprovider === 'fish' && (
              <div>
                <h3 style={{ fontSize: '12.5px', fontWeight: 700, color: 'var(--text-dark)', marginBottom: '10px' }}>Fish Audio Settings</h3>
                <div className="flux-grid-autofit">
                  <div>
                    <label style={{ ...fieldLabelStyle, marginBottom: '4px', fontWeight: 600 }}>Endpoint</label>
                    <input
                      type="url"
                      name="fishurl"
                      value={formData.fishurl}
                      onChange={handleChange}
                      className="flux-field-input flux-field-input-card"
                    />
                  </div>
                  <div>
                    <label style={{ ...fieldLabelStyle, marginBottom: '4px', fontWeight: 600 }}>Model</label>
                    <FluxSelect name="fishmodel" value={formData.fishmodel} onChange={handleChange} options={TTS_FISH_MODELS} />
                  </div>
                  <div>
                    <label style={{ ...fieldLabelStyle, marginBottom: '4px', fontWeight: 600 }}>Voice / Reference ID</label>
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
                        <span style={{ fontWeight: 700 }}>ID:</span>
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

                    {(customFishVoice || (!TTS_FISH_VOICES.some((v) => v.val === formData.fishvoice) && formData.fishvoice && formData.fishvoice !== 'default')) && (
                      <div style={{ marginTop: '8px' }}>
                        <input
                          type="text"
                          name="fishvoice"
                          value={formData.fishvoice === 'default' ? '' : formData.fishvoice}
                          onChange={handleChange}
                          placeholder="Paste 32-character reference ID hex..."
                          className="flux-field-input flux-field-input-card"
                          style={{ fontFamily: 'monospace', fontSize: '11.5px' }}
                        />
                      </div>
                    )}
                  </div>
                </div>
              </div>
            )}
          </div>
        </div>
      )}

      {/* ================= CARD 6: CAPACITIVE TOUCH MAPPING & GESTURES ================= */}
      {showTab('touch') && (
        <div className="flux-white-card">
          <div className="flux-card-header">
            <span className="flux-card-title">
              <Hand size={16} />
              <span>Touch &amp; Gestures</span>
            </span>
            <label className="flux-toggle flux-toggle-sm">
              <input
                type="checkbox"
                name="tch_snd"
                checked={formData.tch_snd === '1'}
                onChange={handleChange}
              />
              <span className="flux-toggle-slider" />
              <span style={{ fontSize: '11.5px', fontWeight: 700, color: 'var(--text-dark)', whiteSpace: 'nowrap' }}>
                Touch Sound
              </span>
            </label>
          </div>

          <div style={{ marginBottom: '16px' }}>
            <div style={{ display: 'flex', justifyContent: 'space-between', fontSize: '12px', marginBottom: '8px' }}>
              <span style={{ fontWeight: 600, color: 'var(--text-muted)' }}>
                Debounce Sensitivity: <strong style={{ color: 'var(--text-dark)' }}>{formData.tch_deb} ms</strong>
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

          <div className="flux-grid-autofit">
            {TOUCH_ZONES.map((z) => (
              <div key={z.zone} style={{ ...subPanelStyle, display: 'flex', flexDirection: 'column', gap: '10px' }}>
                <span style={{ fontSize: '12px', fontWeight: 700, color: 'var(--text-dark)' }}>{z.zone}</span>
                {z.fields.map((f) => (
                  <div key={f.name}>
                    <label style={{ ...fieldLabelStyle, marginBottom: '4px', fontSize: '10.5px', fontWeight: 600 }}>{f.label}</label>
                    <FluxSelect compact name={f.name} value={formData[f.name]} onChange={handleChange} options={TOUCH_ACTION_OPTIONS} />
                  </div>
                ))}
              </div>
            ))}
          </div>

          <small style={{ display: 'block', marginTop: '12px', color: 'var(--text-muted)', fontSize: '11.5px' }}>
            Tip: Hold Top Head for 3s to change face skin; hold for 7s to enter Night Sleep.
          </small>
        </div>
      )}

      {/* ================= CARD 7: WEATHER & PERSONALITY ================= */}
      {showTab('persona') && (
        <div className="flux-white-card">
          <div className="flux-card-header">
            <span className="flux-card-title">
              <CloudSun size={16} />
              <span>Weather &amp; Personality</span>
            </span>
          </div>

          <div style={{ marginBottom: '12px' }}>
            <label style={{ ...fieldLabelStyle, marginBottom: '4px', fontWeight: 600 }}>City Name</label>
            <input
              type="text"
              name="wthr_city"
              value={formData.wthr_city}
              onChange={handleChange}
              placeholder="e.g. Mumbai, New Delhi"
              className="flux-field-input"
            />
          </div>

          <div className="flux-grid-2" style={{ marginBottom: '14px' }}>
            <div>
              <label style={{ ...fieldLabelStyle, marginBottom: '4px', fontWeight: 600 }}>Latitude (-90 to 90)</label>
              <input
                type="number"
                step="any"
                min="-90"
                max="90"
                name="wthr_lat"
                value={formData.wthr_lat}
                onChange={handleChange}
                placeholder="e.g. 19.0760"
                className="flux-field-input"
              />
            </div>

            <div>
              <label style={{ ...fieldLabelStyle, marginBottom: '4px', fontWeight: 600 }}>Longitude (-180 to 180)</label>
              <input
                type="number"
                step="any"
                min="-180"
                max="180"
                name="wthr_lon"
                value={formData.wthr_lon}
                onChange={handleChange}
                placeholder="e.g. 72.8777"
                className="flux-field-input"
              />
            </div>
          </div>

          <div>
            <label style={{ ...fieldLabelStyle, marginBottom: '4px', fontWeight: 600 }}>Robot System Prompt</label>
            <textarea
              name="prompt"
              rows={4}
              value={formData.prompt}
              onChange={handleChange}
              placeholder="You are Kiwi, an autonomous companion robot..."
              className="flux-field-input"
              style={{ borderRadius: '14px', resize: 'vertical' }}
            />
          </div>
        </div>
      )}

      {/* ================= CARD 8: PERSONAL LIFE DIARY & LONG-TERM MEMORY ================= */}
      {showTab('diary') && (
        <div className="flux-white-card">
          <div className="flux-card-header" style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'center', gap: '8px', flexWrap: 'wrap' }}>
            <span className="flux-card-title">
              <User size={16} />
              <span>Personal Life Diary &amp; Memory</span>
            </span>
            <button
              type="button"
              onClick={handleClearAllMemory}
              style={{
                display: 'inline-flex',
                alignItems: 'center',
                gap: '5px',
                padding: '5px 11px',
                borderRadius: '999px',
                fontSize: '11px',
                fontWeight: 700,
                cursor: 'pointer',
                background: confirmClearMem ? 'rgba(244, 63, 94, 0.14)' : 'var(--card-muted)',
                border: confirmClearMem ? '1px solid #F43F5E' : '1px solid var(--border-light)',
                color: confirmClearMem ? '#F43F5E' : 'var(--text-muted)'
              }}
            >
              <Trash2 size={12} />
              <span>{confirmClearMem ? 'Confirm Clear?' : 'Clear Memory'}</span>
            </button>
          </div>

          {pendingFollowUp && (
            <div style={{
              marginBottom: '14px',
              padding: '10px 14px',
              borderRadius: '12px',
              background: 'var(--card-subtle)',
              border: '1px solid var(--border-light)',
              display: 'flex',
              justifyContent: 'space-between',
              alignItems: 'center',
              gap: '10px'
            }}>
              <div style={{ fontSize: '12px', color: 'var(--text-dark)' }}>
                <span style={{ fontWeight: 700, color: 'var(--text-muted)', marginRight: '6px' }}>Next Check-In:</span>
                <span>{pendingFollowUp}</span>
              </div>
              <button
                type="button"
                onClick={handleClearFollowUp}
                title="Dismiss follow-up"
                style={{ background: 'transparent', border: 'none', color: '#F43F5E', cursor: 'pointer', padding: '2px 4px', flexShrink: 0, display: 'flex' }}
              >
                <X size={14} />
              </button>
            </div>
          )}

          <div style={{ marginBottom: '14px' }}>
            <label style={{ ...fieldLabelStyle, marginBottom: '4px', fontWeight: 600 }}>Your Name (Owner)</label>
            <input
              type="text"
              name="mem_name"
              value={formData.mem_name}
              onChange={handleChange}
              placeholder="Enter your name (e.g. Rohil)"
              className="flux-field-input"
            />
          </div>

          <div style={{ marginBottom: '14px' }}>
            <label style={{ ...fieldLabelStyle, fontWeight: 600 }}>
              Friends &amp; Family Members ({familyMembers.length})
            </label>

            {familyMembers.length > 0 ? (
              <div style={{ display: 'flex', flexWrap: 'wrap', gap: '6px', marginBottom: '10px' }}>
                {familyMembers.map((member, idx) => {
                  const { name, rel } = formatFamilyMember(member)
                  const relParts = rel ? rel.split(/\s*(?:\|| - )\s*/).filter(Boolean) : []
                  const baseRel = relParts[0] || ''
                  const linkNote = relParts.slice(1).join(' · ')
                  const isEditing = editingFamIdx === idx
                  return (
                    <span
                      key={idx}
                      style={{
                        display: 'inline-flex',
                        alignItems: 'center',
                        gap: '4px',
                        padding: '4px 9px',
                        borderRadius: '999px',
                        fontSize: '12px',
                        background: isEditing ? 'var(--card-muted)' : 'var(--card-subtle)',
                        border: isEditing ? '1px solid var(--border-dark)' : '1px solid var(--border-light)',
                        color: 'var(--text-dark)'
                      }}
                    >
                      <button
                        type="button"
                        onClick={() => handleEditFamily(idx)}
                        title="Tap to edit"
                        style={{
                          display: 'inline-flex',
                          alignItems: 'center',
                          gap: '5px',
                          flexWrap: 'wrap',
                          background: 'transparent',
                          border: 'none',
                          padding: 0,
                          cursor: 'pointer',
                          color: 'inherit',
                          fontSize: 'inherit',
                          textAlign: 'left'
                        }}
                      >
                        <span style={{ fontWeight: 600 }}>{name}</span>
                        {baseRel && <span style={{ color: 'var(--text-muted)', fontSize: '11px' }}>({baseRel})</span>}
                        {linkNote && <span style={{ color: 'var(--text-muted)', fontSize: '10px', fontWeight: 600 }}>· {linkNote}</span>}
                      </button>
                      <button
                        type="button"
                        onClick={() => handleRemoveFamily(idx)}
                        title="Remove member"
                        style={{ background: 'transparent', border: 'none', color: '#F43F5E', cursor: 'pointer', padding: '0 2px', display: 'flex' }}
                      >
                        <X size={12} />
                      </button>
                    </span>
                  )
                })}
              </div>
            ) : (
              <div style={{ color: 'var(--text-muted)', fontSize: '12px', marginBottom: '8px' }}>
                No family/friends added yet — add below or tell Kiwi in voice!
              </div>
            )}

            <div style={{ display: 'flex', gap: '8px', flexWrap: 'wrap', marginBottom: '8px' }}>
              <input
                type="text"
                value={newFamName}
                onChange={(e) => setNewFamName(e.target.value)}
                onKeyDown={(e) => {
                  if (e.key === 'Enter') { e.preventDefault(); handleAddFamily() }
                  if (e.key === 'Escape' && editingFamIdx !== null) { e.preventDefault(); resetFamilyDraft() }
                }}
                placeholder="Name (e.g. Yunay)"
                className="flux-field-input"
                style={{ flex: '1 1 120px', borderRadius: '999px', padding: '9px 14px', fontSize: '12px' }}
              />
              <input
                type="text"
                value={newFamRel}
                onChange={(e) => setNewFamRel(e.target.value)}
                onKeyDown={(e) => {
                  if (e.key === 'Enter') { e.preventDefault(); handleAddFamily() }
                  if (e.key === 'Escape' && editingFamIdx !== null) { e.preventDefault(); resetFamilyDraft() }
                }}
                placeholder="Relation (e.g. Bhatija - Yash's son)"
                className="flux-field-input"
                style={{ flex: '1 1 170px', borderRadius: '999px', padding: '9px 14px', fontSize: '12px' }}
              />
              <button
                type="button"
                onClick={handleAddFamily}
                className="flux-btn"
                style={{ ...neutralBtnStyle, padding: '0 16px', minHeight: '36px' }}
              >
                <Plus size={14} />
                <span>{editingFamIdx !== null ? 'Update' : 'Add'}</span>
              </button>
              {editingFamIdx !== null && (
                <button
                  type="button"
                  onClick={resetFamilyDraft}
                  style={{
                    padding: '0 12px',
                    borderRadius: '999px',
                    minHeight: '36px',
                    fontSize: '12px',
                    fontWeight: 600,
                    cursor: 'pointer',
                    background: 'var(--card-muted)',
                    border: '1px solid var(--border-light)',
                    color: 'var(--text-muted)'
                  }}
                >
                  Cancel
                </button>
              )}
            </div>

            <input
              type="text"
              name="mem_friends"
              value={formData.mem_friends}
              onChange={(e) => {
                if (editingFamIdx !== null) setEditingFamIdx(null)
                handleChange(e)
              }}
              placeholder="Comma-separated: Rohan (best friend), Priya (sister), Papa"
              className="flux-field-input"
              style={{ fontSize: '12px', color: 'var(--text-muted)' }}
            />
          </div>

          <div style={{ marginBottom: '14px' }}>
            <label style={{ ...fieldLabelStyle, fontWeight: 600 }}>
              Facts Kiwi Remembers About You ({facts.length}/16)
            </label>
            <div style={{
              ...subPanelStyle,
              padding: '10px 14px',
              maxHeight: '190px',
              overflowY: 'auto',
              display: 'flex',
              flexDirection: 'column',
              gap: '6px'
            }}>
              {facts.length === 0 ? (
                <div style={{ color: 'var(--text-muted)', fontSize: '12px', padding: '6px 0' }}>
                  No memory facts stored yet.
                </div>
              ) : (
                facts.map((fact, idx) => {
                  const dm = String(fact).match(/^(\[\d{1,2}\s+[A-Za-z]{3}\])\s*(.+)$/)
                  return (
                    <div key={idx} style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'center', gap: '8px', fontSize: '12px', color: 'var(--text-dark)' }}>
                      <span>
                        {dm ? (
                          <>
                            <span style={{ fontSize: '10px', fontWeight: 700, color: 'var(--text-muted)', marginRight: '6px' }}>{dm[1]}</span>
                            <span>{dm[2]}</span>
                          </>
                        ) : (
                          <>• {fact}</>
                        )}
                      </span>
                      <button
                        type="button"
                        onClick={() => handleDeleteFact(idx)}
                        style={{ background: 'transparent', border: 'none', color: '#F43F5E', cursor: 'pointer', padding: '2px 4px', flexShrink: 0, display: 'flex' }}
                      >
                        <X size={13} />
                      </button>
                    </div>
                  )
                })
              )}
            </div>
          </div>

          <div>
            <label style={{ ...fieldLabelStyle, marginBottom: '4px', fontWeight: 600 }}>Add New Memory Fact / Event</label>
            <div style={{ display: 'flex', gap: '8px' }}>
              <input
                type="text"
                name="new_mem"
                value={formData.new_mem}
                onChange={handleChange}
                onKeyDown={(e) => { if (e.key === 'Enter') { e.preventDefault(); handleAddFact() } }}
                placeholder="e.g. Coding test tomorrow / Loves chai"
                className="flux-field-input"
                style={{ flex: 1, borderRadius: '999px' }}
              />
              <button
                type="button"
                onClick={handleAddFact}
                className="flux-btn"
                style={{ ...neutralBtnStyle, padding: '0 16px' }}
              >
                <Plus size={14} />
                <span>Add Fact</span>
              </button>
            </div>
            <small style={{ display: 'block', marginTop: '6px', color: 'var(--text-muted)', fontSize: '12px' }}>
              Kiwi auto-stamps dates and uses follow-up check-ins during natural conversations!
            </small>
          </div>
        </div>
      )}

      {/* Bottom Save Button */}
      <div style={{ textAlign: 'center', padding: '6px 0 30px' }}>
        <Tooltip title={saved ? 'Settings Saved!' : (saving ? 'Saving...' : 'Save Configuration & Apply')} placement="top">
          <button
            type="submit"
            disabled={saving}
            className={`flux-save-btn ${saved ? 'is-success' : ''}`}
            style={{ padding: '14px 42px', fontSize: '15px' }}
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
