import React, { useState, useEffect, useRef, useCallback } from 'react'
import {
  DownloadCloud, UploadCloud, RefreshCw, FileCode, CheckCircle2, X
} from 'lucide-react'
import { Tooltip } from './Tooltip'

const APP_PARTITION_LIMIT_BYTES = 2031616 // ~1.94 MB (esp32s3 default OTA app partition)

function compareVersions(v1, v2) {
  const parse = (v) => String(v || '').replace(/^[vV]/, '').split('.').map((n) => parseInt(n, 10) || 0)
  const p1 = parse(v1)
  const p2 = parse(v2)
  for (let i = 0; i < Math.max(p1.length, p2.length); i++) {
    const n1 = p1[i] || 0
    const n2 = p2[i] || 0
    if (n1 > n2) return 1
    if (n1 < n2) return -1
  }
  return 0
}

function formatReleaseNotes(raw) {
  if (!raw) return []
  return String(raw)
    .split(/\r?\n/)
    .map((line) => line.trim())
    .filter(Boolean)
    .map((line) => ({
      isHeading: /^#{1,3}\s+/.test(line),
      isBullet: /^[-*•]\s+/.test(line),
      text: line
        .replace(/^#{1,6}\s+/, '')
        .replace(/^[-*•]\s+/, '')
        .replace(/\*\*([^*]+)\*\*/g, '$1')
        .replace(/`([^`]+)`/g, '$1')
    }))
    .slice(0, 8)
}

export default function OtaUpdateView({ status, getBaseUrl, onToast }) {
  const [file, setFile] = useState(null)
  const [dragOver, setDragOver] = useState(false)
  const [uploading, setUploading] = useState(false)
  const [progress, setProgress] = useState(0)
  const [uploadStatusText, setUploadStatusText] = useState('')
  const [cloudRelease, setCloudRelease] = useState(null)
  const [checkingCloud, setCheckingCloud] = useState(false)
  const [cloudError, setCloudError] = useState(null)
  const [confirmInstall, setConfirmInstall] = useState(false)
  const [installingGithub, setInstallingGithub] = useState(false)
  const [otaProgress, setOtaProgress] = useState(null)
  const [otaProgressText, setOtaProgressText] = useState('')
  const fileInputRef = useRef(null)
  const pollTimerRef = useRef(null)
  const reloadTimerRef = useRef(null)

  const currentVer = (status?.version || '2.0').replace(/^[vV]/, '')
  const displayVersion = `v${currentVer}`

  const checkGithubReleases = useCallback(async () => {
    setCheckingCloud(true)
    setCloudError(null)
    try {
      const res = await fetch('https://api.github.com/repos/krishh-9085/kiwi/releases/latest', {
        headers: { Accept: 'application/vnd.github.v3+json' }
      })
      if (!res.ok) {
        throw new Error(`GitHub API returned status ${res.status}`)
      }
      const data = await res.json()
      const rawTag = data.tag_name || data.name || ''
      const cleanVer = rawTag.replace(/^[vV]/, '')
      const binAsset = data.assets?.find((a) => a.name?.toLowerCase().endsWith('.bin'))
      const isNewer = compareVersions(cleanVer, currentVer) > 0

      setCloudRelease({
        tag: rawTag,
        version: cleanVer,
        name: data.name || rawTag,
        downloadUrl: binAsset?.browser_download_url || '',
        binName: binAsset?.name || '',
        binSize: binAsset?.size || 0,
        noteLines: formatReleaseNotes(data.body),
        updateAvailable: isNewer,
        hasBin: Boolean(binAsset)
      })
    } catch (err) {
      console.warn('Failed to check GitHub releases:', err)
      setCloudError('Unable to check GitHub repository directly.')
    } finally {
      setCheckingCloud(false)
    }
  }, [currentVer])

  useEffect(() => {
    checkGithubReleases()
    return () => {
      if (pollTimerRef.current) clearInterval(pollTimerRef.current)
      if (reloadTimerRef.current) clearTimeout(reloadTimerRef.current)
    }
  }, [checkGithubReleases])

  const selectFirmwareFile = (selected) => {
    if (!selected) return
    if (!selected.name.toLowerCase().endsWith('.bin')) {
      onToast('Please select a compiled firmware .bin file!')
      return
    }
    setFile(selected)
    onToast(`Selected: ${selected.name} (${(selected.size / 1024 / 1024).toFixed(2)} MB)`)
  }

  const handleFileChange = (e) => {
    if (e.target.files && e.target.files[0]) {
      selectFirmwareFile(e.target.files[0])
    }
  }

  const handleDrop = (e) => {
    e.preventDefault()
    setDragOver(false)
    if (e.dataTransfer?.files?.[0]) {
      selectFirmwareFile(e.dataTransfer.files[0])
    }
  }

  const handleClearFile = (e) => {
    e.stopPropagation()
    setFile(null)
    if (fileInputRef.current) fileInputRef.current.value = ''
  }

  const handleUploadFirmware = (e) => {
    e.preventDefault()
    if (!file) return

    if (file.size > APP_PARTITION_LIMIT_BYTES) {
      onToast('Error: Firmware exceeds 1.94 MB app partition limit!')
      return
    }

    setUploading(true)
    setProgress(0)
    setUploadStatusText('Uploading firmware binary to ESP32-S3...')

    const xhr = new XMLHttpRequest()
    const url = `${getBaseUrl()}/update`

    xhr.upload.addEventListener('progress', (event) => {
      if (event.lengthComputable) {
        const pct = Math.round((event.loaded / event.total) * 100)
        setProgress(pct)
        setUploadStatusText(`Flashing OTA partition... ${pct}% (${(event.loaded / 1024).toFixed(0)} KB / ${(event.total / 1024).toFixed(0)} KB)`)
      }
    })

    xhr.addEventListener('load', () => {
      if (xhr.status === 200) {
        setProgress(100)
        setUploadStatusText('Firmware flash successful! Kiwi is restarting...')
        onToast('Firmware update complete! Rebooting...')
        setTimeout(() => {
          window.location.reload()
        }, 5000)
      } else {
        setUploadStatusText(`Flash failed: ${xhr.responseText || 'Error ' + xhr.status}`)
        onToast('Firmware flash failed!')
        setUploading(false)
      }
    })

    xhr.addEventListener('error', () => {
      setUploadStatusText('Network error during firmware flash.')
      onToast('Upload network error')
      setUploading(false)
    })

    const formData = new FormData()
    formData.append('update', file)
    xhr.open('POST', url)
    xhr.send(formData)
  }

  const handleGithubInstall = async () => {
    const targetUrl = cloudRelease?.downloadUrl
    if (!targetUrl) {
      onToast('No compiled .bin asset found in this release.')
      return
    }

    if (!confirmInstall) {
      setConfirmInstall(true)
      setTimeout(() => setConfirmInstall(false), 5000)
      return
    }

    setConfirmInstall(false)
    const targetTag = cloudRelease?.tag || 'Latest Release'
    setInstallingGithub(true)
    setOtaProgressText('Initiating OTA task on Kiwi...')
    onToast(`Starting wireless update to ${targetTag}...`)

    try {
      const formData = new URLSearchParams()
      formData.append('url', targetUrl)

      const res = await fetch(`${getBaseUrl()}/ota/install`, {
        method: 'POST',
        headers: { 'Content-Type': 'application/x-www-form-urlencoded' },
        body: formData.toString()
      })
      const data = await res.json().catch(() => ({}))
      if (!data.ok) {
        onToast(`Install error: ${data.error || 'Failed to start OTA'}`)
        setInstallingGithub(false)
        setOtaProgressText(`Install error: ${data.error || 'Failed'}`)
        return
      }

      onToast('OTA started! Streaming firmware from GitHub...')
      if (pollTimerRef.current) clearInterval(pollTimerRef.current)
      if (reloadTimerRef.current) clearTimeout(reloadTimerRef.current)
      let sawActive = false
      let disconnectCount = 0
      let idleAfterStartCount = 0

      pollTimerRef.current = setInterval(async () => {
        try {
          const sRes = await fetch(`${getBaseUrl()}/ota/status`, {
            signal: AbortSignal.timeout(2500)
          })
          if (sRes.ok) {
            const sData = await sRes.json()
            setOtaProgress(sData)
            if (sData.state === 'starting') {
              sawActive = true
              setOtaProgressText('Preparing GitHub OTA download...')
            } else if (sData.state === 'downloading') {
              sawActive = true
              const readKb = ((sData.bytes_read || 0) / 1024).toFixed(0)
              const totalText = sData.bytes_total > 0
                ? ` (${readKb} KB / ${(sData.bytes_total / 1024).toFixed(0)} KB)`
                : ` (${readKb} KB)`
              setOtaProgressText(`Downloading... ${sData.progress || 0}%${totalText}`)
            } else if (sData.state === 'flashing') {
              sawActive = true
              setOtaProgressText(`Flashing OTA partition... ${sData.progress || 0}%`)
            } else if (sData.state === 'success') {
              clearInterval(pollTimerRef.current)
              pollTimerRef.current = null
              setOtaProgressText('Update installed! Kiwi is rebooting...')
              onToast('OTA Update successful! Rebooting...')
              reloadTimerRef.current = setTimeout(() => window.location.reload(), 5000)
            } else if (sData.state === 'error') {
              clearInterval(pollTimerRef.current)
              pollTimerRef.current = null
              setOtaProgressText(`OTA Error: ${sData.error || 'Download failed'}`)
              setInstallingGithub(false)
              onToast(`OTA Error: ${sData.error || 'Failed'}`)
            } else if (sData.state === 'idle') {
              if (sawActive || disconnectCount >= 2) {
                clearInterval(pollTimerRef.current)
                pollTimerRef.current = null
                setOtaProgressText('Kiwi rebooted with updated firmware! Reloading...')
                onToast('OTA update complete! Reloading console...')
                reloadTimerRef.current = setTimeout(() => window.location.reload(), 1200)
              } else {
                idleAfterStartCount += 1
                if (idleAfterStartCount >= 5) {
                  clearInterval(pollTimerRef.current)
                  pollTimerRef.current = null
                  setOtaProgressText('')
                  setInstallingGithub(false)
                  onToast('OTA session returned idle — check connection and retry.')
                }
              }
            }
          }
        } catch {
          disconnectCount += 1
          if (sawActive) {
            setOtaProgressText('Kiwi is rebooting... Waiting for reconnect...')
          }
        }
      }, 800)
    } catch (e) {
      onToast(`OTA install error: ${e.message}`)
      setInstallingGithub(false)
    }
  }

  const releaseLabel = cloudRelease
    ? (cloudRelease.name && cloudRelease.name.trim().toLowerCase() !== cloudRelease.tag.trim().toLowerCase()
        ? `${cloudRelease.tag} — ${cloudRelease.name}`
        : cloudRelease.tag)
    : (cloudError || 'Unavailable')

  const fileSizeMb = file ? (file.size / 1024 / 1024).toFixed(2) : null
  const fileOverLimit = file ? file.size > APP_PARTITION_LIMIT_BYTES : false
  const partitionPct = file ? Math.min(100, Math.round((file.size / APP_PARTITION_LIMIT_BYTES) * 100)) : 0

  const summaryStats = [
    { label: 'INSTALLED', value: displayVersion, color: 'var(--text-dark)' },
    {
      label: 'CLOUD LATEST',
      value: checkingCloud ? '...' : (cloudRelease?.tag || '—'),
      color: cloudRelease?.updateAvailable ? '#38BDF8' : 'var(--text-dark)'
    },
    { label: 'OTA LIMIT', value: '1.94 MB', color: 'var(--text-dark)' }
  ]

  return (
    <div style={{ display: 'flex', flexDirection: 'column', gap: '18px' }}>
      {/* Header Row */}
      <div style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'center', gap: '10px', flexWrap: 'nowrap' }}>
        <h1
          className="flux-main-title"
          style={{
            flex: 1,
            minWidth: 0,
            fontSize: 'clamp(19px, 5.2vw, 28px)',
            overflow: 'hidden',
            textOverflow: 'ellipsis',
            whiteSpace: 'nowrap'
          }}
        >
          OTA Firmware Flasher
        </h1>
        <Tooltip title="Refresh GitHub releases">
          <button
            type="button"
            onClick={checkGithubReleases}
            disabled={checkingCloud}
            className="flux-btn"
            style={{
              background: 'var(--card-muted)',
              color: 'var(--text-dark)',
              border: '1px solid var(--border-light)',
              boxShadow: 'none',
              padding: '6px 12px',
              borderRadius: '999px',
              fontSize: '11.5px',
              flexShrink: 0
            }}
          >
            <RefreshCw size={12} className={checkingCloud ? 'spin-fast' : ''} />
            <span>{checkingCloud ? 'Checking...' : 'Check Updates'}</span>
          </button>
        </Tooltip>
      </div>

      {/* Compact Centered Firmware Info Strip */}
      <div
        className="flux-white-card"
        style={{
          padding: '12px 8px',
          display: 'grid',
          gridTemplateColumns: 'repeat(3, minmax(0, 1fr))',
          alignItems: 'center'
        }}
      >
        {summaryStats.map((stat, idx) => (
          <div
            key={stat.label}
            style={{
              display: 'flex',
              flexDirection: 'column',
              alignItems: 'center',
              justifyContent: 'center',
              textAlign: 'center',
              padding: '0 6px',
              minWidth: 0,
              borderRight: idx < summaryStats.length - 1 ? '1px solid var(--border-light)' : 'none'
            }}
          >
            <div style={{ fontSize: '9.5px', fontWeight: 700, color: 'var(--text-muted)', letterSpacing: '0.04em' }}>
              {stat.label}
            </div>
            <div style={{
              fontSize: '14px',
              fontWeight: 800,
              color: stat.color,
              marginTop: '2px',
              maxWidth: '100%',
              overflow: 'hidden',
              textOverflow: 'ellipsis',
              whiteSpace: 'nowrap'
            }}>
              {stat.value}
            </div>
          </div>
        ))}
      </div>

      <div className="flux-grid-2">
        {/* Manual .bin Upload Card */}
        <div className="flux-white-card" style={{ display: 'flex', flexDirection: 'column', gap: '14px' }}>
          <div className="flux-card-header" style={{ marginBottom: 0 }}>
            <span className="flux-card-title">
              <UploadCloud size={16} />
              <span>Manual Upload (.bin)</span>
            </span>
          </div>

          <p style={{ fontSize: '12.5px', color: 'var(--text-muted)', margin: 0, lineHeight: 1.45 }}>
            Select or drop a compiled binary from <code style={{ color: 'var(--text-dark)' }}>.pio/build/esp32s3/firmware.bin</code>.
          </p>

          {/* Drag & Drop Selector */}
          <div
            onClick={() => fileInputRef.current && fileInputRef.current.click()}
            onDragOver={(e) => { e.preventDefault(); setDragOver(true) }}
            onDragLeave={() => setDragOver(false)}
            onDrop={handleDrop}
            style={{
              border: dragOver
                ? '2px dashed #38BDF8'
                : (fileOverLimit ? '2px dashed #F43F5E' : '1.5px dashed var(--border-dark)'),
              borderRadius: '16px',
              padding: '20px 16px',
              textAlign: 'center',
              cursor: 'pointer',
              background: dragOver ? 'rgba(56, 189, 248, 0.08)' : 'var(--card-subtle)',
              display: 'flex',
              flexDirection: 'column',
              alignItems: 'center',
              gap: '10px',
              position: 'relative',
              transition: 'border-color 0.15s ease, background 0.15s ease'
            }}
          >
            <input
              ref={fileInputRef}
              type="file"
              accept=".bin"
              onChange={handleFileChange}
              style={{ display: 'none' }}
            />

            {file && !uploading && (
              <button
                type="button"
                onClick={handleClearFile}
                aria-label="Clear selected binary"
                style={{
                  position: 'absolute',
                  top: '10px',
                  right: '10px',
                  background: 'var(--card-muted)',
                  border: '1px solid var(--border-light)',
                  color: 'var(--text-muted)',
                  borderRadius: '50%',
                  width: '24px',
                  height: '24px',
                  display: 'flex',
                  alignItems: 'center',
                  justifyContent: 'center',
                  cursor: 'pointer',
                  padding: 0
                }}
              >
                <X size={13} />
              </button>
            )}

            <div style={{
              width: '42px',
              height: '42px',
              borderRadius: '50%',
              background: file ? 'rgba(56, 189, 248, 0.14)' : 'var(--card-muted)',
              display: 'flex',
              alignItems: 'center',
              justifyContent: 'center'
            }}>
              <FileCode size={20} color={fileOverLimit ? '#F43F5E' : (file ? '#38BDF8' : 'var(--text-muted)')} />
            </div>

            <div style={{ maxWidth: '100%' }}>
              <p style={{
                fontSize: '13px',
                fontWeight: 700,
                color: 'var(--text-dark)',
                margin: 0,
                overflow: 'hidden',
                textOverflow: 'ellipsis',
                whiteSpace: 'nowrap'
              }}>
                {file ? file.name : 'Click or drop firmware .bin file'}
              </p>
              <p style={{ fontSize: '11.5px', color: fileOverLimit ? '#F43F5E' : 'var(--text-muted)', margin: '4px 0 0' }}>
                {file
                  ? `${fileSizeMb} MB (${partitionPct}% of 1.94 MB partition)${fileOverLimit ? ' — too large' : ''}`
                  : 'ESP32-S3 OTA partition max: 1.94 MB'}
              </p>
            </div>
          </div>

          {uploading && (
            <div>
              <div style={{ display: 'flex', justifyContent: 'space-between', fontSize: '11.5px', marginBottom: '6px', gap: '8px' }}>
                <span style={{ fontWeight: 600, color: 'var(--text-dark)' }}>{uploadStatusText}</span>
                <strong>{progress}%</strong>
              </div>
              <div style={{ height: '7px', background: 'var(--slider-track)', borderRadius: '999px', overflow: 'hidden' }}>
                <div style={{
                  height: '100%',
                  width: `${progress}%`,
                  background: '#0EA5E9',
                  transition: 'width 0.15s ease'
                }} />
              </div>
            </div>
          )}

          <button
            type="button"
            onClick={handleUploadFirmware}
            disabled={!file || uploading || fileOverLimit}
            className="flux-btn"
            style={{
              padding: '11px 16px',
              justifyContent: 'center',
              boxShadow: (!file || uploading || fileOverLimit) ? 'none' : undefined
            }}
          >
            <UploadCloud size={15} />
            <span>{uploading ? `Flashing ${progress}%...` : 'Flash Firmware to Kiwi'}</span>
          </button>
        </div>

        {/* GitHub Wireless Update Card */}
        <div className="flux-white-card" style={{ display: 'flex', flexDirection: 'column', justifyContent: 'space-between', gap: '14px' }}>
          <div style={{ display: 'flex', flexDirection: 'column', gap: '12px' }}>
            <div className="flux-card-header" style={{ marginBottom: 0, gap: '10px' }}>
              <span className="flux-card-title">
                <DownloadCloud size={16} />
                <span>GitHub Cloud Releases</span>
              </span>
              <span style={{
                background: (cloudRelease && !cloudRelease.updateAvailable) ? 'rgba(16, 185, 129, 0.12)' : 'var(--card-muted)',
                border: (cloudRelease && !cloudRelease.updateAvailable) ? '1px solid rgba(16, 185, 129, 0.28)' : '1px solid var(--border-light)',
                padding: '3px 9px',
                borderRadius: '999px',
                fontSize: '10.5px',
                fontWeight: 700,
                color: (cloudRelease && !cloudRelease.updateAvailable) ? '#10B981' : 'var(--text-muted)',
                whiteSpace: 'nowrap',
                flexShrink: 0
              }}>
                {(cloudRelease && !cloudRelease.updateAvailable) ? `${displayVersion} · Latest` : `${displayVersion} Installed`}
              </span>
            </div>

            <div style={{
              background: 'var(--card-subtle)',
              border: '1px solid var(--border-light)',
              padding: '14px',
              borderRadius: '14px',
              display: 'flex',
              flexDirection: 'column',
              gap: '8px'
            }}>
              <div style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'center', gap: '8px', fontSize: '12.5px' }}>
                <span style={{ color: 'var(--text-muted)', fontWeight: 600 }}>Latest Release</span>
                <strong style={{ color: cloudRelease?.updateAvailable ? '#38BDF8' : 'var(--text-dark)', textAlign: 'right' }}>
                  {checkingCloud ? 'Checking GitHub...' : releaseLabel}
                </strong>
              </div>

              {cloudRelease?.binName && (
                <div style={{ fontSize: '11.5px', color: 'var(--text-muted)', display: 'flex', justifyContent: 'space-between' }}>
                  <span>Binary Asset</span>
                  <strong style={{ color: 'var(--text-dark)' }}>
                    {cloudRelease.binName} ({(cloudRelease.binSize / 1024 / 1024).toFixed(2)} MB)
                  </strong>
                </div>
              )}

              {cloudRelease?.noteLines?.length > 0 && (
                <div style={{
                  marginTop: '4px',
                  paddingTop: '8px',
                  borderTop: '1px solid var(--border-light)',
                  maxHeight: '115px',
                  overflowY: 'auto',
                  display: 'flex',
                  flexDirection: 'column',
                  gap: '4px'
                }}>
                  {cloudRelease.noteLines.map((ln, idx) => (
                    <div
                      key={idx}
                      style={{
                        fontSize: ln.isHeading ? '12px' : '11.5px',
                        fontWeight: ln.isHeading ? 700 : 400,
                        color: ln.isHeading ? 'var(--text-dark)' : 'var(--text-muted)',
                        lineHeight: 1.4,
                        paddingLeft: ln.isBullet ? '8px' : 0
                      }}
                    >
                      {ln.isBullet ? `• ${ln.text}` : ln.text}
                    </div>
                  ))}
                </div>
              )}
            </div>

            {installingGithub && (
              <div>
                <div style={{ display: 'flex', justifyContent: 'space-between', fontSize: '11.5px', marginBottom: '6px', color: 'var(--text-dark)' }}>
                  <span>{otaProgressText || 'Downloading firmware from GitHub...'}</span>
                  <strong>{otaProgress?.progress || 0}%</strong>
                </div>
                <div style={{ height: '7px', background: 'var(--card-muted)', borderRadius: '999px', overflow: 'hidden' }}>
                  <div style={{
                    height: '100%',
                    width: `${otaProgress?.progress || 0}%`,
                    background: '#0EA5E9',
                    transition: 'width 0.15s ease'
                  }} />
                </div>
              </div>
            )}
          </div>

          <div style={{ display: 'flex', flexDirection: 'column', gap: '8px' }}>
            <button
              type="button"
              onClick={cloudError && !cloudRelease ? checkGithubReleases : handleGithubInstall}
              disabled={
                installingGithub ||
                checkingCloud ||
                (!cloudRelease && !cloudError) ||
                (!cloudRelease?.updateAvailable && Boolean(cloudRelease) && !confirmInstall) ||
                (cloudRelease?.updateAvailable && !cloudRelease?.hasBin)
              }
              className="flux-btn"
              style={{
                padding: '11px 16px',
                justifyContent: 'center',
                background: installingGithub
                  ? 'var(--card-muted)'
                  : (confirmInstall
                    ? '#D97706'
                    : (cloudRelease?.updateAvailable
                      ? 'linear-gradient(135deg, #0EA5E9 0%, #0284C7 100%)'
                      : (cloudRelease ? 'rgba(16, 185, 129, 0.12)' : 'var(--card-muted)'))),
                color: installingGithub
                  ? 'var(--text-muted)'
                  : (confirmInstall || cloudRelease?.updateAvailable
                    ? '#FFFFFF'
                    : (cloudRelease ? '#10B981' : 'var(--text-muted)')),
                border: (!cloudRelease?.updateAvailable && cloudRelease && !confirmInstall)
                  ? '1px solid rgba(16, 185, 129, 0.28)'
                  : '1px solid var(--border-light)',
                boxShadow: (cloudRelease?.updateAvailable && !confirmInstall) ? '0 3px 12px rgba(14, 165, 233, 0.28)' : 'none',
                cursor: (!cloudRelease?.updateAvailable && cloudRelease && !confirmInstall) ? 'default' : 'pointer'
              }}
            >
              {cloudRelease && !cloudRelease.updateAvailable && !confirmInstall ? (
                <CheckCircle2 size={15} color="#10B981" />
              ) : (
                <DownloadCloud size={15} />
              )}
              <span>
                {installingGithub
                  ? 'Installing Over Wi-Fi...'
                  : (checkingCloud
                    ? 'Checking GitHub Releases...'
                    : (confirmInstall
                      ? `Tap Again to Flash ${cloudRelease?.tag}`
                      : (cloudRelease?.updateAvailable
                        ? `Install ${cloudRelease.tag} Wirelessly`
                        : (cloudRelease
                          ? `Up to Date (${cloudRelease.tag})`
                          : (cloudError ? 'Retry Checking GitHub' : 'Firmware is Up to Date')))))}
              </span>
            </button>

            {cloudRelease && !cloudRelease.updateAvailable && cloudRelease.hasBin && !installingGithub && (
              <div style={{ textAlign: 'center' }}>
                <button
                  type="button"
                  onClick={handleGithubInstall}
                  style={{
                    background: 'transparent',
                    border: 'none',
                    color: 'var(--text-muted)',
                    fontSize: '11px',
                    textDecoration: 'underline',
                    cursor: 'pointer'
                  }}
                >
                  {confirmInstall ? `Confirm reinstall ${cloudRelease.tag}` : `Reinstall ${cloudRelease.tag} anyway`}
                </button>
              </div>
            )}
          </div>
        </div>
      </div>
    </div>
  )
}
