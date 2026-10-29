#include "services/WeatherService.h"
#include "config/AppConfig.h"
#include <WiFi.h>
#include <HTTPClient.h>

String getWeatherCodeDescription(int code) {
  switch (code) {
  case 0:
    return "Clear Sky";
  case 1:
    return "Mainly Clear";
  case 2:
    return "Partly Cloudy";
  case 3:
    return "Overcast";
  case 45:
  case 48:
    return "Foggy";
  case 51:
  case 53:
  case 55:
    return "Drizzle";
  case 61:
  case 63:
  case 65:
    return "Rainy";
  case 71:
  case 73:
  case 75:
    return "Snowy";
  case 80:
  case 81:
  case 82:
    return "Rain Showers";
  case 95:
  case 96:
  case 99:
    return "Thunderstorm";
  default:
    return "Pleasant";
  }
}

bool fetchWeather() {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("[WEATHER] No Wi-Fi");
    return false;
  }
  weatherLat.trim();
  weatherLon.trim();
  if (weatherLat.length() == 0 || weatherLon.length() == 0) {
    weatherLat = "28.6139";
    weatherLon = "77.2090";
  }
  HTTPClient http;
  http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
  http.setTimeout(4000);
  String url = "http://api.open-meteo.com/v1/forecast?latitude=" + weatherLat +
               "&longitude=" + weatherLon +
               "&current=temperature_2m,relative_humidity_2m,weather_code,wind_speed_10m";
  Serial.printf("[WEATHER] Fetching: %s\n", url.c_str());
  if (!http.begin(url)) {
    Serial.println("[WEATHER] Failed to begin HTTP");
    return false;
  }
  int code = http.GET();
  if (code != 200) {
    Serial.printf("[WEATHER] HTTP response: %d\n", code);
    http.end();
    return false;
  }
  if (ESP.getFreeHeap() < 25000) {
    Serial.println("[WEATHER] Low heap, aborting fetch");
    http.end();
    return false;
  }
  String payload = http.getString();
  http.end();
  int curIdx = payload.indexOf("\"current\":");
  if (curIdx < 0) {
    Serial.println("[WEATHER] Current block missing");
    return false;
  }
  String cur = payload.substring(curIdx);
  int tIdx = cur.indexOf("\"temperature_2m\":");
  if (tIdx >= 0) {
    int comma = cur.indexOf(',', tIdx);
    weatherTemp = cur.substring(tIdx + 17, comma).toFloat();
  }
  int hIdx = cur.indexOf("\"relative_humidity_2m\":");
  if (hIdx >= 0) {
    int comma = cur.indexOf(',', hIdx);
    weatherHumidity = cur.substring(hIdx + 23, comma).toInt();
  }
  int wIdx = cur.indexOf("\"weather_code\":");
  if (wIdx >= 0) {
    int comma = cur.indexOf(',', wIdx);
    weatherCode = cur.substring(wIdx + 15, comma).toInt();
    weatherDesc = getWeatherCodeDescription(weatherCode);
  }
  int wsIdx = cur.indexOf("\"wind_speed_10m\":");
  if (wsIdx >= 0) {
    int comma = cur.indexOf(',', wsIdx);
    if (comma < 0)
      comma = cur.indexOf('}', wsIdx);
    weatherWind = cur.substring(wsIdx + 17, comma).toFloat();
  }
  lastWeatherFetchAt = millis();
  Serial.printf("[WEATHER] %s: %.1f C, %s, H:%d%%, Wind:%.1f km/h\n",
                weatherCity.c_str(), weatherTemp, weatherDesc.c_str(),
                weatherHumidity, weatherWind);
  return true;
}
