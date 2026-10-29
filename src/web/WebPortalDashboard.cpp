#include "web/WebPortal.h"
#include "config/AppConfig.h"
#include "config/ConfigManager.h"
#include "hardware/AudioEngine.h"
#include "face/FaceEngine.h"
#include "services/AiService.h"
#include "services/MemoryService.h"
#include "animation/AnimationEngine.h"
#include <WiFi.h>
#include <WebServer.h>
#include <DNSServer.h>
#include <Update.h>
#include <time.h>
#include <Preferences.h>
#include "services/WeatherService.h"

// HTML ESCAPE & TEMPLATES
// ============================================================
static String esc(String s) {
  s.replace("&", "&amp;");
  s.replace("<", "&lt;");
  s.replace(">", "&gt;");
  s.replace("\"", "&quot;");
  return s;
}

static String escJson(String s) {
  s.replace("\\", "\\\\");
  s.replace("\"", "\\\"");
  s.replace("\r", "");
  s.replace("\n", "\\n");
  return s;
}

// --- Chunked PROGMEM Dashboard Segments (Zero-Heap Streamed) ---
static const char DASHBOARD_CSS[] PROGMEM = R"rawcss(

<!doctype html>
<html lang="en">

<head>
  <meta charset="utf-8">
  <meta name="viewport" content="width=device-width,initial-scale=1,viewport-fit=cover">
  <title>Kiwi Robot · System Console</title>
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
      --muted: #8a94a6;
      --astro: #ff4a3d;
      --astro-glow: rgba(255, 74, 61, .35);
      --amber: #f2b45c;
      --amber-dim: rgba(242, 180, 92, .16);
      --amber-glow: rgba(242, 180, 92, .38);
      --ok: #8fd19e;
      --glass: rgba(13, 18, 27, .6);
      --glass-edge: rgba(70, 84, 108, .55);
      --display: "Avenir Next", "Helvetica Neue", sans-serif;
      --body: "Helvetica Neue", Arial, sans-serif;
      --mono: "SF Mono", Menlo, monospace;
      --cut: 14px;
      --cut-sm: 7px;
    }

    * {
      box-sizing: border-box
    }

    html {
      scroll-behavior: smooth
    }

    body {
      margin: 0;
      font: 14px/1.5 var(--body);
      color: var(--ink);
      background: var(--void);
      min-height: 100vh;
      overflow-x: hidden
    }

    body::before {
      content: "";
      position: fixed;
      inset: 0;
      z-index: -1;
      pointer-events: none;
      background-image: linear-gradient(rgba(242, 180, 92, .035) 1px, transparent 1px), linear-gradient(90deg, rgba(242, 180, 92, .035) 1px, transparent 1px);
      background-size: 48px 48px;
      -webkit-mask-image: linear-gradient(#000, transparent 55%);
      mask-image: linear-gradient(#000, transparent 55%)
    }

    #sky {
      position: fixed;
      inset: 0;
      width: 100%;
      height: 100%;
      z-index: -2;
      pointer-events: none
    }

    .wrap {
      width: min(1260px, 100%);
      margin: 0 auto;
      padding: 24px
    }

    /* Bridge header */
    .bridge {
      display: flex;
      align-items: center;
      justify-content: space-between;
      gap: 16px;
      flex-wrap: wrap;
      padding: 8px 0 20px
    }

    .id {
      display: flex;
      align-items: center;
      gap: 14px
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
      overflow: hidden
    }

    .bezel::after {
      content: "";
      position: absolute;
      inset: 0;
      background: repeating-linear-gradient(0deg, rgba(255, 255, 255, .03) 0 1px, transparent 1px 3px);
      pointer-events: none
    }

    .bezel i {
      width: 9px;
      height: 12px;
      background: var(--amber);
      box-shadow: 0 0 8px var(--amber-glow);
      animation: blink 5s infinite
    }

    .unit {
      font: 600 10px/1 var(--display);
      letter-spacing: .22em;
      text-transform: uppercase;
      color: var(--muted)
    }

    h1 {
      margin: 4px 0 2px;
      font: 600 17px/1.1 var(--display);
      letter-spacing: .28em;
      text-transform: uppercase
    }

    .mood {
      font: 12px var(--mono);
      color: var(--amber)
    }

    .nav {
      display: flex;
      gap: 8px;
      flex-wrap: wrap
    }

    .nb,
    .qbtn,
    button.save-btn {
      font: 500 12px var(--body);
      color: var(--ink);
      text-decoration: none;
      cursor: pointer;
      background: var(--well);
      border: 1px solid var(--line-strong);
      padding: 8px 14px;
      clip-path: polygon(var(--cut-sm) 0, 100% 0, 100% calc(100% - var(--cut-sm)), calc(100% - var(--cut-sm)) 100%, 0 100%, 0 var(--cut-sm));
      transition: border-color .15s, background .15s, transform .1s;
      display: inline-flex;
      align-items: center;
      justify-content: center
    }

    .nb:hover,
    .qbtn:hover {
      border-color: rgba(242, 180, 92, .6);
      background: rgba(242, 180, 92, .06)
    }

    .nb:active,
    .qbtn:active,
    button.save-btn:active {
      transform: scale(.97)
    }

    .nb.on,
    .qbtn.primary,
    button.save-btn {
      background: var(--amber);
      border-color: var(--amber);
      color: #14100a;
      font-weight: 600
    }

    .nb.on:hover,
    .qbtn.primary:hover,
    button.save-btn:hover {
      background: #ffc87a;
      border-color: #ffc87a
    }

    .qbtn:disabled {
      opacity: .5;
      cursor: default
    }

    /* Segmented pill tabs */
    .tabs {
      display: inline-flex;
      gap: 3px;
      padding: 4px;
      border-radius: 999px;
      background: rgba(9, 12, 19, .75);
      border: 1px solid var(--line-strong);
      margin-bottom: 20px;
      box-shadow: inset 0 1px 3px rgba(0, 0, 0, .4);
    }

    .tab-btn {
      font: 600 12px var(--display);
      letter-spacing: .16em;
      text-transform: uppercase;
      color: var(--muted);
      background: transparent;
      border: none;
      padding: 7px 22px;
      border-radius: 999px;
      cursor: pointer;
      user-select: none;
      transition: background .15s, color .15s, box-shadow .15s;
    }

    .tab-btn:hover {
      color: var(--ink);
    }

    .tab-btn.on {
      background: var(--amber);
      color: #14100a;
      box-shadow: 0 0 12px var(--amber-glow);
    }

    .live-hardware-grid {
      display: grid;
      grid-template-columns: repeat(2, minmax(0, 1fr));
      gap: 16px;
      margin-top: 16px;
    }
    @media (max-width: 900px) {
      .live-hardware-grid {
        grid-template-columns: 1fr;
      }
    }

    /* Live auto-save feedback tick */
    .live-saved-tick {
      display: inline-flex;
      align-items: center;
      gap: 5px;
      font: 600 11px var(--mono);
      color: #4ade80;
      background: rgba(74, 222, 128, 0.12);
      border: 1px solid rgba(74, 222, 128, 0.35);
      border-radius: 999px;
      padding: 2px 8px;
      opacity: 0;
      transform: translateY(-2px) scale(0.92);
      transition: opacity .25s ease, transform .25s ease;
      pointer-events: none;
      box-shadow: 0 0 10px rgba(74, 222, 128, 0.25);
    }

    .live-saved-tick.show {
      opacity: 1;
      transform: translateY(0) scale(1);
    }

    /* Status strip */
    .strip {
      display: grid;
      grid-template-columns: repeat(5, minmax(0, 1fr));
      margin-bottom: 20px;
      background: var(--glass);
      border: 1px solid var(--glass-edge);
      backdrop-filter: blur(18px);
      clip-path: polygon(var(--cut) 0, 100% 0, 100% calc(100% - var(--cut)), calc(100% - var(--cut)) 100%, 0 100%, 0 var(--cut))
    }

    .cell {
      padding: 12px 16px;
      border-left: 1px solid var(--line);
      min-width: 0;
      position: relative
    }

    .cell:first-child {
      border-left: 0
    }

    .k {
      font: 600 10px var(--display);
      letter-spacing: .18em;
      text-transform: uppercase;
      color: var(--muted);
      margin-bottom: 4px;
      display: flex;
      align-items: center;
      gap: 6px
    }

    .k::before {
      content: "";
      width: 5px;
      height: 5px;
      border-radius: 50%;
      background: var(--amber);
      box-shadow: 0 0 6px var(--amber-glow);
      animation: blink 3s infinite
    }

    .v {
      font: 12px var(--mono);
      color: var(--ink);
      overflow: hidden;
      text-overflow: ellipsis;
      white-space: nowrap
    }

    /* Hull cards */
    .card {
      position: relative;
      background: linear-gradient(180deg, rgba(242, 180, 92, .06), transparent 30%), var(--glass);
      border: 1px solid var(--glass-edge);
      padding: 20px;
      min-width: 0;
      backdrop-filter: blur(18px);
      -webkit-backdrop-filter: blur(18px);
      clip-path: polygon(var(--cut) 0, 100% 0, 100% calc(100% - var(--cut)), calc(100% - var(--cut)) 100%, 0 100%, 0 var(--cut))
    }

    .card::before {
      content: "";
      position: absolute;
      top: 0;
      left: var(--cut);
      width: 36px;
      height: 2px;
      background: var(--amber);
      box-shadow: 0 0 8px var(--amber-glow)
    }

    .card-title,
    .card-head {
      font: 600 11px var(--display);
      letter-spacing: .22em;
      text-transform: uppercase;
      color: var(--muted);
      border-bottom: 1px solid var(--line);
      padding-bottom: 10px;
      margin: 0 0 14px;
      display: flex;
      justify-content: space-between;
      align-items: center;
      gap: 10px;
      flex-wrap: wrap
    }

    .card-head {
      border: 0;
      padding: 0;
      margin: 0
    }

    .live-card {
      margin-bottom: 24px
    }

    .form-grid {
      columns: 2 460px;
      column-gap: 16px
    }

    .form-grid>.card {
      break-inside: avoid;
      margin-bottom: 16px
    }
    .credentials-card {
      column-span: all;
      break-inside: avoid;
      width: 100%;
      margin-bottom: 16px;
    }

    .credentials-card .card-title {
      font-size: 13px;
      font-weight: 700;
      letter-spacing: .12em;
    }

    .credentials-deck {
      display: grid;
      grid-template-columns: 1fr 1fr;
      column-gap: 24px;
      row-gap: 16px;
      margin-top: 14px;
    }

    .api-key-item {
      min-width: 0;
    }

    .api-key-item label {
      font: 700 10.5px var(--display);
      letter-spacing: .16em;
      text-transform: uppercase;
      color: var(--amber);
      margin: 0 0 6px;
    }

    .api-key-item small {
      color: var(--muted);
      font-size: 11px;
      margin-top: 5px;
      line-height: 1.4;
      letter-spacing: 0.02em;
    }

    .ai-pipeline-deck {
      column-span: all;
      break-inside: avoid;
      width: 100%;
      display: grid;
      grid-template-columns: repeat(3, minmax(0, 1fr));
      gap: 16px;
      margin-bottom: 16px;
    }

    .ai-pipeline-deck>.card {
      margin-bottom: 0;
      break-inside: avoid;
    }

    @media (max-width: 1080px) {
      .ai-pipeline-deck {
        grid-template-columns: 1fr;
      }
    }

    @media (max-width: 720px) {
      .credentials-deck {
        grid-template-columns: 1fr;
      }
    }

    .save-wrap {
      column-span: all;
      break-inside: avoid;
      margin-top: 4px
    }

    h3 {
      font: 600 10px var(--display);
      letter-spacing: .2em;
      text-transform: uppercase;
      color: var(--amber);
      margin: 18px 0 4px
    }

    .muted,
    small {
      color: var(--muted);
      font-size: 12px;
      display: block;
      margin-top: 6px;
      line-height: 1.45
    }

    /* Form elements */
    label {
      display: block;
      font: 600 10px var(--display);
      letter-spacing: .18em;
      text-transform: uppercase;
      color: var(--muted);
      margin: 12px 0 5px
    }

    input:not([type=range]):not([type=checkbox]):not([type=radio]),
    select,
    textarea {
      width: 100%;
      background: var(--well);
      border: 1px solid var(--line-strong);
      color: var(--ink);
      padding: 9px 11px;
      border-radius: 3px;
      font: 13px var(--mono);
      outline: none;
      transition: border-color .15s, box-shadow .15s
    }

    select {
      appearance: none;
      -webkit-appearance: none;
      background-image: url("data:image/svg+xml,%3Csvg xmlns='http://www.w3.org/2000/svg' width='10' height='6'%3E%3Cpath d='M0 0l5 6 5-6z' fill='%23f2b45c'/%3E%3C/svg%3E");
      background-repeat: no-repeat;
      background-position: right 11px center;
      padding-right: 30px;
      cursor: pointer
    }

    select option {
      background: #0d121b;
      color: var(--ink)
    }

    input:focus,
    select:focus,
    textarea:focus {
      border-color: var(--amber);
      box-shadow: 0 0 0 3px rgba(242, 180, 92, .18)
    }

    textarea {
      min-height: 92px;
      resize: vertical;
      line-height: 1.5
    }

    input[type=password] {
      letter-spacing: .1em
    }

    .row-2 {
      display: grid;
      grid-template-columns: 1fr 1fr;
      gap: 12px
    }

    .pw {
      position: relative
    }

    .pw input {
      padding-right: 40px
    }

    .icon-btn {
      position: absolute;
      right: 6px;
      top: 50%;
      transform: translateY(-50%);
      background: none;
      border: 0;
      cursor: pointer;
      color: var(--muted);
      font: 600 10px var(--display);
      letter-spacing: .14em;
      text-transform: uppercase;
      padding: 4px
    }

    .icon-btn:hover {
      color: var(--amber)
    }

    /* Sliders */
    input[type=range] {
      -webkit-appearance: none;
      appearance: none;
      width: 100%;
      height: 4px;
      padding: 0;
      border: 0;
      border-radius: 4px;
      background: linear-gradient(90deg, var(--amber) var(--p, 50%), var(--line-strong) var(--p, 50%));
      outline: none;
      cursor: pointer;
      margin: 10px 0
    }

    input[type=range]::-webkit-slider-thumb {
      -webkit-appearance: none;
      width: 16px;
      height: 16px;
      border-radius: 50%;
      background: var(--void);
      border: 2px solid var(--amber);
      box-shadow: 0 0 10px var(--amber-glow)
    }

    input[type=range]::-moz-range-thumb {
      width: 14px;
      height: 14px;
      border-radius: 50%;
      background: var(--void);
      border: 2px solid var(--amber);
      box-shadow: 0 0 10px var(--amber-glow)
    }

    /* Toggles */
    .toggle {
      display: inline-flex;
      align-items: center;
      gap: 10px;
      cursor: pointer;
      user-select: none;
      font-size: 13px;
      color: var(--ink);
      margin: 0
    }

    .toggle input {
      position: absolute;
      opacity: 0;
      pointer-events: none
    }

    .switch {
      width: 36px;
      height: 18px;
      border-radius: 999px;
      background: var(--well);
      border: 1px solid var(--line-strong);
      position: relative;
      flex-shrink: 0;
      transition: background .2s, border-color .2s
    }

    .switch::after {
      content: "";
      position: absolute;
      top: 2px;
      left: 2px;
      width: 12px;
      height: 12px;
      border-radius: 50%;
      background: var(--muted);
      transition: transform .2s, background .2s, box-shadow .2s
    }

    .toggle input:checked+.switch {
      background: var(--amber-dim);
      border-color: var(--amber)
    }

    .toggle input:checked+.switch::after {
      transform: translateX(18px);
      background: var(--amber);
      box-shadow: 0 0 8px var(--amber-glow)
    }

    .toggle input:focus-visible+.switch {
      outline: 2px solid var(--amber);
      outline-offset: 2px
    }

    /* Panels & notes */
    .panel {
      margin-top: 18px;
      padding: 14px;
      background: var(--well);
      border: 1px solid var(--line);
      border-radius: 3px
    }

    .panel-title {
      font: 600 13px var(--body);
      color: var(--ink);
      margin-bottom: 8px
    }

    /* Buttons */
    button.save-btn {
      width: 100%;
      padding: 14px;
      font-size: 14px;
      letter-spacing: .2em;
      text-transform: uppercase;
      font-family: var(--display)
    }

    .status-pill {
      display: inline-flex;
      align-items: center;
      gap: 7px;
      padding: 5px 11px;
      background: var(--well);
      border: 1px solid var(--line-strong);
      font: 11px var(--mono);
      color: var(--ink);
      clip-path: polygon(6px 0, 100% 0, 100% calc(100% - 6px), calc(100% - 6px) 100%, 0 100%, 0 6px)
    }

    .dot {
      width: 7px;
      height: 7px;
      border-radius: 50%;
      background: var(--ok);
      box-shadow: 0 0 6px rgba(143, 209, 158, .6);
      animation: blink 3s infinite
    }

    .dot.busy {
      background: var(--amber);
      box-shadow: 0 0 6px var(--amber-glow)
    }

    .dot.rec {
      background: var(--astro);
      box-shadow: 0 0 8px var(--astro-glow);
      animation: pulse 1s ease-in-out infinite
    }

    .dot.sleep {
      background: var(--muted);
      box-shadow: none
    }

    @keyframes pulse {

      0%,
      100% {
        opacity: 1
      }

      50% {
        opacity: .3
      }
    }

    @keyframes blink {

      0%,
      92%,
      100% {
        opacity: 1
      }

      96% {
        opacity: .25
      }
    }

    @keyframes spin {
      to {
        transform: rotate(360deg)
      }
    }

    .spin {
      display: inline-block;
      animation: spin 1s linear infinite
    }

    /* Live: mic + readouts */
    .console-columns {
      display: grid;
      grid-template-columns: repeat(2, minmax(0, 1fr));
      gap: 16px
    }

    .stack {
      display: flex;
      flex-direction: column;
      gap: 12px
    }

    .mic-btn {
      display: flex;
      align-items: center;
      gap: 14px;
      width: 100%;
      padding: 14px 16px;
      cursor: pointer;
      color: var(--ink);
      font: inherit;
      text-align: left;
      background: var(--well);
      border: 1px solid var(--line-strong);
      transition: border-color .15s, box-shadow .15s, transform .1s
    }

    .mic-btn:hover {
      border-color: rgba(242, 180, 92, .6)
    }

    .mic-btn:active {
      transform: scale(.98)
    }

    .mic-icon {
      width: 14px;
      height: 14px;
      border-radius: 50%;
      background: var(--amber);
      box-shadow: 0 0 10px var(--amber-glow);
      flex-shrink: 0
    }

    .mic-title {
      font: 600 13px var(--display);
      letter-spacing: .14em;
      text-transform: uppercase
    }

    .mic-btn.recording {
      border-color: var(--astro);
      animation: pulseRec 1.5s ease-in-out infinite
    }

    .mic-btn.recording .mic-icon {
      background: var(--astro);
      box-shadow: 0 0 12px var(--astro-glow)
    }

    @keyframes pulseRec {

      0%,
      100% {
        box-shadow: 0 0 0 0 var(--astro-glow)
      }

      50% {
        box-shadow: 0 0 0 4px rgba(255, 74, 61, .08)
      }
    }

    .prompt-row {
      display: flex;
      gap: 8px
    }

    .prompt-row input {
      flex: 1
    }

    .readout {
      background: var(--well);
      border: 1px solid var(--line);
      padding: 12px 14px
    }

    .readout-k {
      font: 600 10px var(--display);
      letter-spacing: .2em;
      text-transform: uppercase;
      margin-bottom: 6px
    }

    .readout-k.blue {
      color: #8fb8e0
    }

    .readout-k.green {
      color: var(--ok)
    }

    .readout-v {
      font: 13px/1.5 var(--mono);
      color: var(--ink);
      min-height: 22px;
      word-break: break-word
    }

    /* Terminal */
    .term {
      display: flex;
      flex-direction: column;
      background: #030407;
      border: 1px solid var(--line-strong);
      padding: 12px;
      position: relative;
      overflow: hidden
    }

    .term::after {
      content: "";
      position: absolute;
      inset: 0;
      pointer-events: none;
      background: repeating-linear-gradient(0deg, rgba(255, 255, 255, .025) 0 1px, transparent 1px 3px)
    }

    .term-head {
      display: flex;
      align-items: center;
      justify-content: space-between;
      gap: 10px;
      flex-wrap: wrap;
      border-bottom: 1px solid var(--line);
      padding-bottom: 8px;
      margin-bottom: 8px;
      font: 11px var(--mono);
      color: var(--muted)
    }

    .term-head .lamps {
      display: flex;
      align-items: center;
      gap: 6px
    }

    .term-head .lamps i {
      width: 6px;
      height: 6px;
      border-radius: 50%;
      background: var(--amber);
      box-shadow: 0 0 6px var(--amber-glow)
    }

    .term-head label {
      display: inline-flex;
      align-items: center;
      gap: 6px;
      margin: 0;
      font: 11px var(--mono);
      letter-spacing: 0;
      text-transform: none;
      color: var(--muted);
      cursor: pointer
    }

    .term-head input[type=checkbox] {
      width: 12px;
      height: 12px;
      margin: 0;
      accent-color: var(--amber)
    }

    #serialTerminal {
      flex: 1;
      min-height: 210px;
      max-height: 250px;
      overflow-y: auto;
      background: #000;
      border: 1px solid var(--line);
      padding: 10px;
      font: 11.5px/1.45 var(--mono);
      color: #8fd19e;
      white-space: pre-wrap;
      word-break: break-all
    }

    .prompt-char {
      color: var(--amber)
    }

    /* Touch grid, bits */
    .grid-2 {
      display: grid;
      grid-template-columns: 1fr 1fr;
      gap: 12px
    }

    .tip {
      margin-top: 12px;
      display: block;
      color: var(--muted)
    }

    /* Rocky brightness card */
    .rocky-bright-card {
      background: linear-gradient(180deg, rgba(242, 180, 92, .08), transparent 35%), var(--glass)
    }

    .rocky-card-header {
      display: flex;
      align-items: center;
      justify-content: space-between;
      margin-bottom: 12px
    }

    .rocky-header-label {
      font: 600 11px var(--display);
      letter-spacing: .22em;
      text-transform: uppercase;
      color: var(--muted)
    }

    .rocky-slider-row {
      display: flex;
      align-items: center;
      gap: 14px;
      margin-bottom: 10px
    }

    .rocky-slider-row input {
      margin: 0
    }

    .rocky-percent-badge {
      font: 600 12px var(--mono);
      color: var(--amber);
      min-width: 44px;
      text-align: right;
      font-variant-numeric: tabular-nums
    }

    .rocky-section-title {
      font: 600 10px var(--display);
      letter-spacing: .22em;
      text-transform: uppercase;
      color: var(--muted);
      margin: 16px 0 8px
    }

    .rocky-pill-group {
      display: flex;
      flex-wrap: wrap;
      gap: 8px;
      margin-bottom: 14px
    }

    .rocky-pill {
      display: inline-flex;
      align-items: center;
      gap: 8px;
      padding: 7px 14px;
      background: var(--well);
      border: 1px solid var(--line-strong);
      color: var(--ink);
      font-size: 13px;
      cursor: pointer;
      user-select: none;
      margin: 0;
      text-transform: none;
      letter-spacing: 0;
      font-family: var(--body);
      font-weight: 500;
      transition: border-color .15s, background .15s
    }

    .rocky-pill:hover {
      border-color: rgba(242, 180, 92, .5)
    }

    .rocky-pill input {
      position: absolute;
      opacity: 0;
      pointer-events: none
    }

    .rocky-radio-dot {
      width: 13px;
      height: 13px;
      border-radius: 50%;
      border: 1px solid var(--muted);
      position: relative;
      flex-shrink: 0
    }

    .rocky-pill.active {
      border-color: var(--amber);
      background: var(--amber-dim);
      color: #fff
    }

    .rocky-pill.active .rocky-radio-dot {
      border-color: var(--amber)
    }

    .rocky-pill.active .rocky-radio-dot::after {
      content: "";
      position: absolute;
      inset: 2px;
      border-radius: 50%;
      background: var(--amber);
      box-shadow: 0 0 6px var(--amber-glow)
    }

    .rocky-time-row {
      display: flex;
      align-items: center;
      gap: 10px;
      flex-wrap: wrap;
      margin-bottom: 12px
    }

    .rocky-time-label {
      font: 12px var(--mono);
      color: var(--muted)
    }

    .rocky-time-picker {
      display: inline-flex;
      align-items: center;
      gap: 6px;
      background: var(--well);
      border: 1px solid var(--line-strong);
      padding: 4px 10px
    }

    .rocky-time-picker select {
      background-color: transparent;
      border: 0;
      padding: 2px 22px 2px 0;
      font: 600 13px var(--mono);
      color: var(--ink);
      background-position: right 0 center
    }

    .rocky-clock-icon {
      font-size: 12px;
      opacity: .5
    }

    .rocky-footnote {
      font-size: 12px;
      color: var(--muted);
      line-height: 1.5;
      margin: 14px 0 0;
      border-top: 1px solid var(--line);
      padding-top: 10px
    }

    .toast {
      position: fixed;
      left: 50%;
      bottom: 24px;
      transform: translate(-50%, 8px);
      opacity: 0;
      pointer-events: none;
      background: var(--panel);
      border: 1px solid var(--amber);
      padding: 8px 14px;
      font: 12px var(--mono);
      transition: opacity .3s, transform .3s
    }

    /* Responsive */
    @media(max-width:1100px) {
      .strip {
        grid-template-columns: repeat(3, minmax(0, 1fr))
      }

      .cell:nth-child(4) {
        border-left: 0
      }
    }

    @media(max-width:900px) {
      .form-grid {
        columns: 1
      }

      .console-columns {
        grid-template-columns: 1fr
      }
    }

    @media(max-width:640px) {
      .wrap {
        padding: 16px
      }

      .bezel {
        width: 52px;
        height: 40px
      }

      h1 {
        font-size: 15px
      }

      .strip {
        grid-template-columns: repeat(2, minmax(0, 1fr))
      }

      .cell {
        border-left: 0;
        border-top: 1px solid var(--line)
      }

      .cell:nth-child(-n+2) {
        border-top: 0
      }

      .card {
        padding: 16px
      }

      .row-2,
      .grid-2 {
        grid-template-columns: 1fr
      }

      .nav {
        width: 100%;
        display: grid;
        grid-template-columns: repeat(4, minmax(0, 1fr))
      }

      .nb {
        padding: 8px 4px;
        font-size: 11px
      }

      .prompt-row {
        flex-direction: column
      }
    }

    @media(prefers-reduced-motion:reduce) {

      *,
      *::before,
      *::after {
        animation: none !important;
        transition: none !important;
        scroll-behavior: auto !important
      }
    }
  </style>
