#include "services/WledService.h"
#include <ESPmDNS.h>
#include <HTTPClient.h>
#include <WiFi.h>
#include "config/AppConfig.h"

static String discoveredWledIp = "";
static String manualWledIp = "";
static uint32_t lastDiscoveryAttempt = 0;
static String wledEffectsJson = "";

struct WledStaticEffect {
  const char *name;
  int id;
};

// Standard WLED effect IDs fallback table (covers all 160+ effects)
static const WledStaticEffect STATIC_EFFECTS[] = {
  {"Solid", 0}, {"Blink", 1}, {"Breathe", 2}, {"Wipe", 3}, {"Wipe Random", 4},
  {"Random Colors", 5}, {"Sweep", 6}, {"Dynamic", 7}, {"Colorloop", 8}, {"Rainbow", 9},
  {"Scan", 10}, {"Dual Scan", 11}, {"Fade", 12}, {"Chase", 13}, {"Chase Rainbow", 14},
  {"Running", 15}, {"Saw", 16}, {"Twinkle", 17}, {"Dissolve", 18}, {"Dissolve Rnd", 19},
  {"Sparkle", 20}, {"Dark Sparkle", 21}, {"Sparkle+", 22}, {"Strobe", 23}, {"Strobe Rainbow", 24},
  {"Mega Strobe", 25}, {"Blink Rainbow", 26}, {"Android", 27}, {"Chase Flash", 28},
  {"Chase Flash Rnd", 29}, {"Rainbow Runner", 30}, {"Candle", 35}, {"Fireworks", 36},
  {"Fireworks 1D", 37}, {"Aurora", 38}, {"Fire 2012", 42}, {"Colorwaves", 43},
  {"Bpm", 44}, {"Fill Noise", 45}, {"Noise 1", 46}, {"Noise 2", 47}, {"Noise 3", 48},
  {"Noise 4", 49}, {"Colortwinkles", 50}, {"Lake", 51}, {"Meteor", 52}, {"Railway", 54},
  {"Ripple", 55}, {"Lighthouse", 57}, {"Heartbeat", 62}, {"PacMan", 65}, {"Percent", 66},
  {"Ripple Rainbow", 67}, {"TV Simulator", 70}, {"Twinklefox", 71}, {"Twinklecat", 72},
  {"Halloween Eyes", 73}, {"Twinkle", 74}, {"Pacifica", 75}, {"Candle Multi", 76},
  {"Solid Glitter", 77}, {"Sunrise", 78}, {"Phased", 79}, {"Oscillate", 80},
  {"Lightning", 81}, {"Drip", 82}, {"Plasma", 83}, {"Fireworks Starburst", 84},
  {"Popcorn", 85}, {"Fire Flicker", 86}, {"Rain", 88}, {"Tetrix", 89},
  {"ICU", 91}, {"Multi Comet", 92}, {"Stream", 93}, {"Glitter", 94},
  {"Washing Machine", 95}, {"Sinelon", 97}, {"Spots", 98}, {"Stream 2", 100},
  {"Police", 101}, {"Two Dots", 102}, {"Fairytwinkle", 103}, {"Fairy", 104},
  {"Waterfall", 105}, {"Blends", 115}, {"Flow", 116}, {"Chunchun", 117},
  {"Dancing Shadows", 118}, {"DJ Light", 119}, {"Rocktaves", 120}, {"Freqwave", 121},
  {"Freqmatrix", 122}, {"Freqpixels", 123}, {"Freqmap", 124}, {"Noisefire", 125},
  {"Noisemove", 126}, {"Noisemeter", 127}, {"Gravcenter", 128}, {"Gravcentric", 129},
  {"Gravfreq", 130}, {"Gravimeter", 131}, {"Matripix", 132}, {"Midnoise", 133},
  {"Pixelwave", 134}, {"Plasmoid", 135}, {"Puddles", 136}, {"Puddlepeak", 137},
  {"Ripple Peak", 138}, {"Rolling Balls", 139}, {"Shimmer", 140}, {"Sine", 141},
  {"Slow Transition", 142}, {"Spots Fade", 143}, {"Traffic Light", 144}, {"Tri Fade", 145},
  {"Tri Wipe", 146}, {"Twinkleup", 147}, {"Wavesins", 148}, {"Noise Pal", 149},
  {"Perlin Move", 150}, {"Phased Noise", 151}, {"Pixels", 152}, {"Color Clouds", 153},
  {"Colorful", 154}, {"Dynamic Smooth", 155}, {"Flow Stripe", 156}, {"Loading", 157},
  {"PS Fire 1D", 158}, {"PS Fireworks 1D", 159}, {"PS GEQ 1D", 160}, {"PS Hourglass", 161},
  {"PS Pinball", 162}, {"PS Sonic Boom", 163}, {"PS Sonic Stream", 164}, {"PS Sparkler", 165},
  {"PS Spray 1D", 166}, {"PS Springy", 167}, {"PS Starburst", 168}, {"PS 1D Balance", 169},
  {"PS Chase", 170}, {"PS Dancing Shadows", 171}, {"PS DripDrop", 172},
  {"Bouncing Balls", 173}, {"Blurz", 174},
  {nullptr, -1}
};

