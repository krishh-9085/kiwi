#include "web/WebPortal.h"
#include <WebServer.h>

void handleWiring() {
  server.sendHeader("Location", "/", true);
  server.send(302, "text/plain", "");
}