</head>

<body>
  <canvas id="sky" aria-hidden="true"></canvas>

  <div class="wrap">
    <header class="bridge">
      <div class="id">
        <div class="bezel" aria-hidden="true"><i></i><i></i></div>
        <div>
          <div class="unit">Kiwi Unit · <span id="clock">--:--</span></div>
          <h1>Kiwi</h1>
          <div class="mood" id="moodLine">System Console</div>
        </div>
      </div>
      <nav class="nav" aria-label="Pages">
        <a class="nb on" href="/">Dashboard</a>
        <a class="nb" href="/faces">Faces</a>
        <a class="nb" href="/update">OTA</a>
      </nav>
    </header>

    <div class="tabs" role="tablist" aria-label="Dashboard tabs">
      <button type="button" class="tab-btn on" id="pillLive" onclick="setTab('live')">Live</button>
      <button type="button" class="tab-btn" id="pillSettings" onclick="setTab('settings')">Settings</button>
    </div>

    )rawcss";

static const char DASHBOARD_STATUS_TEMPLATE[] PROGMEM = R"rawstat(<section class="strip" id="statusStrip" aria-label="System status">
      <div class="cell">
        <div class="k">Hotspot SSID</div>
        <div class="v">%AP%</div>
      </div>
      <div class="cell">
        <div class="k">Setup URL / IP</div>
        <div class="v">192.168.4.1</div>
      </div>
      <div class="cell">
        <div class="k">Active Brain</div>
        <div class="v">%LLMPROV% / %ACTIVEMODEL%</div>
      </div>
      <div class="cell">
        <div class="k">Speech Engine</div>
        <div class="v">%STTPROV% / %STTLANGNAME%</div>
      </div>
      <div class="cell">
        <div class="k">Voice Synthesis</div>
        <div class="v">%TTSPROV%</div>
      </div>
    </section>

    <!-- LIVE SECTION (Real-Time Hardware & Voice Console - Zero Reboot Required) -->
    <div id="liveSection">
      <!-- Live voice interaction & serial monitor -->)rawstat";

