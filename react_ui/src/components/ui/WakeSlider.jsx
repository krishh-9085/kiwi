import React, { useCallback, useEffect, useLayoutEffect, useMemo, useRef, useState } from 'react';

import './WakeSlider.css';

const SETTLE = 9.23;
const FULL_SPEED = 320;
const MIN_REACH = 1.5;
const FLAT = 0.002;

const clamp = (v, lo, hi) => Math.min(hi, Math.max(lo, v));
const smoothstep = s => s * s * (3 - 2 * s);
const toPct = (v, min, max) => (max > min ? ((v - min) / (max - min)) * 100 : 0);
const snap = (v, min, max, step) => {
  if (!(max > min)) return min;
  if (!(step > 0)) return clamp(v, min, max);
  const lastWhole = min + Math.floor(+((max - min) / step).toFixed(6)) * step;
  const grid = clamp(Math.round((v - min) / step) * step + min, min, lastWhole);
  return +(lastWhole < max && Math.abs(v - max) <= Math.abs(v - grid) ? max : grid).toFixed(6);
};

export default function WakeSlider({
  value: valueProp,
  defaultValue = 50,
  onChange,
  onCommit,
  min = 0,
  max = 100,
  step = 5,
  bars = 28,
  height = 42,
  restHeight = 14,
  gap = 3,
  fillColor = 'var(--slider-fill)',
  trackColor = 'var(--slider-track)',
  crestColor = '',
  sensitivity = 1,
  reach = 6,
  skew = 0.6,
  glide = 0.3,
  smoothing = 100,
  showValue = false,
  formatValue,
  disabled = false,
  ariaLabel = 'Value',
  className = ''
}) {
  const [inner, setInner] = useState(defaultValue);
  const value = clamp(valueProp ?? inner, min, max);
  const pct = toPct(value, min, max);
  const [reduce, setReduce] = useState(() =>
    typeof window !== 'undefined' && window.matchMedia
      ? window.matchMedia('(prefers-reduced-motion: reduce)').matches
      : false
  );
  const format = formatValue ?? String;
  const rest = Math.min(restHeight, height - 1) / height;

  const rootRef = useRef(null);
  const trackRef = useRef(null);
  const handleRef = useRef(null);
  const barEls = useRef([]);
  const crestEls = useRef([]);
  const pointerId = useRef(null);
  const lastAmp = useRef(0);
  const latest = useRef(value);
  latest.current = value;

  const springRef = useRef({
    target: pct,
    head: pct,
    headVel: 0,
    speed: 0,
    speedVel: 0,
    rafId: 0,
    lastT: 0
  });

  useEffect(() => {
    if (typeof window === 'undefined' || !window.matchMedia) return;
    const mq = window.matchMedia('(prefers-reduced-motion: reduce)');
    const onChangeMq = (e) => setReduce(e.matches);
    mq.addEventListener?.('change', onChangeMq);
    return () => mq.removeEventListener?.('change', onChangeMq);
  }, []);

  const paint = (force = false) => {
    const st = springRef.current;
    const rawVal = reduce ? st.target : st.head;
    const curVal = clamp(Number.isFinite(rawVal) ? rawVal : st.target, 0, 100);
    const h = (curVal / 100) * (bars - 1);
    const v = reduce || !Number.isFinite(st.speed) ? 0 : st.speed;
    const amp = smoothstep(clamp((Math.abs(v) * sensitivity) / FULL_SPEED, 0, 1));
    const dir = Math.sign(v) || 1;
    const r = MIN_REACH + (reach - MIN_REACH) * amp;
    const behind = r * (1 + skew);
    const ahead = r * (1 - 0.5 * skew);
    const lit = curVal <= 0 ? -1 : Math.round(h);
    const flat = !force && amp < FLAT && lastAmp.current < FLAT;
    for (let i = 0; i < bars; i++) {
      const el = barEls.current[i];
      if (!el) continue;
      const on = i <= lit ? 'true' : 'false';
      if (el.dataset.on !== on) el.dataset.on = on;
      if (flat) continue;
      const d = i - h;
      const R = d * dir < 0 ? behind : ahead;
      const lift = Math.abs(d) < R ? amp * Math.cos((Math.PI * d) / (2 * R)) ** 2 : 0;
      el.style.transform = `scaleY(${rest + lift * (1 - rest)})`;
      const crest = crestEls.current[i];
      if (crest) crest.style.opacity = String(lift);
    }
    lastAmp.current = amp;
  };
  const paintRef = useRef(paint);
  paintRef.current = paint;

  const stopAnim = useCallback(() => {
    const st = springRef.current;
    if (st.rafId) {
      cancelAnimationFrame(st.rafId);
      st.rafId = 0;
    }
  }, []);

  const startAnim = useCallback(() => {
    const st = springRef.current;
    if (reduce) {
      st.head = st.target;
      st.headVel = 0;
      st.speed = 0;
      st.speedVel = 0;
      paintRef.current(true);
      return;
    }
    if (st.rafId) return;
    st.lastT = performance.now();
    const w0 = SETTLE / glide;
    const k0 = w0 * w0;
    const c0 = 2 * w0;
    const wv = 2000 / smoothing;
    const kv = wv * wv;
    const cv = 2.5 * wv;

    const stepFrame = (now) => {
      let rem = Math.min(0.05, Math.max(0.001, (now - st.lastT) / 1000));
      st.lastT = now;

      // Fixed <= 4ms physics sub-stepping keeps forward Euler unconditionally stable across any frame rate
      while (rem > 0) {
        const dt = Math.min(0.004, rem);
        const a0 = k0 * (st.target - st.head) - c0 * st.headVel;
        st.headVel += a0 * dt;
        st.head += st.headVel * dt;

        const av = kv * (st.headVel - st.speed) - cv * st.speedVel;
        st.speedVel += av * dt;
        st.speed += st.speedVel * dt;
        rem -= dt;
      }

      if (!Number.isFinite(st.head) || !Number.isFinite(st.headVel) || !Number.isFinite(st.speed)) {
        st.head = st.target;
        st.headVel = 0;
        st.speed = 0;
        st.speedVel = 0;
      }

      paintRef.current(false);

      const settled =
        Math.abs(st.target - st.head) < 0.01 &&
        Math.abs(st.headVel) < 0.05 &&
        Math.abs(st.speed) < 0.5 &&
        Math.abs(st.speedVel) < 1;

      if (settled) {
        st.head = st.target;
        st.headVel = 0;
        st.speed = 0;
        st.speedVel = 0;
        st.rafId = 0;
        paintRef.current(true);
      } else {
        st.rafId = requestAnimationFrame(stepFrame);
      }
    };
    st.rafId = requestAnimationFrame(stepFrame);
  }, [glide, smoothing, reduce]);

  useEffect(() => {
    const st = springRef.current;
    st.target = pct;
    if (Math.abs(st.target - st.head) < 0.01 && Math.abs(st.headVel) < 0.05 && !st.rafId) {
      st.head = pct;
      paintRef.current(true);
      return;
    }
    startAnim();
  }, [pct, startAnim]);

  useEffect(() => stopAnim, [stopAnim]);

  useLayoutEffect(() => {
    paintRef.current(true);
  });

  const commit = next => {
    const clean = snap(next, min, max, step);
    if (clean === latest.current) return;
    latest.current = clean;
    if (valueProp === undefined) setInner(clean);
    onChange?.(clean);
  };

  const commitFromX = x => {
    const track = trackRef.current;
    if (!track) return;
    const rect = track.getBoundingClientRect();
    if (!rect.width) return;
    let ratio = clamp((x - rect.left) / rect.width, 0, 1);
    if (getComputedStyle(track).direction === 'rtl') ratio = 1 - ratio;
    const rawVal = min + ratio * (max - min);
    const cleanVal = snap(rawVal, min, max, step);
    const p = toPct(cleanVal, min, max);
    springRef.current.target = p;
    startAnim();
    commit(cleanVal);
  };

  const onPointerDown = e => {
    if (disabled || pointerId.current !== null) return;
    pointerId.current = e.pointerId;
    try {
      e.currentTarget.setPointerCapture(e.pointerId);
    } catch {}
    rootRef.current?.removeAttribute('data-instant');
    handleRef.current?.focus({ preventScroll: true });
    commitFromX(e.clientX);
  };

  const onPointerMove = e => {
    if (e.pointerId === pointerId.current) commitFromX(e.clientX);
  };

  const endDrag = e => {
    if (e.pointerId !== pointerId.current) return;
    try {
      e.currentTarget.releasePointerCapture(e.pointerId);
    } catch {}
    pointerId.current = null;
    onCommit?.(latest.current);
  };

  const onKeyDown = e => {
    if (disabled) return;
    const jumps = {
      ArrowRight: value + step,
      ArrowUp: value + step,
      ArrowLeft: value - step,
      ArrowDown: value - step,
      PageUp: value + step * 10,
      PageDown: value - step * 10,
      Home: min,
      End: max
    };
    if (!(e.key in jumps)) return;
    e.preventDefault();
    const clean = snap(jumps[e.key], min, max, step);
    const p = toPct(clean, min, max);
    rootRef.current?.setAttribute('data-instant', 'true');
    stopAnim();
    const st = springRef.current;
    st.head = p;
    st.headVel = 0;
    st.speed = 0;
    st.speedVel = 0;
    st.target = p;
    paintRef.current(true);
    commit(clean);
    onCommit?.(clean);
  };

  const barNodes = useMemo(() => {
    barEls.current.length = bars;
    crestEls.current.length = bars;
    return Array.from({ length: bars }, (_, i) => (
      <span
        key={i}
        ref={el => {
          barEls.current[i] = el;
        }}
        className="wake-slider__bar"
      >
        {crestColor ? (
          <span
            ref={el => {
              crestEls.current[i] = el;
            }}
            className="wake-slider__crest"
          />
        ) : null}
      </span>
    ));
  }, [bars, crestColor]);

  return (
    <div
      ref={rootRef}
      className={`wake-slider${className ? ` ${className}` : ''}`}
      aria-disabled={disabled || undefined}
      style={{
        '--ws-fill': fillColor,
        '--ws-track': trackColor,
        '--ws-crest': crestColor || fillColor,
        '--ws-height': `${height}px`,
        '--ws-gap': `${gap}px`,
        '--ws-rest': rest
      }}
    >
      <div
        ref={trackRef}
        className="wake-slider__track"
        onPointerDown={onPointerDown}
        onPointerMove={onPointerMove}
        onPointerUp={endDrag}
        onPointerCancel={endDrag}
        onLostPointerCapture={endDrag}
      >
        {barNodes}
        <button
          ref={handleRef}
          type="button"
          role="slider"
          className="wake-slider__handle"
          tabIndex={disabled ? -1 : 0}
          aria-label={ariaLabel}
          aria-valuemin={min}
          aria-valuemax={max}
          aria-valuenow={value}
          aria-valuetext={format(value)}
          aria-disabled={disabled || undefined}
          onKeyDown={onKeyDown}
        />
      </div>
      {showValue ? (
        <span className="wake-slider__value" aria-hidden="true">
          {format(value)}
        </span>
      ) : null}
    </div>
  );
}