bool isWledDiscovered() {
  return discoveredWledIp.length() > 0 || manualWledIp.length() > 0;
}

String getWledIp() {
  if (manualWledIp.length() > 0)
    return manualWledIp;
  return discoveredWledIp;
}

void setWledManualIp(const String &ip) {
  manualWledIp = ip;
  manualWledIp.trim();
  if (manualWledIp.length() > 0) {
    discoveredWledIp = manualWledIp;
    Serial.printf("[WLED] Manual IP configured: %s\n", manualWledIp.c_str());
  }
}

bool fetchWledEffectsList() {
  if (WiFi.status() != WL_CONNECTED)
    return false;
  String ip = getWledIp();
  if (!ip.length())
    return false;

  HTTPClient http;
  WiFiClient client;
  http.setTimeout(1800);
  String url = "http://" + ip + "/json/eff";
  if (!http.begin(client, url))
    return false;

  int code = http.GET();
  if (code == 200) {
    wledEffectsJson = http.getString();
    http.end();
    Serial.printf("[WLED] Synced %d bytes of effects from %s\n", (int)wledEffectsJson.length(), ip.c_str());
    return true;
  }
  http.end();
  return false;
}

bool discoverWled(bool force) {
  if (WiFi.status() != WL_CONNECTED)
    return false;

  if (manualWledIp.length() > 0) {
    discoveredWledIp = manualWledIp;
    if (!wledEffectsJson.length()) fetchWledEffectsList();
    return true;
  }

  if (discoveredWledIp.length() > 0 && !force)
    return true;

  // Rate-limit discovery attempts to avoid network congestion
  if (!force && millis() - lastDiscoveryAttempt < 8000)
    return false;
  lastDiscoveryAttempt = millis();

  Serial.println("[WLED] Auto-discovering WLED on local network via mDNS...");

  // 1. Try discovering via standard WLED mDNS service '_wled._tcp'
  int n = MDNS.queryService("wled", "tcp");
  if (n > 0) {
    discoveredWledIp = MDNS.address(0).toString();
    String host = MDNS.hostname(0);
    Serial.printf("[WLED] Found WLED service: %s (%s)\n", discoveredWledIp.c_str(), host.c_str());
    addTerminalLog("[WLED] Light found at " + discoveredWledIp);
    fetchWledEffectsList();
    return true;
  }

  // 2. Fallback: Query default hostname 'wled.local'
  IPAddress hostIp = MDNS.queryHost("wled");
  if (hostIp != INADDR_NONE && hostIp != IPAddress(0, 0, 0, 0)) {
    discoveredWledIp = hostIp.toString();
    Serial.printf("[WLED] Resolved host 'wled.local': %s\n", discoveredWledIp.c_str());
    addTerminalLog("[WLED] Light found at " + discoveredWledIp);
    fetchWledEffectsList();
    return true;
  }

  Serial.println("[WLED] No WLED light found on network");
  return false;
}

bool sendWledCommand(const String &urlParams) {
  if (WiFi.status() != WL_CONNECTED)
    return false;

  String ip = getWledIp();
  if (!ip.length()) {
    if (!discoverWled(true))
      return false;
    ip = getWledIp();
  }
  if (!ip.length())
    return false;

  HTTPClient http;
  WiFiClient client;
  http.setTimeout(1200); // 1.2s timeout prevents freezing robot UI
  String url = "http://" + ip + "/win" + urlParams;

  if (!http.begin(client, url))
    return false;

  int code = http.GET();
  http.end();

  if (code > 0) {
    Serial.printf("[WLED] Command OK (%d): %s\n", code, urlParams.c_str());
    return true;
  } else {
    Serial.printf("[WLED] Command failed (%d), will re-scan IP on next command\n", code);
    if (!manualWledIp.length()) {
      discoveredWledIp = ""; // Allow re-discovery next time if IP changed
    }
    return false;
  }
}

bool setWledPower(bool on) {
  return sendWledCommand(on ? "&T=1" : "&T=0");
}

bool setWledBrightness(uint8_t brightness) {
  return sendWledCommand("&T=1&A=" + String(brightness));
}

bool adjustWledBrightness(int delta) {
  return sendWledCommand("&T=1&A=~" + String(delta));
}