static const char DASHBOARD_LIVE_CONSOLE[] PROGMEM = R"rawconsole(<section id="live" class="card live-card" aria-labelledby="liveTitle">
        <div class="card-title">
          <span id="liveTitle">Live Voice Interaction &amp; Serial Monitor</span>
          <span class="status-pill" id="liveStateBadge" role="status" aria-live="polite"><span class="dot"
              id="liveStateDot"></span><span id="liveStateText">Ready</span></span>
        </div>

        <div class="console-columns">
          <div class="stack">
            <button type="button" id="btnWebMic" class="mic-btn">
              <span class="mic-icon" aria-hidden="true"></span>
              <div>
                <div id="micBtnTitle" class="mic-title">Start Voice Recording</div>
                <div id="micBtnSub" class="muted" style="margin:2px 0 0">Stops shortly after you finish speaking</div>
              </div>
            </button>

            <div class="prompt-row">
              <input type="text" id="textPromptIn" placeholder="Type prompt or question for Kiwi..."
                aria-label="Prompt for Kiwi">
              <button type="button" id="btnSendPrompt" class="qbtn primary" style="padding:0 18px">Send</button>
            </div>

            <div class="readout">
              <div class="readout-k blue">Speech-To-Text Transcription</div>
              <div id="boxQuestion" class="readout-v">Awaiting voice input or prompt...</div>
            </div>

            <div class="readout">
              <div class="readout-k green">Kiwi AI Response</div>
              <div id="boxAnswer" class="readout-v">Awaiting response...</div>
            </div>
          </div>

          <div class="term">
            <div class="term-head">
              <span class="lamps"><i></i><i></i><i></i>&nbsp;ESP32-C3 · 115200 BAUD</span>
              <span style="display:flex;align-items:center;gap:10px">
                <label><input type="checkbox" id="autoScrollCheck" checked> Auto-scroll</label>
                <button type="button" id="btnClearLog" class="qbtn" style="padding:3px 10px;font-size:11px">Clear</button>
              </span>
            </div>
            <div id="serialTerminal" role="log" aria-label="Serial monitor" aria-live="off"></div>
          </div>
        </div>
      </section>

      <!-- Live Hardware Adjustments Deck (Real-Time · No Reboot Required) -->
      <div class="live-hardware-grid">
        <!-- Display brightness & night mode (Rocky Style) -->
        <div class="card rocky-bright-card">
          <div class="rocky-card-header" style="display:flex;align-items:center;justify-content:space-between">
            <span class="rocky-header-label">Display Brightness</span>
            <span class="live-saved-tick" id="tickBrightness"><svg width="12" height="12" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="3" stroke-linecap="round" stroke-linejoin="round"><polyline points="20 6 9 17 4 12"/></svg> Live</span>
          </div>
          <div class="rocky-slider-row">
            <input type="range" name="live_disp_bright" id="dayBrightSlider" min="0" max="100" value="%BRIGHT%"
              aria-label="Day brightness" oninput="onDayBrightChange(this.value)">
            <span class="rocky-percent-badge" id="dayBrightVal">%BRIGHT%%</span>
          </div>

          <div class="rocky-section-title">At Night Screen Mode</div>
          <div class="rocky-pill-group" role="radiogroup" aria-label="Night mode">
            <label class="rocky-pill %NIGHT_SEL_0%">
              <input type="radio" name="live_night_mode" value="0" %NIGHT_CHECK_0% onchange="onNightModeChange(0)">
              <span class="rocky-radio-dot"></span><span>Same as day</span>
            </label>
            <label class="rocky-pill %NIGHT_SEL_1%">
              <input type="radio" name="live_night_mode" value="1" %NIGHT_CHECK_1% onchange="onNightModeChange(1)">
              <span class="rocky-radio-dot"></span><span>Dim</span>
            </label>
            <label class="rocky-pill %NIGHT_SEL_2%">
              <input type="radio" name="live_night_mode" value="2" %NIGHT_CHECK_2% onchange="onNightModeChange(2)">
              <span class="rocky-radio-dot"></span><span>Screen off</span>
            </label>
          </div>

          <div class="rocky-slider-row" id="nightSliderRow" style="%NIGHT_SLIDER_STYLE%">
            <input type="range" name="live_night_bright" id="nightBrightSlider" min="0" max="100" value="%NIGHT_BRIGHT%"
              aria-label="Night brightness" oninput="onNightBrightChange(this.value)">
            <span class="rocky-percent-badge" id="nightBrightVal">%NIGHT_BRIGHT%%</span>
          </div>

          <p class="rocky-footnote">
            Night screen mode applies during circadian night or when Kiwi is asleep. Changes apply instantly without restart.
          </p>
        </div>

        <!-- Live Speaker Volume & Desk Quirks -->
        <div class="card">
          <div class="card-title" style="display:flex;align-items:center;justify-content:space-between">
            <span>Live Speaker Volume &amp; Desk Activity</span>
            <span class="live-saved-tick" id="tickAudio"><svg width="12" height="12" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="3" stroke-linecap="round" stroke-linejoin="round"><polyline points="20 6 9 17 4 12"/></svg> Live</span>
          </div>
          <label for="volSlider">Speaker Volume: <span id="volLabel"
              style="color:var(--amber);font-family:var(--mono);letter-spacing:0">%VOL%</span>%</label>
          <input type="range" id="volSlider" name="live_vol" min="0" max="100" value="%VOL%"
            oninput="onVolumeChangeLive(this.value)">

          <div class="panel">
            <label class="toggle">
              <input type="checkbox" name="live_boredom" id="rootBoredomToggle" value="1" %BOREDOM_CHECKED%
                onchange="toggleBoredomLive(this.checked)">
              <span class="switch"></span>
              <span style="font-weight:600">Autonomous Idle Desk Chatter &amp; Quirks</span>
            </label>
            <div style="margin-top:12px;display:flex;align-items:center;gap:10px;flex-wrap:wrap">
              <label for="boredomMinsSel" style="margin:0">Idle Chatter Time</label>
              <select id="boredomMinsSel" onchange="setBoredomIntervalLive(this.value)"
                style="width:auto;padding:6px 28px 6px 10px">
                <option value="1" %BM_1_SEL%>Every 1 Minute (Lively)</option>
                <option value="2" %BM_2_SEL%>Every 2 Minutes (Default)</option>
                <option value="3" %BM_3_SEL%>Every 3 Minutes</option>
                <option value="5" %BM_5_SEL%>Every 5 Minutes</option>
                <option value="10" %BM_10_SEL%>Every 10 Minutes</option>
                <option value="15" %BM_15_SEL%>Every 15 Minutes</option>
                <option value="30" %BM_30_SEL%>Every 30 Minutes</option>
              </select>
            </div>
            <small>When left idle, Kiwi makes witty remarks, health/tea reminders, and banter. Changes apply instantly without restart.</small>
          </div>
        </div>

        <!-- Night Sleep Schedule (Circadian Sleep) & Instant Manual Sleep -->
        <div class="card" style="grid-column:1 / -1">
          <div class="card-title" style="display:flex;align-items:center;justify-content:space-between">
            <span style="display:flex;align-items:center;gap:8px">
              <span>Night Sleep Schedule &amp; Sleep Mode</span>
            </span>
            <span class="live-saved-tick" id="tickCircadian"><svg width="12" height="12" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="3" stroke-linecap="round" stroke-linejoin="round"><polyline points="20 6 9 17 4 12"/></svg> Live</span>
          </div>

          <div style="display:grid;grid-template-columns:repeat(auto-fit,minmax(280px,1fr));gap:16px;align-items:start">
            <!-- Schedule Toggle & Time Picker -->
            <div class="panel">
              <label class="toggle">
                <input type="checkbox" id="liveCircEnToggle" %CIRC_EN_CHECKED% onchange="onCircadianToggle(this.checked)">
                <span class="switch"></span>
                <span style="font-weight:600">Enable Night Sleep Schedule</span>
              </label>

              <div class="rocky-time-row" id="circTimeRow" style="margin-top:14px">
                <span class="rocky-time-label">Sleep from</span>
                <div class="rocky-time-picker">
                  <select id="circStartSel" onchange="onScheduleChange()" aria-label="Night sleep start time">
                    %CIRC_START_OPTIONS%
                  </select>
                  <span class="rocky-clock-icon" aria-hidden="true">◷</span>
                </div>
                <span class="rocky-time-label">until</span>
                <div class="rocky-time-picker">
                  <select id="circEndSel" onchange="onScheduleChange()" aria-label="Night wake up time">
                    %CIRC_END_OPTIONS%
                  </select>
                  <span class="rocky-clock-icon" aria-hidden="true">◷</span>
                </div>
              </div>

              <small style="display:block;margin-top:10px;line-height:1.5;color:var(--muted)">
                During these hours, Kiwi automatically enters sleep mode to save OLED display life and rest quietly.
              </small>
            </div>

            <!-- Instant Manual Sleep / Wake Control -->
            <div class="panel" style="display:flex;flex-direction:column;justify-content:space-between;min-height:120px">
              <div>
                <div style="font-weight:600;font-size:13px;color:var(--ink)">Instant Manual Sleep / Wake</div>
                <div id="sleepStateHint" style="font-size:12px;color:var(--muted);margin-top:4px">
                  Current state: Robot is awake and active
                </div>
              </div>

              <div style="margin-top:14px;display:flex;align-items:center;gap:10px;flex-wrap:wrap">
                <button type="button" class="qbtn" id="btnManualSleep" onclick="toggleManualSleep()" style="padding:10px 18px;font-weight:600;display:inline-flex;align-items:center;gap:6px">
                  <span id="manualSleepIco">😴</span> <span id="manualSleepTxt">Put Kiwi to Sleep</span>
                </button>
                <small style="color:var(--muted)">Hold head for 7s to sleep via touch</small>
              </div>
            </div>
          </div>
        </div>
      </div>
    </div> <!-- End #liveSection -->

    <!-- SETTINGS SECTION (Permanent Configuration Deck - Saves & Restarts) -->
    <div id="settingsSection" style="display:none">
)rawconsole";

