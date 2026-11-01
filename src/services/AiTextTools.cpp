#include "services/AiTextTools.h"
#include "config/AppConfig.h"

// ============================================================
// BASE64 & UTF-8 HELPERS
// ============================================================
static const char b64_table[] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
int8_t b64_lookup[256];

void initBase64Lookup() {
  for (int i = 0; i < 256; i++)
    b64_lookup[i] = -1;
  for (int i = 0; i < 64; i++)
    b64_lookup[(unsigned char)b64_table[i]] = i;
}

int utf8CharLen(unsigned char c) {
  if ((c & 0x80) == 0x00)
    return 1;
  if ((c & 0xE0) == 0xC0)
    return 2;
  if ((c & 0xF0) == 0xE0)
    return 3;
  if ((c & 0xF8) == 0xF0)
    return 4;
  return 1;
}

int utf8Count(const String &s) {
  int n = 0;
  for (size_t i = 0; i < s.length();) {
    i += utf8CharLen((unsigned char)s[i]);
    n++;
  }
  return n;
}

String utf8Truncate(const String &s, int maxChars) {
  int n = 0;
  size_t i = 0;
  size_t lastSpace = (size_t)-1;
  while (i < s.length() && n < maxChars) {
    unsigned char c = (unsigned char)s[i];
    if (c == ' ')
      lastSpace = i;
    i += utf8CharLen(c);
    n++;
  }
  if (i >= s.length())
    return s;
  size_t cut = (lastSpace != (size_t)-1 && lastSpace > 20) ? lastSpace : i;
  for (size_t p = cut; p > 20; p--) {
    char c = s[p - 1];
    if (c == '.' || c == '!' || c == '?' || c == '\n') {
      cut = p;
      break;
    }
  }
  String r = s.substring(0, cut);
  r.trim();
  return r;
}

String jsonTextField(const String &json, const char *key) {
  String needle = "\"" + String(key) + "\"";
  int p = json.indexOf(needle);
  if (p < 0)
    return "";
  p = json.indexOf(':', p);
  if (p < 0)
    return "";
  p = json.indexOf('"', p);
  if (p < 0)
    return "";
  String out;
  int e = p + 1;
  while (e < (int)json.length()) {
    char c = json[e];
    if (c == '\\' && e + 1 < (int)json.length()) {
      char n = json[e + 1];
      if (n == 'u' && e + 5 < (int)json.length()) {
        long cp = strtol(json.substring(e + 2, e + 6).c_str(), NULL, 16);
        if (cp >= 0xD800 && cp <= 0xDBFF && e + 11 < (int)json.length() &&
            json[e + 6] == '\\' && json[e + 7] == 'u') {
          long lo = strtol(json.substring(e + 8, e + 12).c_str(), NULL, 16);
          if (lo >= 0xDC00 && lo <= 0xDFFF) {
            cp = 0x10000 + ((cp - 0xD800) << 10) + (lo - 0xDC00);
            e += 6;
          }
        }
        if (cp < 0x80)
          out += (char)cp;
        else if (cp < 0x800) {
          out += (char)(0xC0 | (cp >> 6));
          out += (char)(0x80 | (cp & 0x3F));
        } else if (cp < 0x10000) {
          out += (char)(0xE0 | (cp >> 12));
          out += (char)(0x80 | ((cp >> 6) & 0x3F));
          out += (char)(0x80 | (cp & 0x3F));
        } else {
          out += (char)(0xF0 | (cp >> 18));
          out += (char)(0x80 | ((cp >> 12) & 0x3F));
          out += (char)(0x80 | ((cp >> 6) & 0x3F));
          out += (char)(0x80 | (cp & 0x3F));
        }
        e += 6;
        continue;
      }
      if (n == 'n' || n == 't')
        out += ' ';
      else
        out += n;
      e += 2;
      continue;
    }
    if (c == '"')
      return out;
    out += c;
    e++;
  }
  return "";
}

