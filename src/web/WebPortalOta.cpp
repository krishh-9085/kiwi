#include "web/WebPortal.h"
#include "web/update_html_gz.h"
#include <WebServer.h>

void handleOtaPage() {
  notifyWebRequest();
  server.sendHeader("Access-Control-Allow-Origin", "*");
  server.sendHeader("Connection", "close");
  server.sendHeader("Cache-Control", "no-cache, no-store, must-revalidate");
  server.sendHeader("Content-Encoding", "gzip");
  server.send_P(200, "text/html; charset=utf-8", (const char *)UPDATE_HTML_GZ, UPDATE_HTML_GZ_LEN);
}
