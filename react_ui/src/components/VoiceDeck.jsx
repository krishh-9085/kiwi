import React, { useState } from 'react'
import { Mic, MicOff, Send, Sparkles, MessageSquare, Bot } from 'lucide-react'

export default function VoiceDeck({ status, onTriggerAction, busy, recording }) {
  const [promptText, setPromptText] = useState('')
  const [sending, setSending] = useState(false)

  const isListening = status?.state?.toLowerCase().includes('listen') || status?.emotion === 'listening' || recording
  const isSpeaking  = status?.state?.toLowerCase().includes('speak') || status?.emotion === 'speaking'
  const isThinking  = status?.busy

  const handleMicToggle = async () => {
    if (isListening || isThinking) {
      await onTriggerAction('/do?record=0')
    } else {
      await onTriggerAction('/do?record=1')
    }
  }

  const handleSendPrompt = async (textToSend) => {
    const q = (textToSend || promptText).trim()
    if (!q) return
    setSending(true)
    await onTriggerAction(`/do?ask=${encodeURIComponent(q)}`)
    setPromptText('')
    setSending(false)
  }

  const samplePrompts = [
    'Tell me a joke',
    'What is your name?',
    'Sing a cute song',
    'Tell me a funny secret',
    'How are you feeling today?'
  ]

  const micClass = isListening ? 'listening' : (isThinking ? 'thinking' : '')

  return (
    <div className="glass-panel" style={{ padding: '24px', display: 'flex', flexDirection: 'column', gap: '20px' }}>
      <div style={{ display: 'flex', alignItems: 'center', justifyContent: 'space-between' }}>
        <div style={{ display: 'flex', alignItems: 'center', gap: '10px' }}>
          <Sparkles size={17} color="var(--accent-cyan)" />
          <h2 style={{ fontSize: '15px', fontWeight: 600 }}>AI Voice &amp; Interaction Hub</h2>
        </div>
        <span style={{ fontSize: '11px', color: 'var(--text-muted)', fontFamily: 'var(--font-mono)' }}>Real-Time</span>
      </div>

      {/* Hero Mic Stage */}
      <div className="mic-stage">
        {/* Halo rings when listening */}
        {isListening && (
          <>
            <div className="mic-halo mic-halo-1" />
            <div className="mic-halo mic-halo-2" />
            <div className="mic-halo mic-halo-3" />
          </>
        )}

        {/* Glow when thinking */}
        {!isListening && (
          <div style={{
            position: 'absolute',
            width: '130px', height: '130px', borderRadius: '50%',
            background: isThinking
              ? 'radial-gradient(circle, rgba(251,191,36,0.35) 0%, transparent 70%)'
              : 'radial-gradient(circle, rgba(0,240,255,0.2) 0%, transparent 70%)',
            pointerEvents: 'none',
            transition: 'all 0.3s'
          }} className={isThinking ? 'pulse-active' : ''} />
        )}

        <button
          className={`mic-btn ${micClass}`}
          onClick={handleMicToggle}
          title={isListening ? 'Click to Stop / Interrupt' : 'Click to Speak to Kiwi'}
        >
          {isListening ? <MicOff size={34} color="#ffffff" /> : <Mic size={34} color="#ffffff" />}
        </button>

        <div style={{ marginTop: '14px', textAlign: 'center', zIndex: 2 }}>
          <p style={{
            fontSize: '14px', fontWeight: 600,
            color: isListening ? 'var(--accent-coral)' : 'var(--text-main)'
          }}>
            {isListening
              ? 'Listening to your voice...'
              : isThinking
                ? 'Kiwi is thinking...'
                : isSpeaking
                  ? 'Kiwi is speaking...'
                  : 'Tap Mic to Speak'}
          </p>
          <p style={{ fontSize: '11px', color: 'var(--text-muted)', marginTop: '2px' }}>
            {isListening ? 'Click again to cancel / interrupt' : 'Say "Hey Kiwi" or click above'}
          </p>
        </div>
      </div>

      {/* AI Text Prompt */}
      <div>
        <form
          onSubmit={(e) => { e.preventDefault(); handleSendPrompt() }}
          style={{ display: 'flex', gap: '8px' }}
        >
          <input
            type="text"
            value={promptText}
            onChange={(e) => setPromptText(e.target.value)}
            placeholder="Type a message or prompt for Kiwi..."
            disabled={sending}
            style={{ flex: 1, padding: '12px 16px', borderRadius: '12px', fontSize: '13px' }}
          />
          <button
            type="submit"
            disabled={sending || !promptText.trim()}
            className="btn-action btn-primary"
            style={{ borderRadius: '12px', padding: '0 20px' }}
          >
            <Send size={15} />
            <span>Send</span>
          </button>
        </form>

        {/* Quick suggestions */}
        <div style={{ display: 'flex', gap: '6px', flexWrap: 'wrap', marginTop: '10px' }}>
          {samplePrompts.map((p, idx) => (
            <button
              key={idx}
              onClick={() => handleSendPrompt(p)}
              className="btn-action"
              style={{ fontSize: '11px', padding: '4px 10px', borderRadius: '999px', background: 'rgba(255,255,255,0.03)', color: 'var(--text-muted)' }}
            >
              "{p}"
            </button>
          ))}
        </div>
      </div>

      {/* Last Q&A bubble */}
      {(status?.question || status?.answer) && (
        <div style={{
          background: 'rgba(0,0,0,0.3)',
          border: '1px solid var(--border-subtle)',
          borderRadius: '12px',
          padding: '14px',
          display: 'flex',
          flexDirection: 'column',
          gap: '10px'
        }}>
          {status.question && (
            <div style={{ display: 'flex', alignItems: 'flex-start', gap: '8px' }}>
              <div style={{ padding: '4px', background: 'rgba(255,255,255,0.08)', borderRadius: '6px' }}>
                <MessageSquare size={12} color="var(--accent-cyan)" />
              </div>
              <div>
                <span style={{ fontSize: '11px', color: 'var(--text-dim)', fontWeight: 600, fontFamily: 'var(--font-mono)' }}>YOU:</span>
                <p style={{ fontSize: '12px', color: 'var(--text-main)' }}>{status.question}</p>
              </div>
            </div>
          )}

          {status.answer && (
            <div style={{ display: 'flex', alignItems: 'flex-start', gap: '8px' }}>
              <div style={{ padding: '4px', background: 'rgba(0,240,255,0.1)', borderRadius: '6px' }}>
                <Bot size={12} color="var(--accent-cyan)" />
              </div>
              <div>
                <span style={{ fontSize: '11px', color: 'var(--accent-cyan)', fontWeight: 600, fontFamily: 'var(--font-mono)' }}>KIWI:</span>
                <p style={{ fontSize: '12px', color: '#e2e8f0', lineHeight: 1.4 }}>{status.answer}</p>
              </div>
            </div>
          )}
        </div>
      )}
    </div>
  )
}
