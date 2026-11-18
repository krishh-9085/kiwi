import React from 'react'
import { motion } from 'motion/react'
import { Check, AlertCircle, Sparkles, X } from 'lucide-react'

/**
 * Ultra-Clean Sonner / Dynamic Island Style Toast HUD
 * Single-line minimalist frosted obsidian pill with crisp iconography
 */
export default function Toast({ toast, onClose }) {
  if (!toast) return null

  const message = typeof toast === 'object' ? toast.text || toast.message : String(toast)
  const isError = typeof toast === 'object'
    ? toast.type === 'error'
    : (message.toLowerCase().includes('fail') || message.toLowerCase().includes('error') || message.toLowerCase().includes('offline'))
  const isInfo = message.toLowerCase().includes('listen') || message.toLowerCase().includes('connecting') || message.toLowerCase().includes('target')

  return (
    <div className="flux-toast-wrapper" role="status" aria-live="polite">
      <motion.div
        initial={{ opacity: 0, y: -20, scale: 0.92, filter: 'blur(4px)' }}
        animate={{ opacity: 1, y: 0, scale: 1, filter: 'blur(0px)' }}
        exit={{ opacity: 0, y: -14, scale: 0.95, filter: 'blur(3px)' }}
        transition={{ type: 'spring', damping: 26, stiffness: 400 }}
        className={`flux-toast-card ${isError ? 'is-error' : isInfo ? 'is-info' : 'is-success'}`}
      >
        <div className="flux-toast-icon">
          {isError ? (
            <AlertCircle size={13} strokeWidth={2.5} />
          ) : isInfo ? (
            <Sparkles size={13} strokeWidth={2.5} />
          ) : (
            <Check size={13} strokeWidth={2.8} />
          )}
        </div>
        <span className="flux-toast-message">
          {message}
        </span>
        {onClose && (
          <button
            type="button"
            onClick={onClose}
            className="flux-toast-close"
            aria-label="Close notification"
          >
            <X size={12} />
          </button>
        )}
      </motion.div>
    </div>
  )
}