static const char DASHBOARD_BODY_PART1[] PROGMEM = R"rawb1(<form method="POST" action="/save" class="form-grid" id="cfgForm">
        <!-- Hidden inputs preserving live parameters across form saves -->
        <input type="hidden" name="vol" id="hiddenFormVol" value="%VOL%">
        <input type="hidden" name="disp_bright" id="hiddenFormBright" value="%BRIGHT%">
        <input type="hidden" name="night_bright" id="hiddenFormNightBright" value="%NIGHT_BRIGHT%">
        <input type="hidden" name="night_mode" id="hiddenFormNightMode" value="1">
        <input type="hidden" name="circ_start" id="hiddenFormCircStart" value="%CIRC_START%">
        <input type="hidden" name="circ_end" id="hiddenFormCircEnd" value="%CIRC_END%">
        <input type="hidden" name="circ_en" id="hiddenFormCircEn" value="%CIRC_EN_VAL%">
        <input type="hidden" name="boredom" id="hiddenFormBoredom" value="%BOREDOM_CHECKED%">
        <input type="hidden" name="boredom_mins" id="hiddenFormBoredomMins" value="%BOREDOM_MINS%">

                        <!-- WI-FI & LOCAL NETWORK SETUP (Prominent Top Card) -->
        <div class="card credentials-card" style="margin-bottom:16px">
          <div class="card-head" style="margin-bottom:6px">
            <div class="card-title" style="margin:0;border:0;padding:0">📶 WI-FI &amp; LOCAL NETWORK SETUP</div>
            <button type="button" id="btnScanWifi" class="qbtn" onclick="scanWifiNetworks()" style="padding:6px 14px;font-size:11px">
              <span id="scanSpinner" class="spin" style="display:none">↻</span>
              <span id="scanBtnText">🔍 Scan Networks</span>
            </button>
          </div>
          <p style="margin:0 0 14px;color:var(--muted);font-size:12px;line-height:1.4">
            Connect Kiwi to your home 2.4GHz Wi-Fi network. Click "Scan Networks" to search nearby access points.
          </p>
          <div class="credentials-deck" style="margin-top:10px">
            <div>
              <label for="wifiScanSelect">DETECTED 2.4GHz NETWORKS</label>
              <select id="wifiScanSelect" onchange="onSelectScannedWifi(this.value)">
                <option value="">-- Click "Scan Networks" to search --</option>
              </select>
              <small>Selecting a network automatically fills the SSID</small>
            </div>
            <div>
              <label for="wifiSsidInput">WI-FI NETWORK (SSID)</label>
              <input name="ssid" id="wifiSsidInput" value="%SSID%" required placeholder="Enter 2.4GHz Wi-Fi name">
              <small>Case-sensitive 2.4GHz network name</small>
            </div>
            <div style="grid-column:1 / -1">
              <label for="wifiPassInput">WI-FI PASSWORD</label>
              <div class="pw">
                <input name="pass" id="wifiPassInput" type="password" value="%PASS%" placeholder="Enter Wi-Fi password (leave blank for open)">
                <button type="button" class="icon-btn" onclick="togglePassVisibility(this)" title="Show/Hide Password">SHOW</button>
              </div>
              <small>Your password is securely saved in ESP32 flash memory</small>
            </div>
          </div>
        </div>

<!-- CLOUD API KEYS & CREDENTIALS (Centralized Unified 2-Column Deck) -->
        <div class="card credentials-card">
          <div class="card-head" style="margin-bottom:6px">
            <div class="card-title" style="margin:0;border:0;padding:0">🔑 CLOUD API KEYS &amp; CREDENTIALS</div>
          </div>
          <p style="margin:0 0 16px;color:var(--muted);font-size:12px;line-height:1.45;letter-spacing:0.02em">
            Manage all cloud AI credentials in one unified 2-column deck. Keys are stored locally in ESP32 flash memory
          </p>

          <div class="credentials-deck">
            <!-- Left Column: Google Gemini -->
            <div class="api-key-item">
              <label for="geminiKeyIn">GOOGLE GEMINI API</label>
              <div class="pw">
                <input name="llmkey" id="geminiKeyIn" type="password" value="%LLMKEY%" placeholder="AIzaSy...">
                <button type="button" class="icon-btn" onclick="togglePw('geminiKeyIn', this)">SHOW</button>
              </div>
              <small>Google Gemini 2.0 Flash &amp; Flash-Lite reasoning</small>
            </div>

            <!-- Right Column: Sarvam AI -->
            <div class="api-key-item">
              <label for="sarvamKeyIn">SARVAM AI API</label>
              <div class="pw">
                <input name="ttskey" id="sarvamKeyIn" type="password" value="%TTSKEY%" placeholder="Enter Sarvam API key">
                <button type="button" class="icon-btn" onclick="togglePw('sarvamKeyIn', this)">SHOW</button>
              </div>
              <small>Indian regional languages STT (Saaras) &amp; TTS (Bulbul v3/v4)</small>
            </div>

            <!-- Left Column: Groq Cloud -->
            <div class="api-key-item">
              <label for="groqKeyIn">GROQ CLOUD API</label>
              <div class="pw">
                <input name="groqkey" id="groqKeyIn" type="password" value="%GROQKEY%" placeholder="gsk_...">
                <button type="button" class="icon-btn" onclick="togglePw('groqKeyIn', this)">SHOW</button>
              </div>
              <small>Ultra-fast Groq LLM (LLaMA 3.3, Qwen) &amp; Groq Whisper STT</small>
            </div>

            <!-- Right Column: ElevenLabs -->
            <div class="api-key-item">
              <label for="elevenKeyIn">ELEVENLABS API</label>
              <div class="pw">
                <input name="elevkey" id="elevenKeyIn" type="password" value="%ELEVENKEY%" placeholder="Enter ElevenLabs API key">
                <button type="button" class="icon-btn" onclick="togglePw('elevenKeyIn', this)">SHOW</button>
              </div>
              <small>ElevenLabs ultra-realistic neural TTS voices</small>
            </div>

            <!-- Left Column: OpenAI -->
            <div class="api-key-item">
              <label for="openaiKeyIn">OPENAI API</label>
              <div class="pw">
                <input name="openaikey" id="openaiKeyIn" type="password" value="%OPENAIKEY%" placeholder="sk-...">
                <button type="button" class="icon-btn" onclick="togglePw('openaiKeyIn', this)">SHOW</button>
              </div>
              <small>OpenAI GPT models (gpt-4o-mini), Whisper STT, and OpenAI TTS</small>
            </div>

            <!-- Right Column: Fish Audio -->
            <div class="api-key-item">
              <label for="fishKeyIn">FISH AUDIO API</label>
              <div class="pw">
                <input name="fishkey" id="fishKeyIn" type="password" value="%FISHKEY%" placeholder="Enter Fish Audio API key">
                <button type="button" class="icon-btn" onclick="togglePw('fishKeyIn', this)">SHOW</button>
              </div>
              <small>Fish Speech neural voice synthesis (S2.1)</small>
            </div>
          </div>
        </div>

        <div class="ai-pipeline-deck">
          <!-- 1. Speech-to-Text -->
          <div class="card">
            <div class="card-title">1. SPEECH-TO-TEXT (STT)</div>
            <label for="sttProvSel">STT PROVIDER</label>
            <select name="sttprovider" id="sttProvSel" onchange="toggleStt()">
              <option value="sarvam" %STT_SARVAM_SEL%>Sarvam AI (saaras:v3)</option>
              <option value="groq" %STT_GROQ_SEL%>Groq Whisper (fastest)</option>
              <option value="openai" %STT_OPENAI_SEL%>OpenAI Whisper</option>
            </select>
            <label>STT LANGUAGE</label>
            <select name="sttlang">
              <option value="auto" %LANG_AUTO_SEL%>Auto (Hindi + English)</option>
              <option value="hi" %LANG_HI_SEL%>Force Hindi</option>
              <option value="en" %LANG_EN_SEL%>Force English</option>
            </select>
            <div id="sarvamBox">
              <h3>Sarvam AI Settings</h3>
              <label>Endpoint</label><input name="stturl" value="%STTURL%">
              <label>Model</label>
              <select name="sttmodel">
                <option value="saaras:v3" %STT_SAARAS_SEL%>saaras:v3</option>
                <option value="saarika:v2" %STT_SAARIKA_SEL%>saarika:v2</option>
              </select>
            </div>
            <div id="groqSttBox">
              <h3>Groq Whisper Settings</h3>
              <label>Endpoint</label><input name="groqstturl" value="%GROQSTTURL%">
              <label>Model</label>
              <select name="groqsttmodel">
                <option value="whisper-large-v3-turbo" %GW3T_SEL%>whisper-large-v3-turbo (fast)</option>
                <option value="whisper-large-v3" %GW3_SEL%>whisper-large-v3 (accurate)</option>
                <option value="distil-whisper-large-v3-en" %GDW_SEL%>distil-whisper-large-v3-en</option>
              </select>
            </div>
            <div id="openaiSttBox">
              <h3>OpenAI Whisper Settings</h3>
              <label>Endpoint</label><input name="opaistturl" value="%OPENAIURL%">
              <label>Model</label>
              <select name="opaisttmodel">
                <option value="whisper-1" %OW1_SEL%>whisper-1</option>
                <option value="gpt-4o-mini-transcribe" %OG4M_SEL%>gpt-4o-mini-transcribe</option>
                <option value="gpt-4o-transcribe" %OG4_SEL%>gpt-4o-transcribe</option>
              </select>
            </div>
          </div>

          <!-- 2. AI Intelligence -->
          <div class="card">
            <div class="card-title">2. AI INTELLIGENCE (LLM)</div>
            <label for="provSel">LLM PROVIDER</label>
            <select name="llmprovider" id="provSel" onchange="toggleModels()">
              <option value="gemini" %GEMINI_SEL%>Google Gemini</option>
              <option value="groq" %GROQ_SEL%>Groq Cloud</option>
              <option value="openai" %OPENAI_SEL%>OpenAI GPT</option>
            </select>
            <div id="geminiBox">
              <h3>Gemini Model &amp; Settings</h3>
              <label>Model</label>
              <select name="llmmodel" id="geminiModelSel" onchange="updateGeminiUrl()">
                <option value="gemini-2.0-flash-lite" %GE20L_SEL%>gemini-2.0-flash-lite</option>
                <option value="gemini-2.5-flash" %GE25_SEL%>gemini-2.5-flash</option>
                <option value="gemini-2.5-flash-lite" %GE25L_SEL%>gemini-2.5-flash-lite</option>
              </select>
              <label>Endpoint</label><input name="llmurl" id="geminiUrlIn" value="%LLMURL%">
            </div>
            <div id="groqBox">
              <h3>Groq Model &amp; Settings</h3>
              <label>Model</label>
              <select name="groqmodel">
                <option value="openai/gpt-oss-120b" %G120B_SEL%>openai/gpt-oss-120b</option>
                <option value="openai/gpt-oss-20b" %G20B_SEL%>openai/gpt-oss-20b</option>
                <option value="qwen/qwen3-32b" %GQ32B_SEL%>qwen/qwen3-32b</option>
                <option value="llama-3.3-70b-versatile" %GL33_SEL%>llama-3.3-70b-versatile</option>
              </select>
              <label>Endpoint</label><input name="groqurl" value="%GROQURL%">
            </div>
            <div id="openaiBox">
              <h3>OpenAI GPT Model &amp; Settings</h3>
              <label>Model</label>
              <select name="openaimodel" id="openaiModelSel" onchange="updateOpenAiUrl()">
                <option value="gpt-4o-mini" %GPT4OM_SEL%>gpt-4o-mini (fast, recommended)</option>
                <option value="gpt-4.1-mini" %GPT41M_SEL%>gpt-4.1-mini (quality)</option>
                <option value="gpt-4.1-nano" %GPT41N_SEL%>gpt-4.1-nano (fastest)</option>
                <option value="gpt-4o" %GPT4O_SEL%>gpt-4o (higher quality)</option>
              </select>
              <label>Endpoint</label><input name="openaiurl" id="openaiUrlIn" value="%OPENAI_LLM_URL%">
            </div>
          </div>
)rawb1";

