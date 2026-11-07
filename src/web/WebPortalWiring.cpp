#include "web/WebPortal.h"
#include <WebServer.h>

void handleWiring() {
  notifyWebRequest();
  server.sendHeader("Location", "/", true);
  server.send(302, "text/plain", "");
}
