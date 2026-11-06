#include "animation/AnimationEngine.h"
#include "config/AppConfig.h"
#include "config/ConfigManager.h"
#include "face/FaceEngine.h"
#include "hardware/AudioEngine.h"
#include "services/AiService.h"
#include "services/MemoryService.h"
#include "services/WeatherService.h"
#include "web/WebPortal.h"
#include <DNSServer.h>
#include <Preferences.h>
#include <Update.h>
#include <WebServer.h>
#include <WiFi.h>
#include <time.h>


void handleOtaPage() {
  static const char OTA_HTML[] PROGMEM = R"rawliteral(<!doctype html>
<html lang="en">

<head>
  <meta charset="utf-8">
  <meta name="viewport" content="width=device-width,initial-scale=1,viewport-fit=cover">
  <meta name="color-scheme" content="dark">
  <title>Kiwi Robot · Firmware OTA</title>
  <link rel="apple-touch-icon" sizes="180x180" href="/apple-touch-icon.png">
  <link rel="icon" type="image/png" href="/apple-touch-icon.png">
  <meta name="apple-mobile-web-app-title" content="Kiwi">
  <meta name="apple-mobile-web-app-capable" content="yes">
  <meta name="mobile-web-app-capable" content="yes">
  <meta name="apple-mobile-web-app-status-bar-style" content="black-translucent">
  <meta name="theme-color" content="#06080d">
  <link rel="icon"
    href="data:image/svg+xml,%3Csvg xmlns='http://www.w3.org/2000/svg' viewBox='0 0 64 64'%3E%3Crect width='64' height='64' rx='12' fill='%2306080d'/%3E%3Crect x='14' y='22' width='10' height='10' fill='%23f2b45c'/%3E%3Crect x='40' y='22' width='10' height='10' fill='%23f2b45c'/%3E%3Cpath d='M20 42h24' stroke='%23f2b45c' stroke-width='4'/%3E%3C/svg%3E">
  <style>
    :root {
      color-scheme: dark;
      --void: #06080d;
      --panel: #0d121b;
      --panel-2: #111826;
      --well: #090c13;
      --line: #1d2533;
      --line-strong: #2c3648;
      --ink: #ece6d9;
      --muted: #a0abbd;
      --astro: #ff4a3d;
      --astro-glow: rgba(255, 74, 61, .35);
      --amber: #f2b45c;
      --amber-dim: rgba(242, 180, 92, .16);
      --amber-glow: rgba(242, 180, 92, .38);
      --ok: #8fd19e;
      --glass: rgba(13, 18, 27, .88);
      --glass-edge: rgba(70, 84, 108, .55);
      --display: "Avenir Next", "Helvetica Neue", sans-serif;
      --body: "Helvetica Neue", Arial, sans-serif;
      --mono: "SF Mono", Menlo, monospace;
      --cut: 14px;
      --cut-sm: 7px;
    }

    * {
      box-sizing: border-box;
    }

    body {
      margin: 0;
      font: 14px/1.5 var(--body);
      color: var(--ink);
      background: var(--void);
      min-height: 100vh;
      overflow-x: hidden;
    }

    body::before {
      content: "";
      position: fixed;
      inset: 0;
      z-index: -1;
      pointer-events: none;
      background-image: linear-gradient(rgba(242, 180, 92, .035) 1px, transparent 1px), linear-gradient(90deg, rgba(242, 180, 92, .035) 1px, transparent 1px);
      background-size: 48px 48px;
      -webkit-mask-image: linear-gradient(#000, transparent 65%);
      mask-image: linear-gradient(#000, transparent 65%);
    }

    .wrap {
      width: min(1260px, 100%);
      margin: 0 auto;
      padding: 24px;
    }

    /* Bridge header */
    .bridge {
      display: flex;
      align-items: center;
      justify-content: space-between;
      gap: 16px;
      flex-wrap: wrap;
      padding: 8px 0 20px;
    }

    .id {
      display: flex;
      align-items: center;
      gap: 14px;
    }

    .bezel {
      width: 64px;
      height: 48px;
      border-radius: 12px;
      background: #000;
      border: 1px solid rgba(242, 180, 92, .55);
      box-shadow: 0 0 18px var(--amber-glow), inset 0 0 12px #000;
      display: flex;
      align-items: center;
      justify-content: center;
      gap: 9px;
      position: relative;
      overflow: hidden;
    }

    .bezel::after {
      content: "";
      position: absolute;
      inset: 0;
      background: repeating-linear-gradient(0deg, rgba(255, 255, 255, .03) 0 1px, transparent 1px 3px);
      pointer-events: none;
    }

    .bezel i {
      width: 9px;
      height: 12px;
      background: var(--amber);
      box-shadow: 0 0 8px var(--amber-glow);
      animation: blink 5s infinite;
    }

    .unit {
      font: 600 12px/1 var(--display);
      letter-spacing: .2em;
      text-transform: uppercase;
      color: var(--muted);
    }

    h1.brand {
      margin: 4px 0 2px;
      font: 600 18px/1.1 var(--display);
      letter-spacing: .25em;
      text-transform: uppercase;
    }

    .nav {
      display: flex;
      gap: 8px;
      flex-wrap: wrap;
      align-items: center;
    }

    .nb {
      font: 600 13px var(--body);
      color: var(--ink);
      text-decoration: none;
      background: var(--well);
      border: 1px solid var(--line-strong);
      padding: 10px 16px;
      min-height: 44px;
      clip-path: polygon(var(--cut-sm) 0, 100% 0, 100% calc(100% - var(--cut-sm)), calc(100% - var(--cut-sm)) 100%, 0 100%, 0 var(--cut-sm));
      transition: border-color .15s, background .15s, transform .1s;
      display: inline-flex;
      align-items: center;
    }

    .nb:hover {
      border-color: rgba(242, 180, 92, .6);
      background: rgba(242, 180, 92, .06);
    }

    .nb:active {
      transform: scale(.97);
    }

    .nb.on {
      background: var(--amber);
      border-color: var(--amber);
      color: #14100a;
      font-weight: 600;
    }

    .nb:focus-visible,
    .btn:focus-visible,
    .file-label:focus-visible,
    .btn-check-ota:focus-visible,
    .btn-install-ota:focus-visible {
      outline: 2px solid var(--amber);
      outline-offset: 2px;
    }

    /* Container card */
    .container {
      width: min(720px, 100%);
      margin: 24px auto 0;
    }

    .hull {
      position: relative;
      background: linear-gradient(180deg, rgba(242, 180, 92, .06), transparent 30%), var(--glass);
      border: 1px solid var(--glass-edge);
      padding: clamp(24px, 5vw, 36px);
      text-align: center;
      clip-path: polygon(var(--cut) 0, 100% 0, 100% calc(100% - var(--cut)), calc(100% - var(--cut)) 100%, 0 100%, 0 var(--cut));
    }

    .hull::before {
      content: "";
      position: absolute;
      top: 0;
      left: var(--cut);
      width: 36px;
      height: 2px;
      background: var(--amber);
      box-shadow: 0 0 8px var(--amber-glow);
    }

    .fw-meta-pill {
      display: inline-flex;
      align-items: center;
      gap: 8px;
      padding: 5px 16px;
      border-radius: 999px;
      background: var(--well);
      border: 1px solid var(--line-strong);
      font: 600 12px var(--mono);
      color: var(--amber);
      margin-bottom: 20px;
    }

    .glyph {
      width: 56px;
      height: 56px;
      margin: 0 auto 16px;
      display: flex;
      align-items: center;
      justify-content: center;
      background: #000;
      border: 1px solid rgba(242, 180, 92, .5);
      box-shadow: 0 0 16px var(--amber-glow), inset 0 0 10px #000;
      color: var(--amber);
    }

    h2 {
      font: 600 17px/1.2 var(--display);
      letter-spacing: .2em;
      text-transform: uppercase;
      margin: 0 0 8px;
    }

    .sub {
      color: var(--muted);
      font-size: 13px;
      margin: 0 0 20px;
      line-height: 1.5;
    }

    .sub code {
      font: 12px var(--mono);
      color: var(--amber);
    }

    /* GitHub Online OTA Card */
    .gh-card {
      background: var(--well);
      border: 1px solid var(--line-strong);
      border-radius: 8px;
      padding: 20px;
      text-align: left;
      margin-bottom: 24px;
      position: relative;
      box-shadow: inset 0 0 16px rgba(0, 0, 0, .4);
    }

    .gh-header {
      display: flex;
      align-items: center;
      justify-content: space-between;
      gap: 12px;
      flex-wrap: wrap;
      margin-bottom: 14px;
    }

    .gh-title-group {
      display: flex;
      align-items: center;
      gap: 10px;
      font: 600 13px var(--display);
      letter-spacing: .15em;
      text-transform: uppercase;
      color: var(--ink);
    }

    .gh-title-group svg {
      color: var(--amber);
      flex-shrink: 0;
    }

    .gh-repo-chip {
      display: inline-flex;
      align-items: center;
      gap: 6px;
      background: var(--panel);
      border: 1px solid var(--line);
      padding: 4px 10px;
      border-radius: 6px;
      font: 12px var(--mono);
      color: var(--muted);
      cursor: pointer;
      transition: border-color .15s, color .15s;
    }

    .gh-repo-chip:hover {
      border-color: var(--amber);
      color: var(--ink);
    }

    .gh-repo-chip svg {
      opacity: .65;
    }

    .repo-edit-row {
      display: flex;
      gap: 8px;
      margin-bottom: 14px;
    }

    .repo-input {
      flex: 1;
      background: var(--panel);
      border: 1px solid var(--line-strong);
      color: var(--ink);
      font: 12px var(--mono);
      padding: 7px 12px;
      border-radius: 4px;
      outline: none;
    }

    .repo-input:focus {
      border-color: var(--amber);
    }

    .btn-sm {
      background: var(--panel-2);
      border: 1px solid var(--line-strong);
      color: var(--ink);
      font: 600 11px var(--display);
      letter-spacing: .1em;
      text-transform: uppercase;
      padding: 6px 12px;
      border-radius: 4px;
      cursor: pointer;
      transition: all .15s;
    }

    .btn-sm:hover {
      border-color: var(--amber);
      color: var(--amber);
    }

    .btn-ghost {
      background: transparent;
      border-color: transparent;
      color: var(--muted);
    }

    .btn-ghost:hover {
      background: rgba(255, 255, 255, .05);
      color: var(--ink);
    }

    /* Primary "Update available?" button */
    .btn-check-ota {
      width: 100%;
      min-height: 46px;
      background: linear-gradient(180deg, rgba(242, 180, 92, .18), rgba(242, 180, 92, .06));
      border: 1px solid var(--amber);
      color: var(--amber);
      font: 600 13px var(--display);
      letter-spacing: .2em;
      text-transform: uppercase;
      clip-path: polygon(var(--cut-sm) 0, 100% 0, 100% calc(100% - var(--cut-sm)), calc(100% - var(--cut-sm)) 100%, 0 100%, 0 var(--cut-sm));
      cursor: pointer;
      display: inline-flex;
      align-items: center;
      justify-content: center;
      gap: 10px;
      transition: all .15s ease;
      box-shadow: 0 0 12px rgba(242, 180, 92, .15);
    }

    .btn-check-ota:hover:not(:disabled) {
      background: var(--amber);
      color: #14100a;
      box-shadow: 0 0 20px var(--amber-glow);
    }

    .btn-check-ota:disabled {
      opacity: .6;
      cursor: not-allowed;
    }

    .btn-check-ota svg.spinning {
      animation: spin 1s linear infinite;
    }

    @keyframes spin {
      100% {
        transform: rotate(360deg);
      }
    }

    /* Result box states */
    .gh-result-box {
      margin-top: 14px;
      padding: 16px;
      border-radius: 6px;
      background: var(--panel);
      border: 1px solid var(--line);
      font-size: 13px;
      line-height: 1.5;
    }

    .gh-result-box.uptodate {
      border-color: rgba(143, 209, 158, .4);
      background: rgba(143, 209, 158, .06);
      color: #bbf0c8;
    }

    .gh-result-box.update-avail {
      border-color: rgba(242, 180, 92, .6);
      background: rgba(242, 180, 92, .08);
      box-shadow: 0 0 16px rgba(242, 180, 92, .18);
    }

    .gh-result-box.error {
      border-color: rgba(255, 74, 61, .4);
      background: rgba(255, 74, 61, .06);
      color: #ff9d94;
    }

    .update-badge {
      display: inline-flex;
      align-items: center;
      gap: 6px;
      padding: 3px 10px;
      background: var(--amber);
      color: #14100a;
      font: 700 11px var(--mono);
      border-radius: 999px;
      letter-spacing: .08em;
      text-transform: uppercase;
      margin-bottom: 10px;
    }

    .release-title {
      font: 600 15px var(--display);
      color: var(--ink);
      margin: 0 0 4px;
      letter-spacing: .05em;
    }

    .update-meta-grid {
      display: grid;
      grid-template-columns: repeat(auto-fit, minmax(130px, 1fr));
      gap: 10px;
      margin: 12px 0;
      padding: 10px 14px;
      background: var(--well);
      border: 1px solid var(--line);
      border-radius: 4px;
    }

    .update-meta-item .meta-k {
      font: 11px var(--mono);
      color: var(--muted);
      text-transform: uppercase;
      margin-bottom: 2px;
    }

    .update-meta-item .meta-v {
      font: 600 13px var(--mono);
      color: var(--amber);
    }

    .release-body {
      max-height: 140px;
      overflow-y: auto;
      background: var(--void);
      border: 1px solid var(--line);
      border-radius: 4px;
      padding: 10px 12px;
      font: 12px/1.55 var(--mono);
      color: var(--ink);
      white-space: pre-wrap;
      word-break: break-word;
      margin: 12px 0 16px;
    }

    .btn-install-ota {
      width: 100%;
      min-height: 46px;
      background: var(--amber);
      border: 0;
      color: #14100a;
      font: 600 13px var(--display);
      letter-spacing: .2em;
      text-transform: uppercase;
      clip-path: polygon(var(--cut-sm) 0, 100% 0, 100% calc(100% - var(--cut-sm)), calc(100% - var(--cut-sm)) 100%, 0 100%, 0 var(--cut-sm));
      cursor: pointer;
      display: inline-flex;
      align-items: center;
      justify-content: center;
      gap: 8px;
      transition: background .15s, transform .1s, box-shadow .15s;
      box-shadow: 0 0 16px var(--amber-glow);
    }

    .btn-install-ota:hover:not(:disabled) {
      background: #ffc87a;
      box-shadow: 0 0 24px var(--amber-glow);
    }

    .btn-install-ota:active:not(:disabled) {
      transform: scale(.98);
    }

    .btn-install-ota:disabled {
      opacity: .5;
      cursor: not-allowed;
    }

    .direct-dl-link {
      display: inline-block;
      margin-top: 10px;
      font: 12px var(--mono);
      color: var(--muted);
      text-decoration: underline;
      cursor: pointer;
    }

    .direct-dl-link:hover {
      color: var(--amber);
    }

    /* Online OTA Live Progress Card */
    .online-progress-card {
      margin-top: 14px;
      padding: 16px;
      background: var(--void);
      border: 1px solid var(--amber);
      border-radius: 6px;
      box-shadow: 0 0 16px rgba(242, 180, 92, .2);
    }

    .online-progress-header {
      display: flex;
      justify-content: space-between;
      align-items: center;
      font: 600 12px var(--mono);
      color: var(--amber);
      margin-bottom: 8px;
    }

    .online-bar-track {
      width: 100%;
      height: 12px;
      background: var(--panel);
      border: 1px solid var(--line-strong);
      position: relative;
      overflow: hidden;
      border-radius: 3px;
    }

    .online-bar-fill {
      height: 100%;
      width: 0%;
      background: linear-gradient(90deg, var(--amber), #ffe0a3);
      box-shadow: 0 0 12px var(--amber-glow);
      transition: width .2s ease;
    }

    .online-progress-sub {
      margin-top: 8px;
      font: 12px var(--mono);
      color: var(--muted);
    }

    .section-divider {
      display: flex;
      align-items: center;
      gap: 14px;
      margin: 28px 0 20px;
      color: var(--muted);
      font: 600 11px var(--mono);
      letter-spacing: .18em;
    }

    .section-divider::before,
    .section-divider::after {
      content: "";
      flex: 1;
      height: 1px;
      background: var(--line-strong);
    }

    /* Warning banner */
    .warning-banner {
      display: flex;
      align-items: center;
      justify-content: center;
      gap: 10px;
      background: rgba(255, 74, 61, 0.12);
      border: 1px solid rgba(255, 74, 61, 0.35);
      color: #ff9d94;
      font-size: 12px;
      padding: 10px 14px;
      margin-bottom: 20px;
      text-align: left;
    }

    .warning-banner svg {
      flex-shrink: 0;
      stroke: var(--astro);
    }

    /* File picker: recessed dropzone */
    .file-picker {
      display: flex;
      flex-direction: column;
      align-items: center;
      gap: 12px;
      padding: 26px 16px;
      margin: 14px 0 8px;
      background: var(--well);
      border: 2px dashed rgba(242, 180, 92, .45);
      transition: border-color .15s, background .15s, box-shadow .15s;
      cursor: pointer;
    }

    .file-picker:hover,
    .file-picker.dragover,
    .file-picker:focus-within {
      border-color: var(--amber);
      background: rgba(242, 180, 92, .04);
      box-shadow: 0 0 0 3px rgba(242, 180, 92, .15);
    }

    .file-picker input {
      position: absolute;
      width: 1px;
      height: 1px;
      opacity: 0;
      pointer-events: none;
    }

    .file-label {
      display: inline-flex;
      align-items: center;
      justify-content: center;
      padding: 11px 24px;
      min-height: 44px;
      cursor: pointer;
      background: var(--panel);
      border: 1px solid var(--line-strong);
      color: var(--ink);
      font: 600 12px var(--display);
      letter-spacing: .18em;
      text-transform: uppercase;
      clip-path: polygon(var(--cut-sm) 0, 100% 0, 100% calc(100% - var(--cut-sm)), calc(100% - var(--cut-sm)) 100%, 0 100%, 0 var(--cut-sm));
      transition: border-color .15s, background .15s, transform .1s;
    }

    .file-label:hover {
      border-color: rgba(242, 180, 92, .6);
      background: rgba(242, 180, 92, .06);
    }

    .file-label:active {
      transform: scale(.97);
    }

    .file-name {
      font: 12px var(--mono);
      color: var(--muted);
      word-break: break-all;
      max-width: 90%;
    }

    .file-name.set {
      color: var(--ink);
      font-weight: 600;
    }

    .file-help {
      color: var(--muted);
      font-size: 12px;
      margin-bottom: 20px;
    }

    /* Primary action */
    .btn {
      width: 100%;
      min-height: 48px;
      border: 0;
      cursor: pointer;
      background: var(--amber);
      color: #14100a;
      font: 600 13px var(--display);
      letter-spacing: .22em;
      text-transform: uppercase;
      clip-path: polygon(var(--cut-sm) 0, 100% 0, 100% calc(100% - var(--cut-sm)), calc(100% - var(--cut-sm)) 100%, 0 100%, 0 var(--cut-sm));
      transition: background .15s, transform .1s, opacity .15s;
    }

    .btn:hover:not(:disabled) {
      background: #ffc87a;
    }

    .btn:active:not(:disabled) {
      transform: scale(.98);
    }

    .btn:disabled {
      background: var(--well);
      color: #5b6579;
      border: 1px solid var(--line);
      cursor: not-allowed;
      clip-path: none;
    }

    /* Progress */
    #prg {
      margin-top: 24px;
      width: 100%;
      height: 10px;
      background: var(--well);
      border: 1px solid var(--line);
      display: none;
      position: relative;
      overflow: hidden;
    }

    #bar {
      height: 100%;
      width: 0%;
      background: var(--amber);
      box-shadow: 0 0 12px var(--amber-glow);
      transition: width .15s ease;
    }

    #stat {
      margin-top: 16px;
      font: 13px var(--mono);
      min-height: 24px;
      color: var(--amber);
    }

    /* Collapsible partition help */
    .notice-details {
      margin-top: 28px;
      background: var(--well);
      border: 1px solid var(--line);
      border-left: 2px solid var(--amber);
      text-align: left;
    }

    .notice-details summary {
      padding: 12px 16px;
      cursor: pointer;
      font: 600 12px var(--display);
      letter-spacing: .14em;
      text-transform: uppercase;
      color: var(--ink);
      user-select: none;
    }

    .notice-details summary:hover {
      color: var(--amber);
    }

    .notice-body {
      padding: 0 16px 14px;
      font-size: 13px;
      line-height: 1.55;
      color: var(--muted);
    }

    .notice-body code {
      font: 12px var(--mono);
      color: var(--amber);
    }

    .noscript-warn {
      padding: 12px;
      background: #451a1a;
      color: #ffb4b4;
      text-align: center;
      font-weight: 600;
      border-bottom: 1px solid #782a2a;
    }

    @keyframes blink {

      0%,
      92%,
      100% {
        opacity: 1;
      }

      96% {
        opacity: .25;
      }
    }

    @media(max-width:640px) {
      .wrap {
        padding: 16px;
      }

      .bezel {
        width: 52px;
        height: 40px;
      }

      h1.brand {
        font-size: 15px;
      }

      .nav {
        width: 100%;
        display: grid;
        grid-template-columns: repeat(3, minmax(0, 1fr));
      }

      .nb {
        padding: 8px 4px;
        font-size: 12px;
        justify-content: center;
      }
    }

    @media(prefers-reduced-motion:reduce) {

      *,
      *::before,
      *::after {
        animation: none !important;
        transition: none !important;
      }
    }
  </style>
</head>

<body>
  <noscript>
    <div class="noscript-warn">JavaScript is required to perform OTA firmware updates.</div>
  </noscript>

  <div class="wrap">
    <header class="bridge">
      <div class="id">
        <div class="bezel" aria-hidden="true"><i></i><i></i></div>
        <div>
          <div class="unit">Kiwi Unit · <span id="fwVer">v1.0</span> · Firmware</div>
          <h1 class="brand">Over-The-Air Update</h1>
        </div>
      </div>
      <nav class="nav" aria-label="Main Pages">
        <a class="nb" href="/">Dashboard</a>
        <a class="nb" href="/faces">Faces</a>
        <a class="nb on" href="/update" aria-current="page">OTA</a>
      </nav>
    </header>

    <main class="container">
      <section class="hull" aria-labelledby="otaTitle">
        <div class="fw-meta-pill" id="fwPill">
          <span>Current Firmware:</span>
          <strong id="fwVersion">v1.0</strong>
        </div>

        <div class="glyph" aria-hidden="true">
          <svg width="26" height="26" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2"
            stroke-linecap="square">
            <path d="M21 15v4a2 2 0 0 1-2 2H5a2 2 0 0 1-2-2v-4" />
            <polyline points="17 8 12 3 7 8" />
            <line x1="12" y1="3" x2="12" y2="15" />
          </svg>
        </div>
        <h2 id="otaTitle">Flash Firmware Wirelessly</h2>
        <p class="sub">Check for official releases on GitHub or upload a compiled <code>MyRobot_Modular.ino.bin</code> locally.</p>

        <!-- ======================================================= -->
        <!-- ONLINE GITHUB RELEASE UPDATE SECTION                   -->
        <!-- ======================================================= -->
        <div class="gh-card" id="ghCard">
          <div class="gh-header">
            <div class="gh-title-group">
              <svg width="18" height="18" viewBox="0 0 24 24" fill="currentColor" aria-hidden="true">
                <path d="M12 0C5.37 0 0 5.37 0 12c0 5.31 3.435 9.795 8.205 11.385.6.105.825-.255.825-.57 0-.285-.015-1.23-.015-2.235-3.015.555-3.795-.735-4.035-1.41-.135-.345-.72-1.41-1.23-1.695-.42-.225-1.02-.78-.015-.795.945-.015 1.62.87 1.845 1.23 1.08 1.815 2.805 1.305 3.495.99.105-.78.42-1.305.765-1.605-2.67-.3-5.46-1.335-5.46-5.925 0-1.305.465-2.385 1.23-3.225-.12-.3-.54-1.53.12-3.18 0 0 1.005-.315 3.3 1.23.96-.27 1.98-.405 3-.405s2.04.135 3 .405c2.295-1.56 3.3-1.23 3.3-1.23.66 1.65.24 2.88.12 3.18.765.84 1.23 1.905 1.23 3.225 0 4.605-2.805 5.625-5.475 5.925.435.375.81 1.095.81 2.22 0 1.605-.015 2.895-.015 3.3 0 .315.225.69.825.57A12.02 12.02 0 0024 12c0-6.63-5.37-12-12-12z"/>
              </svg>
              <span>GitHub Online Update</span>
            </div>
            <button type="button" class="gh-repo-chip" id="btnEditRepo" title="Click to customize repository">
              <span id="repoNameDisplay">krishh-9085/kiwi</span>
              <svg width="12" height="12" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2.5"><path d="M12 20h9"/><path d="M16.5 3.5a2.121 2.121 0 0 1 3 3L7 19l-4 1 1-4L16.5 3.5z"/></svg>
            </button>
          </div>

          <!-- Optional Inline Repo Edit Row -->
          <div id="repoEditRow" class="repo-edit-row" style="display:none;">
            <input type="text" id="repoInput" class="repo-input" placeholder="owner/repo (e.g. krishh-9085/kiwi)" value="krishh-9085/kiwi" spellcheck="false">
            <button type="button" id="btnSaveRepo" class="btn-sm">Save</button>
            <button type="button" id="btnCancelRepo" class="btn-sm btn-ghost">Cancel</button>
          </div>

          <!-- PRIMARY CHECK BUTTON -->
          <button type="button" id="btnCheckUpdate" class="btn-check-ota">
            <svg id="btnCheckIcon" width="16" height="16" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2.2" stroke-linecap="round" stroke-linejoin="round">
              <path d="M21.5 2v6h-6M21.34 15.57a10 10 0 1 1-.57-8.38l5.67-5.67"/>
            </svg>
            <span id="btnCheckText">Update available?</span>
          </button>

          <!-- Result Presentation Box -->
          <div id="ghResult" class="gh-result-box" style="display:none;" aria-live="polite"></div>

          <!-- Live Online OTA Download & Flash Progress -->
          <div id="onlineOtaCard" class="online-progress-card" style="display:none;" role="region" aria-label="Wireless firmware update progress">
            <div class="online-progress-header">
              <span id="onlineOtaStatusText">Contacting Kiwi...</span>
              <span id="onlineOtaPercent">0%</span>
            </div>
            <div class="online-bar-track">
              <div id="onlineOtaBar" class="online-bar-fill"></div>
            </div>
            <div id="onlineOtaSub" class="online-progress-sub">Downloading firmware binary from GitHub directly into ESP32 flash...</div>
          </div>
        </div>

        <div class="section-divider">
          <span>OR FLASH LOCAL BINARY</span>
        </div>

        <div class="warning-banner" role="alert">
          <svg width="18" height="18" viewBox="0 0 24 24" fill="none" stroke-width="2" aria-hidden="true">
            <path d="M10.29 3.86L1.82 18a2 2 0 0 0 1.71 3h16.94a2 2 0 0 0 1.71-3L13.71 3.86a2 2 0 0 0-3.42 0z" />
            <line x1="12" y1="9" x2="12" y2="13" />
            <line x1="12" y1="17" x2="12.01" y2="17" />
          </svg>
          <span><b>Crucial:</b> Do not power off Kiwi or disconnect Wi-Fi while firmware write is active.</span>
        </div>

        <form method="POST" action="/update" enctype="multipart/form-data" id="otaForm">
          <div class="file-picker" id="dropZone" tabindex="0" role="region" aria-label="Firmware file drop zone">
            <input type="file" name="update" accept=".bin" required id="fileIn">
            <label class="file-label" for="fileIn" id="fileLabel" tabindex="0">Choose Firmware Binary</label>
            <span class="file-name" id="fileName" aria-live="polite">Drag &amp; drop .bin file here or click to
              browse</span>
          </div>
          <div class="file-help">Accepts compiled .bin binary files (maximum 1.9 MB).</div>
          <button type="submit" id="btn" class="btn">Update Firmware</button>
        </form>

        <div id="prg" role="progressbar" aria-label="Upload progress" aria-valuemin="0" aria-valuemax="100"
          aria-valuenow="0">
          <div id="bar"></div>
        </div>
        <div id="stat" role="status" aria-live="polite"></div>

        <details class="notice-details">
          <summary>Partition Scheme &amp; Troubleshooting Guide</summary>
          <div class="notice-body">
            For OTA updates to succeed, the ESP32 partition table must have dual OTA app partitions enabled (e.g.
            <b>Minimal SPIFFS / Large APPS with OTA</b> or Kiwi dual 1.9MB custom partitions).
            <br><br>
            If your board was previously running a single <code>huge_app</code> scheme without OTA slots, please flash
            once via USB cable in the Arduino IDE before using wireless updates.
          </div>
        </details>
      </section>
    </main>
  </div>

  <script>
    (function () {
      'use strict';

      const form = document.getElementById('otaForm');
      const fileInput = document.getElementById('fileIn');
      const fileLabel = document.getElementById('fileLabel');
      const fileName = document.getElementById('fileName');
      const dropZone = document.getElementById('dropZone');
      const stat = document.getElementById('stat');
      const btn = document.getElementById('btn');
      const prg = document.getElementById('prg');
      const bar = document.getElementById('bar');
      const fwVersion = document.getElementById('fwVersion');
      const fwVer = document.getElementById('fwVer');

      // GitHub Update Elements
      const btnCheckUpdate = document.getElementById('btnCheckUpdate');
      const btnCheckIcon = document.getElementById('btnCheckIcon');
      const btnCheckText = document.getElementById('btnCheckText');
      const ghResult = document.getElementById('ghResult');
      const btnEditRepo = document.getElementById('btnEditRepo');
      const repoEditRow = document.getElementById('repoEditRow');
      const repoInput = document.getElementById('repoInput');
      const repoNameDisplay = document.getElementById('repoNameDisplay');
      const btnSaveRepo = document.getElementById('btnSaveRepo');
      const btnCancelRepo = document.getElementById('btnCancelRepo');
      const onlineOtaCard = document.getElementById('onlineOtaCard');
      const onlineOtaStatusText = document.getElementById('onlineOtaStatusText');
      const onlineOtaPercent = document.getElementById('onlineOtaPercent');
      const onlineOtaBar = document.getElementById('onlineOtaBar');
      const onlineOtaSub = document.getElementById('onlineOtaSub');

      let currentFirmwareVersion = 'v1.0';
      const DEFAULT_REPO = 'krishh-9085/kiwi';
      let activeRepo = localStorage.getItem('kiwi_ota_repo') || DEFAULT_REPO;
      repoNameDisplay.textContent = activeRepo;
      repoInput.value = activeRepo;

      // Fetch version dynamically from Kiwi status endpoint
      fetch('/status', { cache: 'no-store' })
        .then(function (r) { return r.json(); })
        .then(function (j) {
          if (j && j.version) {
            currentFirmwareVersion = j.version;
            if (fwVersion) fwVersion.textContent = j.version;
            if (fwVer) fwVer.textContent = j.version;
          }
        })
        .catch(function () { });

      // Repo editing
      btnEditRepo.addEventListener('click', function () {
        repoEditRow.style.display = (repoEditRow.style.display === 'none') ? 'flex' : 'none';
        if (repoEditRow.style.display === 'flex') repoInput.focus();
      });

      btnCancelRepo.addEventListener('click', function () {
        repoEditRow.style.display = 'none';
        repoInput.value = activeRepo;
      });

      btnSaveRepo.addEventListener('click', function () {
        const val = repoInput.value.trim();
        if (val && val.includes('/')) {
          activeRepo = val;
          localStorage.setItem('kiwi_ota_repo', activeRepo);
          repoNameDisplay.textContent = activeRepo;
          repoEditRow.style.display = 'none';
          checkGitHubUpdate();
        } else {
          alert('Please enter a valid GitHub repository in "owner/repo" format (e.g. krishh-9085/kiwi).');
        }
      });

      // Semantic version comparator
      function parseVersion(v) {
        if (!v) return [0, 0, 0];
        const clean = String(v).replace(/^v/i, '').trim();
        const parts = clean.split(/[-.+]/).map(function (n) { return parseInt(n, 10); }).filter(function (n) { return !isNaN(n); });
        while (parts.length < 3) parts.push(0);
        return parts;
      }

      function isVersionNewer(latest, current) {
        const l = parseVersion(latest);
        const c = parseVersion(current);
        for (let i = 0; i < Math.max(l.length, c.length); i++) {
          const li = l[i] || 0;
          const ci = c[i] || 0;
          if (li > ci) return true;
          if (li < ci) return false;
        }
        return false;
      }

      function formatBytes(bytes) {
        if (!bytes || bytes <= 0) return '0 B';
        const k = 1024;
        const dm = 1;
        const sizes = ['B', 'KB', 'MB'];
        const i = Math.floor(Math.log(bytes) / Math.log(k));
        return parseFloat((bytes / Math.pow(k, i)).toFixed(dm)) + ' ' + sizes[i];
      }

      // ----------------------------------------------------
      // GITHUB RELEASE CHECK LOGIC
      // ----------------------------------------------------
      function checkGitHubUpdate() {
        btnCheckUpdate.disabled = true;
        btnCheckIcon.classList.add('spinning');
        btnCheckText.textContent = 'Checking GitHub Releases...';
        ghResult.style.display = 'block';
        ghResult.className = 'gh-result-box';
        ghResult.innerHTML = '<span style="color:var(--amber);">Querying GitHub releases for <code>' + escapeHtml(activeRepo) + '</code>...</span>';

        const apiUrl = 'https://api.github.com/repos/' + activeRepo + '/releases/latest';

        fetch(apiUrl, {
          headers: { 'Accept': 'application/vnd.github.v3+json' },
          cache: 'no-store'
        })
          .then(function (res) {
            if (res.status === 404) {
              // No "latest" release tag published yet. Check all releases list or fallback
              return fetch('https://api.github.com/repos/' + activeRepo + '/releases', { cache: 'no-store' })
                .then(function (r) { return r.json(); })
                .then(function (releases) {
                  if (Array.isArray(releases) && releases.length > 0) {
                    return releases[0];
                  }
                  return { notFound: true };
                });
            }
            if (!res.ok) {
              throw new Error('GitHub API response ' + res.status + ': ' + res.statusText);
            }
            return res.json();
          })
          .then(function (release) {
            btnCheckUpdate.disabled = false;
            btnCheckIcon.classList.remove('spinning');
            btnCheckText.textContent = 'Check for Updates';

            if (release.notFound) {
              // Check latest commit on main for additional visibility
              fetch('https://api.github.com/repos/' + activeRepo + '/commits/main', { cache: 'no-store' })
                .then(function (cr) { return cr.json(); })
                .then(function (commitData) {
                  const sha = (commitData && commitData.sha) ? commitData.sha.substring(0, 7) : '';
                  const msg = (commitData && commitData.commit && commitData.commit.message) ? commitData.commit.message.split('\n')[0] : '';
                  ghResult.className = 'gh-result-box uptodate';
                  ghResult.innerHTML =
                    '<div><b>✓ Firmware is up to date</b></div>' +
                    '<div style="margin-top:6px;color:var(--ink);font-size:12px;">You are running the current firmware version (<strong>' + escapeHtml(currentFirmwareVersion) + '</strong>).</div>' +
                    '<div style="margin-top:6px;color:var(--muted);font-size:12px;">Repository <code>' + escapeHtml(activeRepo) + '</code> has no newer tagged release.' +
                    (sha ? '<br>Latest main commit: <code>' + escapeHtml(sha) + '</code> — <i>' + escapeHtml(msg) + '</i>' : '') +
                    '</div>';
                })
                .catch(function () {
                  ghResult.className = 'gh-result-box uptodate';
                  ghResult.innerHTML =
                    '<div><b>✓ Firmware is up to date</b></div>' +
                    '<div style="margin-top:6px;color:var(--ink);font-size:12px;">You are running the current firmware version (<strong>' + escapeHtml(currentFirmwareVersion) + '</strong>).</div>' +
                    '<div style="margin-top:4px;color:var(--muted);font-size:12px;">No newer releases found on <code>' + escapeHtml(activeRepo) + '</code>.</div>';
                });
              return;
            }

            const latestTag = release.tag_name || release.name || '';
            const isNewer = isVersionNewer(latestTag, currentFirmwareVersion);

            // Find .bin asset in release
            let binAsset = null;
            if (Array.isArray(release.assets)) {
              for (let i = 0; i < release.assets.length; i++) {
                const a = release.assets[i];
                if (a.name && a.name.toLowerCase().endsWith('.bin')) {
                  binAsset = a;
                  break;
                }
              }
            }

            if (isNewer) {
              ghResult.className = 'gh-result-box update-avail';
              let html =
                '<div class="update-badge">⚡ New Update Available</div>' +
                '<h3 class="release-title">' + escapeHtml(release.name || latestTag) + '</h3>' +
                '<div class="update-meta-grid">' +
                '  <div class="update-meta-item"><div class="meta-k">Current</div><div class="meta-v">' + escapeHtml(currentFirmwareVersion) + '</div></div>' +
                '  <div class="update-meta-item"><div class="meta-k">New Version</div><div class="meta-v">' + escapeHtml(latestTag) + '</div></div>' +
                '  <div class="update-meta-item"><div class="meta-k">Binary Size</div><div class="meta-v">' + (binAsset ? formatBytes(binAsset.size) : 'N/A') + '</div></div>' +
                '</div>';

              if (release.body && release.body.trim()) {
                html += '<div class="release-body">' + escapeHtml(release.body.trim()) + '</div>';
              }

              if (binAsset && binAsset.browser_download_url) {
                html +=
                  '<button type="button" id="btnInstallOta" class="btn-install-ota" data-url="' + escapeHtml(binAsset.browser_download_url) + '" data-ver="' + escapeHtml(latestTag) + '">' +
                  '  <svg width="18" height="18" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2.5"><path d="M21 15v4a2 2 0 0 1-2 2H5a2 2 0 0 1-2-2v-4"/><polyline points="7 10 12 15 17 10"/><line x1="12" y1="15" x2="12" y2="3"/></svg>' +
                  '  <span>Install ' + escapeHtml(latestTag) + ' Wirelessly</span>' +
                  '</button>' +
                  '<div style="text-align:center;"><a class="direct-dl-link" href="' + escapeHtml(binAsset.browser_download_url) + '" target="_blank" download>⬇ Download ' + escapeHtml(binAsset.name) + ' directly</a></div>';
              } else {
                html +=
                  '<div style="color:var(--amber);margin-top:10px;font-size:12px;">Notice: No pre-compiled <code>.bin</code> binary attached to this release. You can compile and flash manually.</div>';
              }

              ghResult.innerHTML = html;

              const installBtn = document.getElementById('btnInstallOta');
              if (installBtn) {
                installBtn.addEventListener('click', function () {
                  startWirelessOta(this.getAttribute('data-url'), this.getAttribute('data-ver'));
                });
              }
            } else {
              ghResult.className = 'gh-result-box uptodate';
              ghResult.innerHTML =
                '<div><b>✓ Kiwi is up to date</b></div>' +
                '<div style="margin-top:6px;color:var(--ink);font-size:12px;">You are already running the latest firmware (<strong>' + escapeHtml(currentFirmwareVersion) + '</strong>).</div>' +
                '<div style="margin-top:4px;color:var(--muted);font-size:12px;">Latest GitHub release on <code>' + escapeHtml(activeRepo) + '</code> is <b>' + escapeHtml(latestTag) + '</b>.</div>';
            }
          })
          .catch(function (err) {
            btnCheckUpdate.disabled = false;
            btnCheckIcon.classList.remove('spinning');
            btnCheckText.textContent = 'Check for Updates';
            ghResult.className = 'gh-result-box error';
            ghResult.innerHTML =
              '<div><b>✗ Unable to check GitHub releases</b></div>' +
              '<div style="margin-top:6px;font-size:12px;">' + escapeHtml(err.message || String(err)) + '</div>' +
              '<div style="margin-top:4px;color:var(--muted);font-size:11px;">Make sure your device has internet access or verify repository name <code>' + escapeHtml(activeRepo) + '</code>.</div>';
          });
      }

      btnCheckUpdate.addEventListener('click', checkGitHubUpdate);

      // Auto-trigger check if ?check=1 in URL (e.g. from Dashboard click)
      if (window.location.search.indexOf('check=1') !== -1) {
        checkGitHubUpdate();
      }

      // ----------------------------------------------------
      // AUTOMATED WIRELESS OTA INSTALLATION VIA ESP32
      // ----------------------------------------------------
      function startWirelessOta(downloadUrl, versionTag) {
        if (!confirm('Flash Kiwi with firmware ' + versionTag + ' wirelessly?\n\nThe robot will download the update directly from GitHub and reboot into the new firmware. Keep Kiwi powered on.')) {
          return;
        }

        ghResult.style.display = 'none';
        onlineOtaCard.style.display = 'block';
        btnCheckUpdate.disabled = true;
        btn.disabled = true;
        onlineOtaStatusText.textContent = 'Initiating OTA on Kiwi...';
        onlineOtaPercent.textContent = '0%';
        onlineOtaBar.style.width = '0%';
        onlineOtaSub.textContent = 'Contacting Kiwi to download binary from GitHub...';

        fetch('/ota/install', {
          method: 'POST',
          headers: { 'Content-Type': 'application/json' },
          body: JSON.stringify({ url: downloadUrl })
        })
          .then(function (r) { return r.json(); })
          .then(function (res) {
            if (res && res.ok) {
              pollOnlineOtaProgress(versionTag);
            } else {
              throw new Error((res && res.error) ? res.error : 'Failed to start OTA update');
            }
          })
          .catch(function (err) {
            onlineOtaCard.style.display = 'none';
            ghResult.style.display = 'block';
            ghResult.className = 'gh-result-box error';
            ghResult.innerHTML = '<div><b>✗ OTA initiation failed:</b> ' + escapeHtml(err.message) + '</div>';
            btnCheckUpdate.disabled = false;
            btn.disabled = false;
          });
      }

      function pollOnlineOtaProgress(versionTag) {
        let pollInterval = setInterval(function () {
          fetch('/ota/status', { cache: 'no-store' })
            .then(function (r) { return r.json(); })
            .then(function (data) {
              if (!data) return;

              if (data.state === 'downloading' || data.state === 'starting' || data.state === 'flashing') {
                const pct = Math.max(0, Math.min(100, data.progress || 0));
                onlineOtaBar.style.width = pct + '%';
                onlineOtaPercent.textContent = pct + '%';
                if (data.bytes_total > 0) {
                  onlineOtaStatusText.textContent = 'Downloading firmware: ' + pct + '%';
                  onlineOtaSub.textContent = 'Received ' + formatBytes(data.bytes_read) + ' of ' + formatBytes(data.bytes_total) + ' directly into flash memory...';
                } else if (data.bytes_read > 0) {
                  onlineOtaStatusText.textContent = 'Streaming firmware...';
                  onlineOtaSub.textContent = 'Received ' + formatBytes(data.bytes_read) + ' into flash memory...';
                } else {
                  onlineOtaStatusText.textContent = 'Connecting to GitHub...';
                }
              } else if (data.state === 'success') {
                clearInterval(pollInterval);
                onlineOtaBar.style.width = '100%';
                onlineOtaPercent.textContent = '100%';
                onlineOtaStatusText.style.color = 'var(--ok)';
                onlineOtaStatusText.textContent = '✓ Firmware flashed successfully!';
                onlineOtaSub.textContent = 'Kiwi is rebooting into ' + versionTag + '. Polling device...';
                setTimeout(pollForReboot, 4000);
              } else if (data.state === 'error') {
                clearInterval(pollInterval);
                onlineOtaCard.style.display = 'none';
                ghResult.style.display = 'block';
                ghResult.className = 'gh-result-box error';
                ghResult.innerHTML =
                  '<div><b>✗ Online Update Failed</b></div>' +
                  '<div style="margin-top:6px;font-size:12px;">' + escapeHtml(data.error || 'Unknown flash error') + '</div>' +
                  '<div style="margin-top:8px;"><button type="button" class="btn-sm" onclick="location.reload()">Retry</button></div>';
                btnCheckUpdate.disabled = false;
                btn.disabled = false;
              }
            })
            .catch(function () {
              // Network blip while writing flash or rebooting; continue polling
            });
        }, 600);
      }

      function escapeHtml(s) {
        if (!s) return '';
        return String(s)
          .replace(/&/g, '&amp;')
          .replace(/</g, '&lt;')
          .replace(/>/g, '&gt;')
          .replace(/"/g, '&quot;')
          .replace(/'/g, '&#039;');
      }

      // ----------------------------------------------------
      // LOCAL FILE UPLOAD LOGIC
      // ----------------------------------------------------
      function handleFileSelected(file) {
        if (!file) {
          fileName.textContent = 'Drag & drop .bin file here or click to browse';
          fileName.classList.remove('set');
          return;
        }

        if (!file.name.toLowerCase().endsWith('.bin')) {
          stat.style.color = 'var(--astro)';
          stat.textContent = 'Invalid file type. Please select a compiled .bin file.';
          fileInput.value = '';
          fileName.textContent = 'Invalid file (must end in .bin)';
          fileName.classList.remove('set');
          return;
        }

        // Limit check: 1.9MB max for Kiwi dual OTA app partition
        if (file.size > 1966080) {
          stat.style.color = 'var(--astro)';
          stat.textContent = 'File too large (' + (file.size / 1048576).toFixed(2) + ' MB). Maximum size is 1.9 MB.';
          fileInput.value = '';
          fileName.textContent = 'File exceeds 1.9 MB limit';
          fileName.classList.remove('set');
          return;
        }

        stat.textContent = '';
        fileName.textContent = file.name + ' (' + (file.size / 1024).toFixed(0) + ' KB)';
        fileName.classList.add('set');
      }

      fileInput.addEventListener('change', function () {
        if (this.files && this.files.length > 0) {
          handleFileSelected(this.files[0]);
        }
      });

      function triggerPicker(e) {
        if (e.key === 'Enter' || e.key === ' ') {
          e.preventDefault();
          fileInput.click();
        }
      }
      fileLabel.addEventListener('keydown', triggerPicker);
      dropZone.addEventListener('keydown', triggerPicker);

      ['dragenter', 'dragover'].forEach(function (eventName) {
        dropZone.addEventListener(eventName, function (e) {
          e.preventDefault();
          e.stopPropagation();
          dropZone.classList.add('dragover');
        });
      });

      ['dragleave', 'drop'].forEach(function (eventName) {
        dropZone.addEventListener(eventName, function (e) {
          e.preventDefault();
          e.stopPropagation();
          dropZone.classList.remove('dragover');
        });
      });

      dropZone.addEventListener('drop', function (e) {
        const dt = e.dataTransfer;
        if (dt && dt.files && dt.files.length > 0) {
          fileInput.files = dt.files;
          handleFileSelected(dt.files[0]);
        }
      });

      function pollForReboot() {
        stat.style.color = 'var(--amber)';
        stat.textContent = 'Verifying and rebooting Kiwi... Polling system status...';
        let attempts = 0;
        const maxAttempts = 30;

        const interval = setInterval(function () {
          attempts++;
          fetch('/status', { cache: 'no-store' })
            .then(function (r) {
              if (r.ok) {
                clearInterval(interval);
                stat.style.color = 'var(--ok)';
                stat.textContent = '✓ Kiwi rebooted successfully! Redirecting to Dashboard...';
                setTimeout(function () { window.location.href = '/'; }, 1200);
              }
            })
            .catch(function () {
              if (attempts >= maxAttempts) {
                clearInterval(interval);
                stat.style.color = 'var(--amber)';
                stat.textContent = 'Device is taking longer than expected to reboot. Please refresh manually.';
              }
            });
        }, 2500);
      }

      form.onsubmit = function (e) {
        e.preventDefault();
        const file = fileInput.files[0];
        if (!file) return;

        if (!file.name.toLowerCase().endsWith('.bin')) {
          stat.style.color = 'var(--astro)';
          stat.textContent = 'Only .bin files are accepted.';
          return;
        }

        const xhr = new XMLHttpRequest();
        const fd = new FormData();
        fd.append('update', file);

        prg.style.display = 'block';
        prg.setAttribute('aria-valuenow', '0');
        btn.disabled = true;
        stat.style.color = 'var(--amber)';
        stat.textContent = 'Uploading firmware binary to ESP32 flash...';

        xhr.upload.onprogress = function (pe) {
          if (pe.lengthComputable) {
            const pct = Math.round((pe.loaded / pe.total) * 100);
            bar.style.width = pct + '%';
            prg.setAttribute('aria-valuenow', String(pct));
            if (pct < 100) {
              stat.textContent = 'Uploading firmware: ' + pct + '%';
            } else {
              stat.textContent = 'Upload complete (100%). Writing to flash partition & verifying checksum...';
            }
          }
        };

        xhr.onload = function () {
          const resp = (xhr.responseText || '').trim();
          const isSuccess = (xhr.status === 200 && (resp === 'OK' || resp.indexOf('OK') !== -1 || resp === ''));
          if (isSuccess) {
            stat.style.color = 'var(--ok)';
            bar.style.width = '100%';
            prg.setAttribute('aria-valuenow', '100');
            stat.textContent = '✓ Flash write verified! Kiwi is rebooting into the new firmware...';
            setTimeout(pollForReboot, 4000);
          } else {
            stat.style.color = 'var(--astro)';
            stat.textContent = '✗ ' + (resp || 'Flash Update Failed (Code: ' + xhr.status + ')');
            btn.disabled = false;
          }
        };

        xhr.onerror = function () {
          stat.style.color = 'var(--astro)';
          stat.textContent = '✗ Connection lost during firmware upload. Check Wi-Fi connection.';
          btn.disabled = false;
        };

        xhr.open('POST', '/update');
        xhr.send(fd);
      };
    })();
  </script>
</body>

</html>
)rawliteral";
  server.send_P(200, "text/html; charset=utf-8", OTA_HTML);
}