static const char DASHBOARD_BODY_PART2[] PROGMEM = R"rawb2(<!-- Text-to-Speech -->
          <div class="card">
            <div class="card-title">3. TEXT-TO-SPEECH (TTS)</div>
            <label for="ttsProvSel">TTS PROVIDER</label>
            <select name="ttsprovider" id="ttsProvSel" onchange="toggleTts()">
              <option value="sarvam" %TTS_SARVAM_SEL%>Sarvam AI (bulbul:v3)</option>
              <option value="openai" %TTS_OPENAI_SEL%>OpenAI TTS</option>
              <option value="elevenlabs" %TTS_ELEVEN_SEL%>ElevenLabs</option>
              <option value="fish" %TTS_FISH_SEL%>Fish Audio (Fish Speech)</option>
            </select>
            <div id="ttsSarvamBox">
              <h3>Sarvam TTS Settings</h3>
              <label>Endpoint</label><input name="ttsurl" value="%TTSURL%">
              <label>Model</label>
              <select name="ttsmodel">
                <option value="bulbul:v4-flash" %SARVAM_V4F_SEL%>bulbul:v4-flash (⚡ New Flash Model · Ultra Fast)</option>
                <option value="bulbul:v3" %SARVAM_V3_SEL%>bulbul:v3 (Latest Stable · Hindi/Hinglish/English)</option>
                <option value="bulbul:v2" %SARVAM_V2_SEL%>bulbul:v2 (Legacy v2)</option>
              </select>
              <label>Voice / Speaker</label>
              <select name="ttsvoice">
                %SARVAM_VOICE_OPTIONS%
              </select>
            </div>
            <div id="ttsOpenaiBox">
              <h3>OpenAI TTS Settings</h3>
              <label>Endpoint</label><input name="openaittsurl" value="%OPENAITTSURL%">
              <label>Model</label><input name="openaittsmodel" value="%OPENAITTSMODEL%">
              <label>Voice</label>
              <select name="openaittsvoice">
                <option value="alloy" %OA_ALLOY_SEL%>alloy</option>
                <option value="echo" %OA_ECHO_SEL%>echo</option>
                <option value="fable" %OA_FABLE_SEL%>fable</option>
                <option value="onyx" %OA_ONYX_SEL%>onyx</option>
                <option value="nova" %OA_NOVA_SEL%>nova</option>
                <option value="shimmer" %OA_SHIMMER_SEL%>shimmer</option>
              </select>
            </div>
            <div id="ttsElevenBox">
              <h3>ElevenLabs Settings</h3>
              <label>Endpoint</label><input name="elevurl" value="%ELEVURL%">
              <label>Model</label><input name="elevmodel" value="%ELEVMODEL%">
              <label>Voice ID</label><input name="elevvoice" value="%ELEVVOICE%">
            </div>
            <div id="ttsFishBox">
              <h3>Fish Audio Settings</h3>
              <label>Endpoint</label><input name="fishurl" value="%FISHURL%">
              <label>Model</label><input name="fishmodel" value="%FISHMODEL%">
              <label>Voice / Reference ID (Optional)</label>
              <input name="fishvoice" value="%FISHVOICE%" placeholder="e.g. voice model reference ID">
            </div>
          </div>
        </div> <!-- End ai-pipeline-deck -->

        

        <!-- Touch Controls & Hardware Sensitivity -->
      <div class="card">
        <div class="card-title">Touch Controls &amp; Hardware Sensitivity</div>
        <small style="margin:0 0 14px">Customize touch sensor responsiveness, debounce filter, and capacitive gestures.</small>

        <div class="panel" style="margin-bottom:14px">
          <label class="toggle" style="margin-bottom:10px">
            <input type="checkbox" name="tch_snd" value="1" %TCH_SND_CHECKED%>
            <span class="switch"></span>
            <span style="font-weight:600">Instant Audio/Visual Feedback (Blip &amp; Wink)</span>
          </label>
          <label for="tchDebSlider" style="margin-top:6px">Debounce Filter / Sensitivity: <span id="tchDebLabel"
              style="color:var(--amber);font-family:var(--mono);letter-spacing:0">%TCH_DEB%</span> ms</label>
          <input type="range" id="tchDebSlider" name="tch_deb" min="15" max="150" step="5" value="%TCH_DEB%"
            oninput="document.getElementById('tchDebLabel').textContent=this.value">
          <small>Lower ms (15-30ms) = Ultra-responsive touch. Higher ms (50-100ms) = Filters out static &amp; accidental ghost touches.</small>
        </div>

        <div class="grid-2" style="margin-top:14px">
          <div><label>Top Head (Single Tap)</label><select name="act_top_tap" class="touch-act-sel"
              data-val="%ACT_TOP_TAP%"></select></div>
          <div><label>Top Head (Double Tap)</label><select name="act_top_dbl" class="touch-act-sel"
              data-val="%ACT_TOP_DBL%"></select></div>
          <div><label>Left Cheek (Single Tap)</label><select name="act_lt_tap" class="touch-act-sel"
              data-val="%ACT_LT_TAP%"></select></div>
          <div><label>Left Cheek (Double Tap)</label><select name="act_lt_dbl" class="touch-act-sel"
              data-val="%ACT_LT_DBL%"></select></div>
          <div><label>Right Cheek (Single Tap)</label><select name="act_rt_tap" class="touch-act-sel"
              data-val="%ACT_RT_TAP%"></select></div>
          <div><label>Right Cheek (Double Tap)</label><select name="act_rt_dbl" class="touch-act-sel"
              data-val="%ACT_RT_DBL%"></select></div>
          <div><label>Left Cheek (Hold 1.5s)</label><select name="act_lt_hld" class="touch-act-sel"
              data-val="%ACT_LT_HLD%"></select></div>
          <div><label>Right Cheek (Hold 1.5s)</label><select name="act_rt_hld" class="touch-act-sel"
              data-val="%ACT_RT_HLD%"></select></div>
        </div>
        <small class="tip">Tip: Hold Top Head for 3s to show Clock; hold for 7s to enter Night Sleep.</small>
      </div>

      <!-- Weather & personality -->
      <div class="card">
        <div class="card-title">Weather &amp; Personality</div>
        <label for="wCity">City Name</label><input id="wCity" name="wthr_city" value="%WTHR_CITY%">
        <div class="row-2">
          <div><label for="wLat">Latitude</label><input id="wLat" name="wthr_lat" value="%WTHR_LAT%"></div>
          <div><label for="wLon">Longitude</label><input id="wLon" name="wthr_lon" value="%WTHR_LON%"></div>
        </div>
        <label for="skinSel" style="margin-top:16px">OLED Face &amp; Eye Skin</label>
        <select name="skin" id="skinSel" onchange="toggleSkinLive(this.value)">
          <option value="kiwi" %SKIN_KIWI_SEL%>Desk Buddy (Cute Block) [Default]</option>
          <option value="default" %SKIN_DEFAULT_SEL%>Default (Clean Round Vector)</option>
          <option value="cyberpunk" %SKIN_CYBER_SEL%>Cyberpunk Visor &amp; HUD</option>
          <option value="cat" %SKIN_CAT_SEL%>Kawaii Cat Neko &amp; Ears</option>
          <option value="retro" %SKIN_RETRO_SEL%>Retro 8-Bit Arcade</option>
          <option value="anime" %SKIN_ANIME_SEL%>Anime Shounen (Starry Glint)</option>
          <option value="hearts" %SKIN_HEARTS_SEL%>Lovestruck Heart Eyes</option>
          <option value="steampunk" %SKIN_STEAM_SEL%>Steampunk Aviator Goggles</option>
          <option value="demon" %SKIN_DEMON_SEL%>Demon Spooky (Fiery Horns)</option>
          <option value="matrix" %SKIN_MATRIX_SEL%>Matrix Code Stream (Terminal)</option>
          <option value="cyborg" %SKIN_CYBORG_SEL%>Cyborg T-800 (Targeting Aperture)</option>
          <option value="mecha" %SKIN_MECHA_SEL%>Mecha Gundam (V-Fin Crest &amp; Core)</option>
          <option value="sunglasses" %SKIN_SHADES_SEL%>Boss Shades (Cool Sunglasses)</option>
          <option value="puppy" %SKIN_PUPPY_SEL%>Kawaii Puppy (Floppy Ears &amp; Boba)</option>
          <option value="hypno" %SKIN_HYPNO_SEL%>Hypno Spiral (Mesmerizing Vortex)</option>
          <option value="alien" %SKIN_ALIEN_SEL%>Cosmic Alien (UFO Orbit &amp; Antenna)</option>
          <option value="ninja" %SKIN_NINJA_SEL%>Shinobi Ninja (Headband &amp; Stealth)</option>
          <option value="round" %SKIN_ROUND_SEL%>Round Eyes (Classic Ocular)</option>
        </select>
        <label for="promptIn" style="margin-top:16px">Robot System Prompt</label>
        <textarea id="promptIn" name="prompt">%PROMPT%</textarea>
      </div>

      <!-- Memory -->
      <div class="card">
        <div class="card-title">Personal Life Diary &amp; Long-Term Memory</div>
        <label for="memName">Your Name (Owner)</label>
        <input id="memName" name="mem_name" value="%MEM_NAME%" placeholder="e.g. Rohil">

        <label style="margin-top:14px">Facts Kiwi Remembers About You</label>
        <div class="readout" style="margin-bottom:12px;max-height:180px;overflow-y:auto">
          %MEM_LIST_HTML%
        </div>

        <label for="newMem">Add New Memory Fact / Event</label>
        <input id="newMem" name="new_mem" placeholder="e.g. Coding test tomorrow / Loves chai">
        <small>Kiwi automatically learns facts while chatting, remembers them across reboots in Flash, and follows up on
          events!</small>

        <div style="display:flex;margin-top:14px">
          <button type="button" class="qbtn" style="flex:1" onclick="clearMemories()">Clear Memories</button>
        </div>
      </div>

      <div class="save-wrap">
        <button type="submit" class="save-btn">Save Configuration &amp; Restart</button>
      </div>
    </form>
  </div> <!-- End #settingsSection -->
</div> <!-- End .wrap -->

<div class="toast" id="toast" role="status" aria-live="polite"></div>
)rawb2";
static String makeHourOptions(int selectedHour) {
  String out = "";
  for (int h = 0; h < 24; h++) {
    int h12 = h % 12;
    if (h12 == 0) h12 = 12;
    const char *ampm = (h >= 12) ? "PM" : "AM";
    char buf[32];
    snprintf(buf, sizeof(buf), "%02d:00 %s", h12, ampm);
    out += "<option value=\"";
    out += String(h);
    out += "\"";
    if (h == selectedHour) out += " selected";
    out += ">";
    out += buf;
    out += "</option>\n";
  }
  return out;
}

static String getDashboardLiveConsole() {
  String h;
  h.reserve(7000);
  h = FPSTR(DASHBOARD_LIVE_CONSOLE);

  h.replace("%BRIGHT%", String(displayBrightness));
  h.replace("%NIGHT_BRIGHT%", String(nightBrightness));
  h.replace("%NIGHT_SEL_0%", nightMode == 0 ? "active" : "");
  h.replace("%NIGHT_SEL_1%", nightMode == 1 ? "active" : "");
  h.replace("%NIGHT_SEL_2%", nightMode == 2 ? "active" : "");
  h.replace("%NIGHT_CHECK_0%", nightMode == 0 ? "checked" : "");
  h.replace("%NIGHT_CHECK_1%", nightMode == 1 ? "checked" : "");
  h.replace("%NIGHT_CHECK_2%", nightMode == 2 ? "checked" : "");
  h.replace("%NIGHT_SLIDER_STYLE%", nightMode == 2 ? "opacity:0.25;pointer-events:none" : "");
  h.replace("%CIRC_START_OPTIONS%", makeHourOptions(circadianSleepStart));
  h.replace("%CIRC_END_OPTIONS%", makeHourOptions(circadianSleepEnd));

  h.replace("%VOL%", String(speakerVolume));
  h.replace("%BOREDOM_CHECKED%", boredomEnabled ? "checked" : "");
  h.replace("%BM_1_SEL%", boredomMinutes == 1 ? "selected" : "");
  h.replace("%BM_2_SEL%", boredomMinutes == 2 ? "selected" : "");
  h.replace("%BM_3_SEL%", boredomMinutes == 3 ? "selected" : "");
  h.replace("%BM_5_SEL%", boredomMinutes == 5 ? "selected" : "");
  h.replace("%BM_10_SEL%", boredomMinutes == 10 ? "selected" : "");
  h.replace("%BM_15_SEL%", boredomMinutes == 15 ? "selected" : "");
  h.replace("%BM_30_SEL%", boredomMinutes == 30 ? "selected" : "");

  return h;
}