bool setWledColor(uint8_t r, uint8_t g, uint8_t b, int brightness) {
  String cmd = "&T=1&R=" + String(r) + "&G=" + String(g) + "&B=" + String(b) + "&FX=0";
  if (brightness >= 0) {
    cmd += "&A=" + String(constrain(brightness, 0, 255));
  }
  return sendWledCommand(cmd);
}

int findWledEffectId(const String &query, String *outMatchedName) {
  String q = query;
  q.toLowerCase();
  q.trim();

  // Clean filler words
  String cleanQ = q;
  cleanQ.replace("mode", "");
  cleanQ.replace("light", "");
  cleanQ.replace("lights", "");
  cleanQ.replace("batti", "");
  cleanQ.replace("bulb", "");
  cleanQ.replace("lamp", "");
  cleanQ.replace("effect", "");
  cleanQ.replace("kardo", "");
  cleanQ.replace("kar do", "");
  cleanQ.replace("lagao", "");
  cleanQ.replace("chalao", "");
  cleanQ.replace("dikhao", "");
  cleanQ.replace("chahiye", "");
  cleanQ.trim();

  if (!cleanQ.length())
    cleanQ = q;

  // If live JSON list has not been fetched yet, try fetching it
  if (!wledEffectsJson.length() && WiFi.status() == WL_CONNECTED && getWledIp().length()) {
    fetchWledEffectsList();
  }

  // 1. If we have live JSON list from WLED device, parse dynamically!
  if (wledEffectsJson.length() > 0) {
    int currentId = 0;
    int p = 0;
    int bestId = -1;
    int bestScore = 0;
    String bestName = "";

    while (p < (int)wledEffectsJson.length()) {
      int startQ = wledEffectsJson.indexOf('"', p);
      if (startQ < 0) break;
      int endQ = wledEffectsJson.indexOf('"', startQ + 1);
      if (endQ < 0) break;

      String name = wledEffectsJson.substring(startQ + 1, endQ);
      p = endQ + 1;

      // In WLED 0.14, strip UI flags after '@' e.g. "Android@,!;" -> "Android"
      int atPos = name.indexOf('@');
      if (atPos >= 0) {
        name = name.substring(0, atPos);
      }
      name.trim();

      String norm = name;
      norm.toLowerCase();

      // Exact match with cleanQ or q
      if (norm == cleanQ || norm == q) {
        if (outMatchedName) *outMatchedName = name;
        return currentId;
      }

      // Substring match
      if (q.indexOf(norm) >= 0 || (norm.length() >= 3 && cleanQ.indexOf(norm) >= 0) ||
          (cleanQ.length() >= 4 && norm.indexOf(cleanQ) >= 0)) {
        int score = norm.length();
        if (score > bestScore) {
          bestScore = score;
          bestId = currentId;
          bestName = name;
        }
      }

      currentId++;
    }

    if (bestId >= 0) {
      if (outMatchedName) *outMatchedName = bestName;
      return bestId;
    }
  }

  // 2. Fallback to comprehensive static table
  int bestStaticId = -1;
  int bestStaticScore = 0;
  String bestStaticName = "";

  for (int i = 0; STATIC_EFFECTS[i].name != nullptr; i++) {
    String name = STATIC_EFFECTS[i].name;
    String norm = name;
    norm.toLowerCase();

    if (norm == cleanQ || norm == q) {
      if (outMatchedName) *outMatchedName = name;
      return STATIC_EFFECTS[i].id;
    }

    if (q.indexOf(norm) >= 0 || (norm.length() >= 3 && cleanQ.indexOf(norm) >= 0) ||
        (cleanQ.length() >= 4 && norm.indexOf(cleanQ) >= 0)) {
      int score = norm.length();
      if (score > bestStaticScore) {
        bestStaticScore = score;
        bestStaticId = STATIC_EFFECTS[i].id;
        bestStaticName = name;
      }
    }
  }

  if (bestStaticId >= 0) {
    if (outMatchedName) *outMatchedName = bestStaticName;
    return bestStaticId;
  }

  return -1;
}

bool setWledEffectById(int fxId, const String &effectName) {
  if (fxId < 0) return false;
  int speed = 128;
  int intensity = 128;

  // Custom tuning for common effects
  if (fxId == 9 || fxId == 8 || fxId == 14 || fxId == 26 || fxId == 30) {
    speed = 135; intensity = 140; // Rainbow / Colorloop variations
  } else if (fxId == 2) {
    speed = 85; intensity = 128; // Breathe
  } else if (fxId == 42 || fxId == 35 || fxId == 86) {
    speed = 120; intensity = 140; // Fire / Candle
  } else if (fxId == 23 || fxId == 24 || fxId == 25) {
    speed = 160; intensity = 128; // Strobe
  } else if (fxId == 101) {
    speed = 150; intensity = 200; // Police
  } else if (fxId == 38 || fxId == 75 || fxId == 90) {
    speed = 110; intensity = 135; // Aurora / Pacifica
  }

  Serial.printf("[WLED] Setting effect ID %d (%s)\n", fxId, effectName.c_str());
  addTerminalLog("[WLED] Effect: " + (effectName.length() ? effectName : String(fxId)));
  return sendWledCommand("&T=1&FX=" + String(fxId) + "&SX=" + String(speed) + "&IX=" + String(intensity));
}

