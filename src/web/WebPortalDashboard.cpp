#include "web/WebPortal.h"
#include "web/dashboard_html_gz.h"
#include <WebServer.h>

void handleRoot() {
  notifyWebRequest();
  server.sendHeader("Access-Control-Allow-Origin", "*");
  server.sendHeader("Connection", "close");
  server.sendHeader("Cache-Control", "no-cache, no-store, must-revalidate");
  server.sendHeader("Pragma", "no-cache");
  server.sendHeader("Expires", "0");
  server.sendHeader("Content-Encoding", "gzip");
  server.send_P(200, "text/html; charset=utf-8", (const char *)DASHBOARD_HTML_GZ, DASHBOARD_HTML_GZ_LEN);
}