static String getDashboardStatusHtml() {
  String h;
  h.reserve(1200);
  h = FPSTR(DASHBOARD_STATUS_TEMPLATE);

  String langName = (sttLang == "hi") ? "Hindi" : (sttLang == "en") ? "English" : "Auto (HI+EN)";
  h.replace("%AP%", esc(apSSID));
  h.replace("%LLMPROV%", esc(llmProvider));
  h.replace("%ACTIVEMODEL%", esc(llmProvider == "groq" ? groqModel : llmModel));
  h.replace("%STTPROV%", esc(sttProvider));
  h.replace("%STTLANGNAME%", esc(langName));
  h.replace("%TTSPROV%", esc(ttsProvider));

  return h;
}

static String getDashboardBodyPart1() {
  String h;
  h.reserve(8000);
  h = FPSTR(DASHBOARD_BODY_PART1);

  h.replace("%SSID%", esc(wifiSSID));
  h.replace("%PASS%", esc(wifiPassword));

  h.replace("%VOL%", String(speakerVolume));
  h.replace("%BRIGHT%", String(displayBrightness));
  h.replace("%NIGHT_BRIGHT%", String(nightBrightness));
  h.replace("%NIGHT_MODE%", String(nightMode));
  h.replace("%CIRC_START%", String(circadianSleepStart));
  h.replace("%CIRC_END%", String(circadianSleepEnd));
  h.replace("%BOREDOM_CHECKED%", boredomEnabled ? "1" : "0");
  h.replace("%BOREDOM_MINS%", String(boredomMinutes));

  h.replace("%STT_SARVAM_SEL%", sttProvider == "sarvam" ? "selected" : "");
  h.replace("%STT_GROQ_SEL%", sttProvider == "groq" ? "selected" : "");
  h.replace("%STT_OPENAI_SEL%", sttProvider == "openai" ? "selected" : "");
  h.replace("%LANG_AUTO_SEL%", sttLang == "auto" ? "selected" : "");
  h.replace("%LANG_HI_SEL%", sttLang == "hi" ? "selected" : "");
  h.replace("%LANG_EN_SEL%", sttLang == "en" ? "selected" : "");

  h.replace("%STTURL%", esc(sttEndpoint));
  h.replace("%STTKEY%", esc(sttKey));
  h.replace("%STT_SAARAS_SEL%", sttModel == "saaras:v3" ? "selected" : "");
  h.replace("%STT_SAARIKA_SEL%", sttModel == "saarika:v2" ? "selected" : "");
  h.replace("%GROQSTTURL%", esc(groqSttEndpoint));
  h.replace("%GROQSTTKEY%", esc(groqSttKey));
  h.replace("%GW3T_SEL%", groqSttModel == "whisper-large-v3-turbo" ? "selected" : "");
  h.replace("%GW3_SEL%", groqSttModel == "whisper-large-v3" ? "selected" : "");
  h.replace("%GDW_SEL%", groqSttModel == "distil-whisper-large-v3-en" ? "selected" : "");
  h.replace("%OPENAIURL%", esc(openaiSttEndpoint));
  h.replace("%OPENAIKEY%", esc(openaiSttKey));
  h.replace("%OW1_SEL%", openaiSttModel == "whisper-1" ? "selected" : "");
  h.replace("%OG4M_SEL%", openaiSttModel == "gpt-4o-mini-transcribe" ? "selected" : "");
  h.replace("%OG4_SEL%", openaiSttModel == "gpt-4o-transcribe" ? "selected" : "");

  h.replace("%GROQ_SEL%", llmProvider == "groq" ? "selected" : "");
  h.replace("%GEMINI_SEL%", llmProvider == "gemini" ? "selected" : "");
  h.replace("%OPENAI_SEL%", llmProvider == "openai" ? "selected" : "");
  h.replace("%G20B_SEL%", groqModel == "openai/gpt-oss-20b" ? "selected" : "");
  h.replace("%G120B_SEL%", groqModel == "openai/gpt-oss-120b" ? "selected" : "");
  h.replace("%GQ32B_SEL%", groqModel == "qwen/qwen3-32b" ? "selected" : "");
  h.replace("%GL33_SEL%", groqModel == "llama-3.3-70b-versatile" ? "selected" : "");
  h.replace("%GE20L_SEL%", (llmModel == "gemini-2.0-flash-lite" || llmModel == "gemini-3.5-flash-lite") ? "selected" : "");
  h.replace("%GE25_SEL%", llmModel == "gemini-2.5-flash" ? "selected" : "");
  h.replace("%GE25L_SEL%", llmModel == "gemini-2.5-flash-lite" ? "selected" : "");
  h.replace("%GPT4OM_SEL%", llmModel == "gpt-4o-mini" ? "selected" : "");
  h.replace("%GPT41M_SEL%", llmModel == "gpt-4.1-mini" ? "selected" : "");
  h.replace("%GPT41N_SEL%", llmModel == "gpt-4.1-nano" ? "selected" : "");
  h.replace("%GPT4O_SEL%", llmModel == "gpt-4o" ? "selected" : "");
  h.replace("%LLMURL%", esc(llmEndpoint));
  h.replace("%LLMKEY%", esc(llmKey));
  h.replace("%OPENAI_LLM_URL%", esc(llmEndpoint));
  h.replace("%OPENAI_LLM_KEY%", esc(llmKey));
  h.replace("%OPENAIKEY%", esc(openaiSttKey.length() ? openaiSttKey : (openaiTtsKey.length() ? openaiTtsKey : (llmProvider == "openai" ? llmKey : ""))));
  h.replace("%GROQURL%", esc(groqEndpoint));
  h.replace("%GROQKEY%", esc(groqKey));
  h.replace("%TTSKEY%", esc(ttsKey.length() ? ttsKey : sttKey));
  h.replace("%ELEVENKEY%", esc(elevenKey));
  h.replace("%FISHKEY%", esc(fishKey));

  return h;
}