bool setWledEffect(const String &effectName) {
  String matchedName = "";
  int fxId = findWledEffectId(effectName, &matchedName);
  if (fxId >= 0) {
    return setWledEffectById(fxId, matchedName);
  }
  // If a numeric effect ID was passed directly e.g. "42"
  int idVal = -1;
  if (sscanf(effectName.c_str(), "%d", &idVal) == 1 && idVal >= 0) {
    return setWledEffectById(idVal, "FX " + String(idVal));
  }
  return false;
}

bool setWledNextEffect() {
  return sendWledCommand("&T=1&FX=~1&SX=130&IX=140");
}

bool executeWledAction(const String &paramIn) {
  String p = paramIn;
  p.trim();
  p.toLowerCase();

  if (!p.length())
    return false;

  // 1. Power On / Off
  if (p == "on" || p == "1" || p == "true" || p == "start" || p == "chalu" || p == "turn on") {
    return setWledPower(true);
  }
  if (p == "off" || p == "0" || p == "false" || p == "stop" || p == "band" || p == "turn off") {
    return setWledPower(false);
  }

  // 2. Brightness
  // Check for brightness / bri / percent or number
  if (p.startsWith("bri") || p.indexOf("brightness") >= 0 || p.endsWith("%")) {
    int val = -1;
    for (int i = 0; i < (int)p.length(); i++) {
      if (isdigit(p[i])) {
        int v = 0;
        while (i < (int)p.length() && isdigit(p[i])) {
          v = v * 10 + (p[i] - '0');
          i++;
        }
        val = v;
        break;
      }
    }
    if (val >= 0) {
      val = constrain(val, 0, 100);
      uint8_t bri = (uint8_t)((val * 255) / 100);
      return setWledBrightness(bri);
    }
  }
  if (p == "dim" || p == "low" || p == "kam" || p == "dheemi") {
    return setWledBrightness(45); // ~18%
  }
  if (p == "full" || p == "tez" || p == "max" || p == "bright") {
    return setWledBrightness(255); // 100%
  }
  if (p == "medium" || p == "half" || p == "aadhi" || p == "med") {
    return setWledBrightness(128); // 50%
  }

  // 3. Effects / Modes
  if (p == "next" || p == "mode" || p == "next mode" || p == "change" || p == "cycle" || p == "random") {
    return setWledNextEffect();
  }

  // Try matching any of the 160+ effects
  String matchedFx = "";
  int fxId = findWledEffectId(p, &matchedFx);
  if (fxId >= 0) {
    return setWledEffectById(fxId, matchedFx);
  }

  // 4. Colors (Keeps current brightness intact!)
  if (p.indexOf("red") >= 0 || p.indexOf("laal") >= 0)
    return setWledColor(255, 0, 0);
  if (p.indexOf("green") >= 0 || p.indexOf("hara") >= 0)
    return setWledColor(0, 255, 0);
  if (p.indexOf("blue") >= 0 || p.indexOf("neela") >= 0)
    return setWledColor(0, 80, 255);
  if (p.indexOf("yellow") >= 0 || p.indexOf("peela") >= 0)
    return setWledColor(255, 200, 0);
  if (p.indexOf("orange") >= 0 || p.indexOf("narangi") >= 0)
    return setWledColor(255, 90, 0);
  if (p.indexOf("pink") >= 0 || p.indexOf("gulabi") >= 0)
    return setWledColor(255, 40, 140);
  if (p.indexOf("purple") >= 0 || p.indexOf("violet") >= 0 || p.indexOf("baingani") >= 0)
    return setWledColor(160, 0, 255);
  if (p.indexOf("cyan") >= 0 || p.indexOf("aqua") >= 0 || p.indexOf("firozi") >= 0)
    return setWledColor(0, 255, 255);
  if (p.indexOf("white") >= 0 || p.indexOf("safed") >= 0)
    return setWledColor(255, 255, 255);
  if (p.indexOf("warm") >= 0)
    return setWledColor(255, 160, 60);
  if (p.indexOf("cool") >= 0)
    return setWledColor(200, 220, 255);

  // Raw RGB e.g. "255 120 0"
  int r = 0, g = 0, b = 0;
  if (sscanf(p.c_str(), "%d %d %d", &r, &g, &b) == 3) {
    return setWledColor((uint8_t)constrain(r, 0, 255),
                        (uint8_t)constrain(g, 0, 255),
                        (uint8_t)constrain(b, 0, 255));
  }

  return false;
}
