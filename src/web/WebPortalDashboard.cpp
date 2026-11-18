#include "web/WebPortal.h"
#include "web/dashboard_html_gz.h"
#include "config/AppConfig.h"
#include <WebServer.h>

void handleRoot() {
  notifyWebRequest();

  String etag = "\"" + String(FIRMWARE_VER) + "-" + String(DASHBOARD_HTML_GZ_LEN) + "\"";

  server.sendHeader("Access-Control-Allow-Origin", "*");
  server.sendHeader("Cache-Control", "public, max-age=3600, must-revalidate");
  server.sendHeader("ETag", etag);

  // Browser cache validation: return 304 Not Modified if ETag matches
  if (server.hasHeader("If-None-Match")) {
    String reqEtag = server.header("If-None-Match");
    if (reqEtag == etag || reqEtag.indexOf(etag) >= 0 || reqEtag == "*") {
      server.send(304, "text/plain", "");
      return;
    }
  }

  server.sendHeader("Connection", "close");
  server.sendHeader("Content-Encoding", "gzip");
  server.send_P(200, "text/html; charset=utf-8", (const char *)DASHBOARD_HTML_GZ, DASHBOARD_HTML_GZ_LEN);
}