static String getDashboardBodyPart2() {
  String h;
  h.reserve(18000);
  h = FPSTR(DASHBOARD_BODY_PART2);

  h.replace("%TTS_SARVAM_SEL%", ttsProvider == "sarvam" ? "selected" : "");
  h.replace("%TTS_OPENAI_SEL%", ttsProvider == "openai" ? "selected" : "");
  h.replace("%TTS_ELEVEN_SEL%", ttsProvider == "elevenlabs" ? "selected" : "");
  h.replace("%TTS_FISH_SEL%", (ttsProvider == "fish" || ttsProvider == "fishaudio") ? "selected" : "");
  h.replace("%TTSURL%", esc(ttsEndpoint));
  h.replace("%TTSKEY%", esc(ttsKey));
  h.replace("%SARVAM_V4F_SEL%", (ttsModel == "bulbul:v4-flash" || ttsModel == "bulbul:v4") ? "selected" : "");
  h.replace("%SARVAM_V3_SEL%", (ttsModel == "bulbul:v3" || ttsModel == "" || ttsModel == "DEFAULT") ? "selected" : "");
  h.replace("%SARVAM_V2_SEL%", ttsModel == "bulbul:v2" ? "selected" : "");

  static const struct {
    const char *id;
    const char *label;
  } sarvamVoices[] = {
    {"amit", "Amit (Male · Clear & Expressive)"},
    {"priya", "Priya (Female · Default)"},
    {"aditya", "Aditya (Male · Dynamic)"},
    {"ritu", "Ritu (Female · Natural)"},
    {"ashutosh", "Ashutosh (Male · Deep Voice)"},
    {"neha", "Neha (Female · Warm)"},
    {"rahul", "Rahul (Male · Casual)"},
    {"pooja", "Pooja (Female · Friendly)"},
    {"rohan", "Rohan (Male · Young)"},
    {"simran", "Simran (Female · Cheerful)"},
    {"kabir", "Kabir (Male · Authoritative)"},
    {"shreya", "Shreya (Female · Sweet)"},
    {"varun", "Varun (Male · Energetic)"},
    {"ishita", "Ishita (Female · Professional)"},
    {"kavya", "Kavya (Female · Soft)"},
    {"dev", "Dev (Male · Modern)"},
    {"ratan", "Ratan (Male · Classic)"},
    {"manan", "Manan (Male)"},
    {"sumit", "Sumit (Male)"},
    {"roopa", "Roopa (Female)"},
    {"aayan", "Aayan (Male)"},
    {"shubh", "Shubh (Male)"},
    {"advait", "Advait (Male)"},
    {"anand", "Anand (Male)"},
    {"tanya", "Tanya (Female)"},
    {"tarun", "Tarun (Male)"},
    {"sunny", "Sunny (Male)"},
    {"mani", "Mani (Male)"},
    {"gokul", "Gokul (Male)"}
  };
  String voiceOpts;
  voiceOpts.reserve(2200);
  for (const auto &v : sarvamVoices) {
    bool sel = (ttsVoice.equalsIgnoreCase(v.id)) || (ttsVoice == "" && strcmp(v.id, "priya") == 0);
    voiceOpts += "          <option value=\"";
    voiceOpts += v.id;
    voiceOpts += "\"";
    if (sel) voiceOpts += " selected";
    voiceOpts += ">";
    voiceOpts += v.label;
    voiceOpts += "</option>\n";
  }
  h.replace("%SARVAM_VOICE_OPTIONS%", voiceOpts);

  h.replace("%OPENAITTSURL%", esc(openaiTtsEndpoint));
  h.replace("%OPENAITTSKEY%", esc(openaiTtsKey));
  h.replace("%OPENAITTSMODEL%", esc(openaiTtsModel));
  h.replace("%OPENAITTSVOICE%", esc(openaiTtsVoice));
  h.replace("%OA_ALLOY_SEL%", (openaiTtsVoice == "alloy" || openaiTtsVoice == "") ? "selected" : "");
  h.replace("%OA_ECHO_SEL%", openaiTtsVoice == "echo" ? "selected" : "");
  h.replace("%OA_FABLE_SEL%", openaiTtsVoice == "fable" ? "selected" : "");
  h.replace("%OA_ONYX_SEL%", openaiTtsVoice == "onyx" ? "selected" : "");
  h.replace("%OA_NOVA_SEL%", openaiTtsVoice == "nova" ? "selected" : "");
  h.replace("%OA_SHIMMER_SEL%", openaiTtsVoice == "shimmer" ? "selected" : "");

  h.replace("%ELEVURL%", esc(elevenEndpoint));
  h.replace("%ELEVKEY%", esc(elevenKey));
  h.replace("%ELEVMODEL%", esc(elevenModel));
  h.replace("%ELEVVOICE%", esc(elevenVoice));

  h.replace("%FISHURL%", esc(fishEndpoint));
  h.replace("%FISHKEY%", esc(fishKey));
  h.replace("%FISHMODEL%", esc(fishModel));
  h.replace("%FISHVOICE%", esc(fishVoice));

  h.replace("%VOL%", String(speakerVolume));
  h.replace("%REPLANG%", esc(replyLang));
  h.replace("%PROMPT%", esc(systemPrompt));

  h.replace("%BRIGHT%", String(displayBrightness));
  h.replace("%NIGHT_BRIGHT%", String(nightBrightness));
  h.replace("%NIGHT_SEL_0%", nightMode == 0 ? "active" : "");
  h.replace("%NIGHT_SEL_1%", nightMode == 1 ? "active" : "");
  h.replace("%NIGHT_SEL_2%", nightMode == 2 ? "active" : "");
  h.replace("%NIGHT_CHECK_0%", nightMode == 0 ? "checked" : "");
  h.replace("%NIGHT_CHECK_1%", nightMode == 1 ? "checked" : "");
  h.replace("%NIGHT_CHECK_2%", nightMode == 2 ? "checked" : "");
  h.replace("%NIGHT_SLIDER_STYLE%", nightMode == 2 ? "opacity:0.25;pointer-events:none" : "");
  h.replace("%CIRC_START_OPTIONS%", makeHourOptions(circadianSleepStart));
  h.replace("%CIRC_END_OPTIONS%", makeHourOptions(circadianSleepEnd));

  h.replace("%CIRC_EN_CHECKED%", circadianSleepEnabled ? "checked" : "");
  h.replace("%CIRC_EN_VAL%", circadianSleepEnabled ? "1" : "0");
  h.replace("%CIRC_START%", String(circadianSleepStart));
  h.replace("%CIRC_END%", String(circadianSleepEnd));
  h.replace("%BOREDOM_CHECKED%", boredomEnabled ? "checked" : "");
  h.replace("%BM_1_SEL%", boredomMinutes == 1 ? "selected" : "");
  h.replace("%BM_2_SEL%", boredomMinutes == 2 ? "selected" : "");
  h.replace("%BM_3_SEL%", boredomMinutes == 3 ? "selected" : "");
  h.replace("%BM_5_SEL%", boredomMinutes == 5 ? "selected" : "");
  h.replace("%BM_10_SEL%", boredomMinutes == 10 ? "selected" : "");
  h.replace("%BM_15_SEL%", boredomMinutes == 15 ? "selected" : "");
  h.replace("%BM_30_SEL%", boredomMinutes == 30 ? "selected" : "");
  h.replace("%TCH_SND_CHECKED%", touchFeedbackSound ? "checked" : "");
  h.replace("%TCH_DEB%", String(touchDebounceMs));

  h.replace("%ACT_TOP_TAP%", touchActionToString(touchActTopTap));
  h.replace("%ACT_TOP_DBL%", touchActionToString(touchActTopDbl));
  h.replace("%ACT_LT_TAP%", touchActionToString(touchActLeftTap));
  h.replace("%ACT_LT_DBL%", touchActionToString(touchActLeftDbl));
  h.replace("%ACT_RT_TAP%", touchActionToString(touchActRightTap));
  h.replace("%ACT_RT_DBL%", touchActionToString(touchActRightDbl));
  h.replace("%ACT_LT_HLD%", touchActionToString(touchActLeftHold));
  h.replace("%ACT_RT_HLD%", touchActionToString(touchActRightHold));

  h.replace("%WTHR_CITY%", esc(weatherCity));
  h.replace("%WTHR_LAT%", esc(weatherLat));
  h.replace("%WTHR_LON%", esc(weatherLon));

  h.replace("%SKIN_DEFAULT_SEL%", currentEyeSkin == SKIN_DEFAULT ? "selected" : "");
  h.replace("%SKIN_CYBER_SEL%", currentEyeSkin == SKIN_CYBERPUNK ? "selected" : "");
  h.replace("%SKIN_CAT_SEL%", currentEyeSkin == SKIN_CAT ? "selected" : "");
  h.replace("%SKIN_RETRO_SEL%", currentEyeSkin == SKIN_RETRO8BIT ? "selected" : "");
  h.replace("%SKIN_ANIME_SEL%", currentEyeSkin == SKIN_ANIME ? "selected" : "");
  h.replace("%SKIN_HEARTS_SEL%", currentEyeSkin == SKIN_HEARTS ? "selected" : "");
  h.replace("%SKIN_STEAM_SEL%", currentEyeSkin == SKIN_STEAMPUNK ? "selected" : "");
  h.replace("%SKIN_DEMON_SEL%", currentEyeSkin == SKIN_DEMON ? "selected" : "");
  h.replace("%SKIN_MATRIX_SEL%", currentEyeSkin == SKIN_MATRIX ? "selected" : "");
  h.replace("%SKIN_CYBORG_SEL%", currentEyeSkin == SKIN_CYBORG ? "selected" : "");
  h.replace("%SKIN_MECHA_SEL%", currentEyeSkin == SKIN_MECHA ? "selected" : "");
  h.replace("%SKIN_SHADES_SEL%", currentEyeSkin == SKIN_SUNGLASSES ? "selected" : "");
  h.replace("%SKIN_PUPPY_SEL%", currentEyeSkin == SKIN_PUPPY ? "selected" : "");
  h.replace("%SKIN_HYPNO_SEL%", currentEyeSkin == SKIN_HYPNO ? "selected" : "");
  h.replace("%SKIN_ALIEN_SEL%", currentEyeSkin == SKIN_ALIEN ? "selected" : "");
  h.replace("%SKIN_NINJA_SEL%", currentEyeSkin == SKIN_NINJA ? "selected" : "");
  h.replace("%SKIN_ROUND_SEL%", currentEyeSkin == SKIN_ROUND ? "selected" : "");
  h.replace("%SKIN_KIWI_SEL%", currentEyeSkin == SKIN_KIWI ? "selected" : "");

  h.replace("%MEM_NAME%", esc(getOwnerName()));

  String memListHtml;
  memListHtml.reserve(1000);
  if (kiwiMemory.memoryCount == 0) {
    memListHtml = "<div style=\"font-size:12px;color:var(--tertiary);font-style:italic\">No facts learned yet. Tell Kiwi your name or chat!</div>";
  } else {
    memListHtml = "<ul style=\"margin:0;padding-left:18px;font-size:12px;line-height:1.6;color:var(--text)\">";
    for (int i = 0; i < kiwiMemory.memoryCount; i++) {
      memListHtml += "<li>" + esc(kiwiMemory.memories[i]) + "</li>";
    }
    memListHtml += "</ul>";
  }
  h.replace("%MEM_LIST_HTML%", memListHtml);

  return h;
}
static const char DASHBOARD_FOOT[] PROGMEM = R"rawfoot(<script>
    /* ── Starfield (paused when hidden, static under reduced motion) ── */
    (function () {
      var c = document.getElementById('sky'), x = c.getContext('2d'), stars = [], W, H, raf;
      var reduce = window.matchMedia('(prefers-reduced-motion: reduce)').matches;
      function size() {
        W = c.width = innerWidth; H = c.height = innerHeight; stars = [];
        var n = Math.min(220, Math.round(W * H / 9000));
        for (var i = 0; i < n; i++)stars.push({ x: Math.random() * W, y: Math.random() * H, r: Math.random() * 1.2 + .2, s: Math.random() * .15 + .02, p: Math.random() * 6.28 });
      }
      function draw(t) {
        x.clearRect(0, 0, W, H);
        for (var i = 0; i < stars.length; i++) {
          var s = stars[i];
          if (!reduce) { s.y += s.s; if (s.y > H) { s.y = 0; s.x = Math.random() * W; } }
          var a = .35 + .35 * Math.sin(t / 1200 + s.p);
          x.fillStyle = 'rgba(236,230,217,' + a.toFixed(3) + ')';
          x.beginPath(); x.arc(s.x, s.y, s.r, 0, 6.283); x.fill();
        }
        if (!reduce) raf = requestAnimationFrame(draw);
      }
      size(); window.addEventListener('resize', size);
      if (reduce) { draw(0); } else {
        raf = requestAnimationFrame(draw);
        document.addEventListener('visibilitychange', function () {
          if (document.hidden) { cancelAnimationFrame(raf); } else { raf = requestAnimationFrame(draw); }
        });
      }
    })();

    /* ── Clock ── */
    (function () {
      var el = document.getElementById('clock');
      function tick() { var d = new Date(); el.textContent = String(d.getHours()).padStart(2, '0') + ':' + String(d.getMinutes()).padStart(2, '0'); }
      tick(); setInterval(tick, 15000);
    })();

    /* ── Range fill helper for amber progress track ── */
    document.querySelectorAll('input[type=range]').forEach(function (r) {
      function fill() { r.style.setProperty('--p', ((r.value - r.min) / (r.max - r.min) * 100) + '%'); }
      r.addEventListener('input', fill); fill();
    });

    /* ── Tab switcher (Live vs Settings - Zero Scrolling) ── */
    function setTab(tab) {
      var isSettings = (tab === 'settings');
      var pLive = document.getElementById('pillLive');
      var pSet = document.getElementById('pillSettings');
      var liveSec = document.getElementById('liveSection');
      var setSec = document.getElementById('settingsSection');
      var mood = document.getElementById('moodLine');

      if (pLive) pLive.classList.toggle('on', !isSettings);
      if (pSet) pSet.classList.toggle('on', isSettings);

      if (liveSec) liveSec.style.display = isSettings ? 'none' : 'block';
      if (setSec) setSec.style.display = isSettings ? 'block' : 'none';
      if (mood) mood.textContent = isSettings ? 'Configuration & APIs' : 'System Console';
    }

    /* ── Toast ── */
    function toast(msg) {
      var t = document.getElementById('toast'); t.textContent = msg; t.style.opacity = '1'; t.style.transform = 'translate(-50%,0)';
      clearTimeout(toast._t); toast._t = setTimeout(function () { t.style.opacity = '0'; t.style.transform = 'translate(-50%,8px)'; }, 1800);
    }

    /* ── Live adjustment visual feedback (green tick badge) ── */
    function showLiveTick(tickId) {
      var el = document.getElementById(tickId);
      if (!el) return;
      el.classList.add('show');
      clearTimeout(el._t);
      el._t = setTimeout(function () {
        el.classList.remove('show');
      }, 1800);
    }

    /* ── Real-Time Hardware Adjustments (No Save/Reboot Needed) ── */
    var volDebounce = null;
    function onVolumeChangeLive(val) {
      var lbl = document.getElementById('volLabel');
      if (lbl) lbl.textContent = val;
      var hid = document.getElementById('hiddenFormVol');
      if (hid) hid.value = val;
      showLiveTick('tickAudio');
      clearTimeout(volDebounce);
      volDebounce = setTimeout(function () {
        fetch('/do?vol=' + encodeURIComponent(val)).catch(function () { });
      }, 100);
    }

    var brightDebounce = null;
    function onDayBrightChange(val) {
      var b = document.getElementById('dayBrightVal');
      if (b) b.textContent = val + '%';
      var hid = document.getElementById('hiddenFormBright');
      if (hid) hid.value = val;
      showLiveTick('tickBrightness');
      clearTimeout(brightDebounce);
      brightDebounce = setTimeout(function () {
        fetch('/do?brightness=' + encodeURIComponent(val)).catch(function () { });
      }, 100);
    }

    var nightBrightDebounce = null;
    function onNightBrightChange(val) {
      var b = document.getElementById('nightBrightVal');
      if (b) b.textContent = val + '%';
      var hid = document.getElementById('hiddenFormNightBright');
      if (hid) hid.value = val;
      showLiveTick('tickBrightness');
      clearTimeout(nightBrightDebounce);
      nightBrightDebounce = setTimeout(function () {
        fetch('/do?night_bright=' + encodeURIComponent(val)).catch(function () { });
      }, 100);
    }

    function onNightModeChange(mode) {
      var pills = document.querySelectorAll('.rocky-pill');
      pills.forEach(function (p, idx) {
        if (idx === mode) p.classList.add('active');
        else p.classList.remove('active');
      });
      var row = document.getElementById('nightSliderRow');
      if (row) {
        row.style.opacity = (mode === 2) ? '0.25' : '1.0';
        row.style.pointerEvents = (mode === 2) ? 'none' : 'auto';
      }
      var hid = document.getElementById('hiddenFormNightMode');
      if (hid) hid.value = mode;
      showLiveTick('tickBrightness');
      fetch('/do?night_mode=' + encodeURIComponent(mode)).catch(function () { });
    }

    function onCircadianToggle(en) {
      var s = document.getElementById('circStartSel');
      var e = document.getElementById('circEndSel');
      var sVal = s ? s.value : '23';
      var eVal = e ? e.value : '7';
      var row = document.getElementById('circTimeRow');
      if (row) {
        row.style.opacity = en ? '1.0' : '0.4';
        row.style.pointerEvents = en ? 'auto' : 'none';
      }
      var hc = document.getElementById('hiddenFormCircEn');
      if (hc) hc.value = en ? '1' : '0';
      showLiveTick('tickCircadian');
      fetch('/do?circ_en=' + (en ? '1' : '0') + '&circ_start=' + encodeURIComponent(sVal) + '&circ_end=' + encodeURIComponent(eVal)).catch(function () { });
    }

    function onScheduleChange() {
      var s = document.getElementById('circStartSel');
      var e = document.getElementById('circEndSel');
      var tog = document.getElementById('liveCircEnToggle');
      if (!s || !e) return;
      var hs = document.getElementById('hiddenFormCircStart');
      if (hs) hs.value = s.value;
      var he = document.getElementById('hiddenFormCircEnd');
      if (he) he.value = e.value;
      var en = (!tog || tog.checked) ? '1' : '0';
      showLiveTick('tickCircadian');
      fetch('/do?circ_en=' + en + '&circ_start=' + encodeURIComponent(s.value) + '&circ_end=' + encodeURIComponent(e.value)).catch(function () { });
    }

    function toggleManualSleep() {
      fetch('/do?sleep=toggle').then(function (r) { return r.json(); }).then(function (d) {
        if (d && typeof d.sleeping !== 'undefined') updateSleepUI(d.sleeping);
      }).catch(function () {
        pollStatus();
      });
    }

    function updateSleepUI(isSleeping) {
      var btn = document.getElementById('btnManualSleep');
      var ico = document.getElementById('manualSleepIco');
      var txt = document.getElementById('manualSleepTxt');
      var hint = document.getElementById('sleepStateHint');
      if (isSleeping) {
        if (ico) ico.textContent = '☀️';
        if (txt) txt.textContent = 'Wake Up Kiwi';
        if (btn) btn.classList.add('primary');
        if (hint) {
          hint.textContent = 'Current state: Robot is sleeping (Tap head or click to wake)';
          hint.style.color = 'var(--amber)';
        }
      } else {
        if (ico) ico.textContent = '😴';
        if (txt) txt.textContent = 'Put Kiwi to Sleep';
        if (btn) btn.classList.remove('primary');
        if (hint) {
          hint.textContent = 'Current state: Robot is awake and active';
          hint.style.color = 'var(--muted)';
        }
      }
    }

    function scanWifiNetworks() {
      var btn = document.getElementById('btnScanWifi');
      var txt = document.getElementById('scanBtnText');
      var spin = document.getElementById('scanSpinner');
      var sel = document.getElementById('wifiScanSelect');
      if (btn) btn.disabled = true;
      if (txt) txt.textContent = 'Scanning...';
      if (spin) spin.style.display = 'inline-block';
      if (sel) sel.innerHTML = '<option value="">Scanning surrounding 2.4GHz Wi-Fi networks...</option>';

      fetch('/scan')
        .then(function (r) { return r.json(); })
        .then(function (networks) {
          if (btn) btn.disabled = false;
          if (txt) txt.textContent = 'Scan Networks';
          if (spin) spin.style.display = 'none';
          if (!sel) return;

          if (!networks || networks.length === 0) {
            sel.innerHTML = '<option value="">No networks found (click Scan to retry)</option>';
            return;
          }

          var currentSSID = (document.getElementById('wifiSsidInput') || {}).value || '';
          var html = '<option value="">-- ' + networks.length + ' Networks Found (Select One) --</option>';

          networks.forEach(function (net) {
            var lock = net.secure ? '🔒' : '🔓';
            var signal = (net.rssi >= -60) ? '📶 4/4' : (net.rssi >= -70) ? '📶 3/4' : (net.rssi >= -80) ? '📶 2/4' : '📶 1/4';
            var isCurrent = (net.ssid === currentSSID);
            html += '<option value="' + encodeURIComponent(net.ssid) + '"' + (isCurrent ? ' selected' : '') + '>';
            html += net.ssid + ' (' + signal + ' ' + net.rssi + 'dBm ' + lock + ')';
            html += '</option>';
          });
          sel.innerHTML = html;
        })
        .catch(function (err) {
          if (btn) btn.disabled = false;
          if (txt) txt.textContent = 'Scan Networks';
          if (spin) spin.style.display = 'none';
          if (sel) sel.innerHTML = '<option value="">Scan failed (Click to retry)</option>';
        });
    }

    function onSelectScannedWifi(val) {
      if (!val) return;
      var ssid = decodeURIComponent(val);
      var inp = document.getElementById('wifiSsidInput');
      if (inp) inp.value = ssid;
      var passInp = document.getElementById('wifiPassInput');
      if (passInp) passInp.focus();
    }

    function togglePw(id, btn) {
      var el = document.getElementById(id);
      if (!el) return;
      if (el.type === 'password') {
        el.type = 'text';
        if (btn) btn.innerText = 'HIDE';
      } else {
        el.type = 'password';
        if (btn) btn.innerText = 'SHOW';
      }
    }
    function togglePassVisibility(btn) {
      var p = document.getElementById('wifiPassInput');
      if (!p) return;
      if (p.type === 'password') {
        p.type = 'text';
        if (btn) btn.innerText = 'HIDE';
      } else {
        p.type = 'password';
        if (btn) btn.innerText = 'SHOW';
      }
    }


    function toggleSkinLive(v) {
      fetch('/do?skin=' + encodeURIComponent(v)).catch(function () { });
    }
    function toggleBoredomLive(val) {
      var hid = document.getElementById('hiddenFormBoredom');
      if (hid) hid.value = val ? '1' : '0';
      showLiveTick('tickAudio');
      fetch('/do?boredom=' + (val ? '1' : '0')).catch(function () { });
    }
    function setBoredomIntervalLive(val) {
      var hid = document.getElementById('hiddenFormBoredomMins');
      if (hid) hid.value = val;
      showLiveTick('tickAudio');
      fetch('/do?boredom_mins=' + encodeURIComponent(val)).catch(function () { });
    }
    function clearMemories() {
      if (confirm("Are you sure you want to clear all Kiwi's memories?")) {
        fetch('/do?clearmem=1').then(function () { alert("All memories cleared!"); location.reload(); });
      }
    }
    function toggleModels() {
      var p = document.getElementById('provSel').value;
      document.getElementById('groqBox').style.display = (p === 'groq') ? 'block' : 'none';
      document.getElementById('geminiBox').style.display = (p === 'gemini') ? 'block' : 'none';
      document.getElementById('openaiBox').style.display = (p === 'openai') ? 'block' : 'none';
    }
    function updateGeminiUrl() {
      var m = document.getElementById('geminiModelSel').value;
      var u = document.getElementById('geminiUrlIn');
      if (u && (u.value.includes('generativelanguage.googleapis.com') || u.value === '')) {
        u.value = 'https://generativelanguage.googleapis.com/v1beta/models/' + m + ':generateContent';
      }
    }
    function updateOpenAiUrl() {
      var u = document.getElementById('openaiUrlIn');
      if (u && (u.value.includes('api.openai.com') || u.value === '')) {
        u.value = 'https://api.openai.com/v1/chat/completions';
      }
    }
    function toggleStt() {
      var p = document.getElementById('sttProvSel').value;
      document.getElementById('sarvamBox').style.display = (p === 'sarvam') ? 'block' : 'none';
      document.getElementById('groqSttBox').style.display = (p === 'groq') ? 'block' : 'none';
      document.getElementById('openaiSttBox').style.display = (p === 'openai') ? 'block' : 'none';
    }
    function toggleTts() {
      var p = document.getElementById('ttsProvSel').value;
      document.getElementById('ttsSarvamBox').style.display = (p === 'sarvam') ? 'block' : 'none';
      document.getElementById('ttsOpenaiBox').style.display = (p === 'openai') ? 'block' : 'none';
      document.getElementById('ttsElevenBox').style.display = (p === 'elevenlabs') ? 'block' : 'none';
      document.getElementById('ttsFishBox').style.display = (p === 'fish' || p === 'fishaudio') ? 'block' : 'none';
    }
    toggleModels(); toggleStt(); toggleTts();

    /* ── Terminal & live console ── */
    var autoScroll = true;
    var lastLogCount = 0;
    var chkAuto = document.getElementById('autoScrollCheck');
    if (chkAuto) chkAuto.addEventListener('change', function () { autoScroll = this.checked; });

    var btnClear = document.getElementById('btnClearLog');
    if (btnClear) {
      btnClear.addEventListener('click', function () {
        fetch('/do?clearlog=1').then(function () {
          var t = document.getElementById('serialTerminal');
          if (t) t.innerHTML = '';
          lastLogCount = 0;
        }).catch(function () { });
      });
    }

    var btnMic = document.getElementById('btnWebMic');
    if (btnMic) {
      btnMic.addEventListener('click', function () {
        fetch('/do?record=1').then(function (r) { return r.json(); }).then(function (j) {
          if (!j.ok && j.reason === 'busy') alert('Kiwi is busy processing audio/AI.');
        }).catch(function () { });
      });
    }

    var btnSend = document.getElementById('btnSendPrompt');
    var promptIn = document.getElementById('textPromptIn');
    if (btnSend && promptIn) {
      btnSend.addEventListener('click', sendPrompt);
      promptIn.addEventListener('keypress', function (e) {
        if (e.key === 'Enter') { e.preventDefault(); sendPrompt(); }
      });
    }

    function sendPrompt() {
      var inp = document.getElementById('textPromptIn');
      var txt = inp.value.trim();
      if (!txt) return;
      fetch('/do?ask=' + encodeURIComponent(txt))
        .then(function (r) { return r.json(); })
        .then(function (j) {
          if (j.ok) inp.value = '';
          else if (j.reason === 'busy') alert('Kiwi is currently busy!');
        }).catch(function () { });
    }

    function pollConsole() {
      fetch('/status', { cache: 'no-store' }).then(function (r) { return r.json(); }).then(function (j) {
        var mic = document.getElementById('btnWebMic');
        var badgeDot = document.getElementById('liveStateDot');
        var badgeTxt = document.getElementById('liveStateText');
        var micSub = document.getElementById('micBtnSub');
        var mood = document.getElementById('moodLine');

        var state = j.state || (j.busy ? 'Busy Processing' : 'Ready');
        if (badgeTxt) badgeTxt.textContent = state;
        if (mood) mood.textContent = state;

        if (state.indexOf('Listening') >= 0 || state.indexOf('Recording') >= 0) {
          if (mic) mic.classList.add('recording');
          if (badgeDot) badgeDot.className = 'dot rec';
          if (micSub) micSub.textContent = 'Listening to microphone now...';
        } else if (j.busy) {
          if (mic) mic.classList.remove('recording');
          if (badgeDot) badgeDot.className = 'dot busy';
          if (micSub) micSub.textContent = 'AI processing request...';
        } else if (j.sleeping) {
          if (mic) mic.classList.remove('recording');
          if (badgeDot) badgeDot.className = 'dot sleep';
          if (micSub) micSub.textContent = 'Robot sleeping · Tap mic or head to wake';
        } else {
          if (mic) mic.classList.remove('recording');
          if (badgeDot) badgeDot.className = 'dot';
          if (micSub) micSub.textContent = 'Stops shortly after you finish speaking';
        }

        updateSleepUI(Boolean(j.sleeping));

        if (j.question) {
          var qEl = document.getElementById('boxQuestion');
          if (qEl) qEl.textContent = j.question;
        }
        if (j.answer) {
          var aEl = document.getElementById('boxAnswer');
          if (aEl) aEl.textContent = j.answer;
        }

        if (j.logs && Array.isArray(j.logs)) {
          var term = document.getElementById('serialTerminal');
          if (term && (j.logs.length !== lastLogCount || term.innerHTML === '')) {
            var h = '';
            for (var i = 0; i < j.logs.length; i++) {
              var line = j.logs[i];
              var col = '#8fd19e';
              if (line.indexOf('[REC]') >= 0) col = '#ff4a3d';
              else if (line.indexOf('[STT]') >= 0) col = '#38bdf8';
              else if (line.indexOf('[LLM]') >= 0) col = '#f2b45c';
              else if (line.indexOf('[TTS]') >= 0) col = '#c084fc';
              else if (line.indexOf('Error') >= 0 || line.indexOf('fail') >= 0 || line.indexOf('Failed') >= 0) col = '#ef4444';
              else if (line.indexOf('[CMD]') >= 0) col = '#a3e635';
              h += '<div style="color:' + col + '">' + line + '</div>';
            }
            term.innerHTML = h;
            lastLogCount = j.logs.length;
            if (autoScroll) term.scrollTop = term.scrollHeight;
          }
        }
      }).catch(function () { });
    }
    setInterval(pollConsole, 900);
    pollConsole();

    /* ── Touch action options ── */
    var touchActs = [
      { val: "listen", lbl: "Voice Prompt / Mic Record" },
      { val: "clock", lbl: "Show Clock Screen" },
      { val: "weather", lbl: "Show Weather Screen" },
      { val: "skin", lbl: "Cycle OLED Eye Skin" },
      { val: "pet", lbl: "Pet Reaction / Cuddle" },
      { val: "tickle", lbl: "Tickle Laugh Mode" },
      { val: "vol_up", lbl: "Volume Up (+10%)" },
      { val: "vol_dn", lbl: "Volume Down (-10%)" },
      { val: "dismiss", lbl: "Look Around / Dismiss" },
      { val: "none", lbl: "Disabled" }
    ];
    document.querySelectorAll('.touch-act-sel').forEach(function (sel) {
      var cur = sel.getAttribute('data-val');
      touchActs.forEach(function (a) {
        var opt = document.createElement('option');
        opt.value = a.val;
        opt.textContent = a.lbl;
        if (a.val === cur) opt.selected = true;
        sel.appendChild(opt);
      });
    });
  </script>
