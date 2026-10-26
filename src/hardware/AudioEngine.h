#pragma once
#include "core/CoreTypes.h"
#include <Arduino.h>
#include <driver/i2s_std.h>

extern i2s_chan_handle_t rxHandle;
extern i2s_chan_handle_t txHandle;

// ---------- Hardware & Driver ----------
bool startAudio();
void drainMic();
void detectMicSlot();

// ---------- Speaker Output & DSP ----------
bool spkWriteAll(const void *data, size_t bytes);
void spkFlush();
void spkPushSample(int16_t s);
void makeWavHeader(uint8_t *h, uint32_t dataSize);

// ---------- Tone Synthesis Engine ----------
void playToneQueue(Tone *tones, int count);
void updateTonePlayer();

// ---------- Robotic Sound Effects ----------
void soundPetLove();
void soundPetGiggle();
void soundPetShy();
void soundPetSparkle();
void soundPetKiss();
void soundPetCuddle();
void soundPetPlayful();
void soundPetSleepy();
void soundPetProud();
void soundPetSurprised();
void playBootChime();
void soundError();
void soundClock();
void soundListening();

// ---------- Interactive Soundboard FX ----------
void soundFart();
void soundLaser();
void soundFanfare();
void soundCoin();
void soundSadViolin();
