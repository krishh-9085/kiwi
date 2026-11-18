import React, { useState, useEffect, useRef } from 'react'
import {
  DownloadCloud, UploadCloud, RefreshCw, FileCode, Sparkles
} from 'lucide-react'
import { Tooltip } from './base/tooltip/tooltip'

export default function OtaUpdateView({ status, getBaseUrl, onToast }) {
  const [file, setFile] = useState(null)
  const [uploading, setUploading] = useState(false)
  const [progress, setProgress] = useState(0)
  const [uploadStatusText, setUploadStatusText] = useState('')
  const [cloudRelease, setCloudRelease] = useState(null)
  const [checkingCloud, setCheckingCloud] = useState(false)
  const [cloudError, setCloudError] = useState(null)
  const [installingGithub, setInstallingGithub] = useState(false)
  const [otaProgress, setOtaProgress] = useState(null)
  const [otaProgressText, setOtaProgressText] = useState('')
  const fileInputRef = useRef(null)

  const compareVersions = (v1, v2) => {
    const parse = (v) => (v || '').replace(/^[vV]/, '').split('.').map(n => parseInt(n, 10) || 0)
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

  const checkGithubReleases = async () => {
    setCheckingCloud(true)
    setCloudError(null)
    try {
      const res = await fetch('https://api.github.com/repos/krishh-9085/kiwi/releases/latest', {
        headers: { 'Accept': 'application/vnd.github.v3+json' }
      })
      if (!res.ok) {
        throw new Error(`GitHub API returned status ${res.status}`)
      }
      const data = await res.json()
      const rawTag = data.tag_name || data.name || ''
      const cleanVer = rawTag.replace(/^[vV]/, '')
      const binAsset = data.assets?.find(a => a.name?.toLowerCase().endsWith('.bin'))
      const currentVer = (status?.version || '2.0').replace(/^[vV]/, '')
      const isNewer = compareVersions(cleanVer, currentVer) > 0

      setCloudRelease({
        tag: rawTag,
        version: cleanVer,
        name: data.name || rawTag,
        downloadUrl: binAsset?.browser_download_url || '',
        binName: binAsset?.name || 'firmware.bin',
        binSize: binAsset?.size || 0,
        notes: data.body ? (data.body.length > 240 ? data.body.slice(0, 240) + '...' : data.body) : '',
        updateAvailable: isNewer,
        hasBin: Boolean(binAsset)
      })
    } catch (err) {
      console.warn('Failed to check GitHub releases:', err)
      setCloudError('Unable to check GitHub repository directly.')
    } finally {
      setCheckingCloud(false)
    }
  }

  useEffect(() => {
    checkGithubReleases()
  }, [])

  const handleFileChange = (e) => {
    if (e.target.files && e.target.files[0]) {
      const selected = e.target.files[0]
      if (!selected.name.endsWith('.bin')) {
        onToast('Please select a compiled firmware .bin file!')
        return
      }
      setFile(selected)
      onToast(`Selected: ${selected.name} (${(selected.size / 1024 / 1024).toFixed(2)} MB)`)
    }
  }

  const handleUploadFirmware = (e) => {
    e.preventDefault()
    if (!file) return

    if (file.size > 0x1E0000) {
      onToast('Error: Firmware file exceeds app partition (1.9 MB limit)!')
      return
    }

    setUploading(true)
    setProgress(0)
    setUploadStatusText('Uploading firmware binary to ESP32...')

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
        setUploadStatusText('✓ Firmware flash successful! Kiwi is restarting to boot new firmware...')
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
    const targetUrl = cloudRelease?.downloadUrl || 'https://github.com/krishh-9085/kiwi/releases/latest/download/KIWI_V1.2.bin'
    const targetTag = cloudRelease?.tag || 'Latest Release'

    if (!confirm(`Download and install ${targetTag} directly onto Kiwi over Wi-Fi?`)) {
      return
    }

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

      onToast('OTA started! Streaming firmware directly from GitHub...')
      const pollTimer = setInterval(async () => {
        try {
          const sRes = await fetch(`${getBaseUrl()}/ota/status`)
          if (sRes.ok) {
            const sData = await sRes.json()
            setOtaProgress(sData)
            if (sData.state === 'downloading') {
              setOtaProgressText(`Downloading from GitHub... ${sData.progress || 0}% (${((sData.bytes_read || 0) / 1024).toFixed(0)} KB / ${((sData.bytes_total || 1) / 1024).toFixed(0)} KB)`)
            } else if (sData.state === 'flashing') {
              setOtaProgressText(`Flashing OTA partition... ${sData.progress || 0}%`)
            } else if (sData.state === 'success') {
              clearInterval(pollTimer)
              setOtaProgressText('✓ Update installed successfully! Kiwi is rebooting...')
              onToast('OTA Update successful! Rebooting...')
              setTimeout(() => window.location.reload(), 6000)
            } else if (sData.state === 'error') {
              clearInterval(pollTimer)
              setOtaProgressText(`OTA Error: ${sData.error || 'Download failed'}`)
              setInstallingGithub(false)
              onToast(`OTA Error: ${sData.error || 'Failed'}`)
            }
          }
        } catch {
          // Ignore network errors while robot reboots
        }
      }, 1200)
    } catch (e) {
      onToast(`OTA install triggered: ${e.message}`)
      setInstallingGithub(false)
    }
  }

  return (
    <div style={{ display: 'flex', flexDirection: 'column', gap: '24px' }}>
      {/* Title */}
      <div className="flux-title-section" style={{ alignItems: 'center' }}>
        <div>
          <h1 className="flux-main-title">OTA Firmware Flasher</h1>
        </div>
        <Tooltip title="Check Updates">
          <button
            onClick={checkGithubReleases}
            disabled={checkingCloud}
            className="flux-btn"
            style={{ background: 'var(--card-white)', color: 'var(--text-dark)', border: '1px solid var(--border-light)' }}
          >
            <RefreshCw size={13} className={checkingCloud ? 'pulse-active' : ''} />
            <span>{checkingCloud ? 'Checking...' : 'Check Updates'}</span>
          </button>
        </Tooltip>
      </div>

      <div className="flux-grid-2">
        {/* Manual .bin Upload White Card */}
        <div className="flux-white-card" style={{ display: 'flex', flexDirection: 'column', gap: '16px' }}>
          <div className="flux-card-header">
            <span className="flux-card-title">
              <UploadCloud size={16} />
              <span>Manual Firmware Upload (.bin)</span>
            </span>
          </div>

          <p style={{ fontSize: '13px', color: 'var(--text-muted)', margin: 0 }}>
            Upload compiled binary (e.g. <code>.pio/build/esp32/firmware.bin</code>).
          </p>

          {/* Drag & Drop Area */}
          <div
            onClick={() => fileInputRef.current && fileInputRef.current.click()}
            style={{
              border: '2px dashed var(--border-light)',
              borderRadius: '20px',
              padding: '36px 20px',
              textAlign: 'center',
              cursor: 'pointer',
              background: 'var(--card-subtle)',
              display: 'flex',
              flexDirection: 'column',
              alignItems: 'center',
              gap: '12px'
            }}
          >
            <input
              ref={fileInputRef}
              type="file"
              accept=".bin"
              onChange={handleFileChange}
              style={{ display: 'none' }}
            />

            <div style={{
              width: '54px', height: '54px', borderRadius: '50%',
              background: 'rgba(14, 165, 233, 0.15)', display: 'flex', alignItems: 'center', justifyContent: 'center'
            }}>
              <FileCode size={26} color="#38BDF8" />
            </div>

            <div>
              <p style={{ fontSize: '14px', fontWeight: 700, color: 'var(--text-dark)', margin: 0 }}>
                {file ? file.name : 'Click to select Firmware .bin file'}
              </p>
              <p style={{ fontSize: '12px', color: 'var(--text-muted)', margin: '4px 0 0' }}>
                {file ? `${(file.size / 1024 / 1024).toFixed(2)} MB` : 'App partition limit: 1.9 MB'}
              </p>
            </div>
          </div>

          {/* Progress bar */}
          {uploading && (
            <div>
              <div style={{ display: 'flex', justifyContent: 'space-between', fontSize: '12px', marginBottom: '6px' }}>
                <span style={{ fontWeight: 600 }}>{uploadStatusText}</span>
                <strong>{progress}%</strong>
              </div>
              <div style={{ height: '8px', background: 'var(--slider-track)', borderRadius: '999px', overflow: 'hidden' }}>
                <div style={{
                  height: '100%', width: `${progress}%`,
                  background: 'linear-gradient(90deg, #38BDF8, #0EA5E9)'
                }} />
              </div>
            </div>
          )}

          <button
            onClick={handleUploadFirmware}
            disabled={!file || uploading}
            className="flux-btn"
            style={{ padding: '12px', justifyContent: 'center' }}
          >
            <UploadCloud size={16} />
            <span>{uploading ? 'Flashing OTA Firmware...' : 'Flash Firmware to Kiwi'}</span>
          </button>
        </div>

        {/* GitHub Wireless Update Dark Card */}
        <div className="flux-dark-card" style={{ display: 'flex', flexDirection: 'column', justifyContent: 'space-between' }}>
          <div>
            <div style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'center', marginBottom: '16px' }}>
              <span className="flux-card-title">
                <Sparkles size={16} color="#38BDF8" />
                <span>GitHub Cloud Wireless Releases</span>
              </span>
              <span style={{
                background: '#242730', padding: '4px 10px', borderRadius: '999px',
                fontSize: '11px', color: '#FFFFFF'
              }}>
                v{status?.version || '2.0'} Installed
              </span>
            </div>

            <p style={{ fontSize: '13px', color: 'var(--text-light-muted)', lineHeight: 1.5 }}>
              Kiwi can autonomously download and install verified firmware releases directly from the cloud repository.
            </p>

            <div style={{
              background: '#0E1013', padding: '16px', borderRadius: '16px',
              margin: '20px 0', display: 'flex', flexDirection: 'column', gap: '8px'
            }}>
              <div style={{ display: 'flex', justifyContent: 'space-between', fontSize: '13px' }}>
                <span style={{ color: 'var(--text-light-muted)' }}>Latest Cloud Release:</span>
                <strong style={{ color: cloudRelease?.updateAvailable ? '#38BDF8' : '#FFFFFF' }}>
                  {checkingCloud ? 'Checking GitHub repo...' : (cloudRelease ? `${cloudRelease.tag} (${cloudRelease.name})` : (cloudError || 'No releases found'))}
                </strong>
              </div>
              {cloudRelease?.binName && (
                <div style={{ fontSize: '11px', color: '#9CA3AF' }}>
                  Firmware Asset: <strong>{cloudRelease.binName}</strong> ({(cloudRelease.binSize / 1024 / 1024).toFixed(2)} MB)
                </div>
              )}
              {cloudRelease?.notes && (
                <div style={{ fontSize: '11px', color: '#6B7280', maxHeight: '70px', overflowY: 'auto', whiteSpace: 'pre-line' }}>
                  {cloudRelease.notes}
                </div>
              )}
            </div>

            {/* Live Progress Bar for GitHub Wireless OTA */}
            {installingGithub && (
              <div style={{ marginBottom: '16px' }}>
                <div style={{ display: 'flex', justifyContent: 'space-between', fontSize: '12px', marginBottom: '6px', color: '#FFFFFF' }}>
                  <span>{otaProgressText || 'Downloading firmware from GitHub...'}</span>
                  <strong>{otaProgress?.progress || 0}%</strong>
                </div>
                <div style={{ height: '8px', background: '#242730', borderRadius: '999px', overflow: 'hidden' }}>
                  <div style={{
                    height: '100%', width: `${otaProgress?.progress || 0}%`,
                    background: 'linear-gradient(90deg, #38BDF8, #0EA5E9)'
                  }} />
                </div>
              </div>
            )}
          </div>

          <button
            onClick={cloudError && !cloudRelease ? checkGithubReleases : handleGithubInstall}
            disabled={installingGithub || checkingCloud || (!cloudRelease && !cloudError)}
            className="flux-btn"
            style={{
              padding: '12px',
              justifyContent: 'center',
              background: installingGithub
                ? '#374151'
                : (cloudRelease?.updateAvailable
                  ? 'linear-gradient(135deg, #0EA5E9 0%, #0284C7 100%)'
                  : (cloudRelease ? 'rgba(14, 165, 233, 0.15)' : '#242730')),
              color: installingGithub
                ? '#9CA3AF'
                : (cloudRelease?.updateAvailable
                  ? '#FFFFFF'
                  : (cloudRelease ? '#38BDF8' : '#9CA3AF')),
              border: (!cloudRelease?.updateAvailable && cloudRelease)
                ? '1px solid rgba(56, 189, 248, 0.4)'
                : 'none',
              boxShadow: cloudRelease?.updateAvailable ? '0 4px 16px rgba(14, 165, 233, 0.35)' : 'none'
            }}
          >
            <DownloadCloud size={16} />
            <span>
              {installingGithub
                ? 'Installing Firmware Over Wi-Fi...'
                : (checkingCloud
                  ? 'Checking GitHub Releases...'
                  : (cloudRelease?.updateAvailable
                    ? `⚡ Install ${cloudRelease.tag} Wirelessly`
                    : (cloudRelease
                      ? `Install ${cloudRelease.tag} Wirelessly`
                      : (cloudError ? 'Retry Checking GitHub' : 'Firmware is Up-To-Date'))))}
            </span>
          </button>
        </div>
      </div>
    </div>
  )
}