</body>

</html>

)rawfoot";

void handleRoot() {
  server.sendHeader("Cache-Control", "no-cache, no-store, must-revalidate");
  server.sendHeader("Pragma", "no-cache");
  server.sendHeader("Expires", "0");
  server.setContentLength(CONTENT_LENGTH_UNKNOWN);
  server.send(200, "text/html; charset=utf-8", "");

  // 1. Static HTML head, styles & navbar (0 bytes RAM heap)
  server.sendContent_P(DASHBOARD_CSS);

  // 2. Small dynamic status summary pill (< 1 KB RAM heap)
  String statusHtml = getDashboardStatusHtml();
  server.sendContent(statusHtml);
  statusHtml = "";

  // 3. Live Console and Real-Time Hardware Adjustments
  String liveHtml = getDashboardLiveConsole();
  server.sendContent(liveHtml);
  liveHtml = "";

  // 4. Form Part 1: Wi-Fi, STT, LLM (~6 KB RAM heap)
  String body1 = getDashboardBodyPart1();
  server.sendContent(body1);
  body1 = "";

  // 5. Form Part 2: TTS, Audio, Touch, Weather, Memory (~11 KB RAM heap)
  String body2 = getDashboardBodyPart2();
  server.sendContent(body2);
  body2 = "";

  // 6. Scripts & Footer (0 bytes RAM heap)
  server.sendContent_P(DASHBOARD_FOOT);
  server.sendContent("");
}