String jsonEscape(const String &s) {
  String o;
  o.reserve(s.length() + 16);
  for (size_t i = 0; i < s.length(); i++) {
    char c = s[i];
    if (c == '"')
      o += "\\\"";
    else if (c == '\\')
      o += "\\\\";
    else if (c == '\n')
      o += "\\n";
    else if ((unsigned char)c < 0x20)
      o += ' ';
    else
      o += c;
  }
  return o;
}

String stripAudioCues(const String &text) {
  String s = text;
  int i = 0;
  while (i < (int)s.length()) {
    int b1 = s.indexOf('[', i);
    if (b1 < 0) break;
    int b2 = s.indexOf(']', b1);
    if (b2 < 0) break;
    String tag = s.substring(b1 + 1, b2);
    tag.trim();
    String tagUpper = tag;
    tagUpper.toUpperCase();
    // Preserve action and memory tags for robot command parser
    if (tagUpper.startsWith("ACTION:") || tagUpper.startsWith("MEM:") || tagUpper.startsWith("FOLLOWUP:")) {
      i = b2 + 1;
      continue;
    }
    // Remove sound/tone cue tag like [laughing], [whispering], etc.
    s.remove(b1, (b2 - b1) + 1);
  }
  s.trim();
  return s;
}

String cleanForSpeech(const String &in) {
  String s = in;
  int t1 = s.indexOf("<think>");
  while (t1 >= 0) {
    int t2 = s.indexOf("</think>", t1);
    if (t2 >= 0) {
      s.remove(t1, (t2 + 8) - t1);
    } else {
      s = s.substring(0, t1);
      break;
    }
    t1 = s.indexOf("<think>");
  }
  s.trim();

  // If NOT using Fish Audio, strip bracketed audio/emotion cues so other TTS don't speak them
  if (ttsProvider != "fish" && ttsProvider != "fishaudio") {
    s = stripAudioCues(s);
  }

  String o;
  o.reserve(s.length() + 8);
  bool lastSpace = true;
  for (size_t i = 0; i < s.length(); i++) {
    unsigned char c = (unsigned char)s[i];
    // Skip 4-byte UTF-8 emojis (0xF0..0xF4)
    if ((c & 0xF8) == 0xF0 && i + 3 < s.length()) {
      i += 3; // Advances past emoji
      continue;
    }
    if (c >= 0x80) {
      o += (char)c;
      lastSpace = false;
      continue;
    }
    if (c == '*' || c == '#' || c == '`' || c == '_' || c == '~')
      continue;
    if (c < 32)
      c = ' ';
    if (c == ' ') {
      if (lastSpace)
        continue;
      lastSpace = true;
    } else
      lastSpace = false;
    o += (char)c;
  }
  o.trim();
  return utf8Truncate(o, TTS_MAX_CHARS);
}

bool parseUrl(const String &url, String &host, String &path) {
  int p = url.indexOf("://");
  if (p < 0)
    return false;
  String rest = url.substring(p + 3);
  int sl = rest.indexOf('/');
  String hostPort = (sl < 0) ? rest : rest.substring(0, sl);
  path = (sl < 0) ? "/" : rest.substring(sl);
  int colon = hostPort.indexOf(':');
  host = (colon < 0) ? hostPort : hostPort.substring(0, colon);
  return host.length() > 0;
}


bool containsDevanagari(const String &s) {
  for (size_t i = 0; i + 1 < s.length(); i++) {
    if ((uint8_t)s[i] == 0xE0 &&
        ((uint8_t)s[i + 1] == 0xA4 || (uint8_t)s[i + 1] == 0xA5)) {
      return true;
    }
  }
  return false;
}

String sarvamSttLangCode() {
  if (sttLang == "en")
    return "en-IN";
  if (sttLang == "hi")
    return "hi-IN";
  return "unknown"; // Sarvam AI official code for auto-detection
}

String whisperSttLangCode() {
  if (sttLang == "en")
    return "en";
  if (sttLang == "hi")
    return "hi";
  // Default to Hindi so Whisper never hallucinates Welsh/Spanish/Indonesian on short clips
  return "hi";
}

// ============================================================
// NETWORK TRANSPORT HELPERS
