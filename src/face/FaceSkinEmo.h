#pragma once
#include <Arduino.h>

/**
 * EMO-style Face Theme:
 * - Large, rounded-rectangular eyes with a prominent corner radius (no pupils).
 * - Procedural geometry: width, height, corner radius, eyelids (lidIn, lidOut),
 *   cheeks (cheek), asymmetric scaling (curious/thinking), and procedural heart eyes.
 * - Solid glowing aesthetic tailored for 128x64 SSD1306 OLED without flickering.
 */
void drawEmoEye(int cx, int cy, int w, int h, int r, int lidIn, int lidOut,
                int cheek, bool isLeft, bool glint);
