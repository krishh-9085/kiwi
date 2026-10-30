import os
import re

WORKSPACE = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
SRC_WEB = os.path.join(WORKSPACE, "src", "web")
TEMPLATES_DIR = os.path.join(os.path.dirname(__file__), "templates")

def sync_dashboard():
    dash_path = os.path.join(TEMPLATES_DIR, "dashboard.html")
    if not os.path.exists(dash_path):
        print(f"Error: {dash_path} not found")
        return False

    with open(dash_path, "r", encoding="utf-8") as f:
        html = f.read()

    # Find status strip position
    status_grid_pos = html.find('<section class="strip"')
    if status_grid_pos == -1:
        status_grid_pos = html.find('<div class="status-grid">')

    # Find live console position
    live_console_pos = html.find('<section id="live"')
    if live_console_pos == -1:
        live_console_pos = html.find('<!-- Live voice interaction')
    if live_console_pos == -1:
        live_console_pos = html.find('<!-- Live AI Voice Interaction')

    # Find form start position
    form_start_pos = html.find('<form method="POST"')
    if form_start_pos == -1:
        form_start_pos = html.find('<form action="/save"')

    # Find TTS start position
    tts_pos = html.find('<!-- Text-to-Speech -->')
    if tts_pos == -1:
        tts_pos = html.find('<!-- Text-to-speech')
    if tts_pos == -1:
        tts_match = re.search(r'<div class="card">\s*<div class="card-title">Text-to-Speech', html)
        if tts_match:
            tts_pos = tts_match.start()

    # Find foot position
    foot_pos = html.find('<script>')

    if any(p == -1 for p in [status_grid_pos, live_console_pos, form_start_pos, tts_pos, foot_pos]):
        print(f"Warning: Marker positions: status={status_grid_pos}, live={live_console_pos}, form={form_start_pos}, tts={tts_pos}, foot={foot_pos}")
        return False

    css_part = html[:status_grid_pos]
    status_part = html[status_grid_pos:live_console_pos].strip()
    console_part = html[live_console_pos:form_start_pos].strip()
    body1_part = html[form_start_pos:tts_pos].strip()
    body2_part = html[tts_pos:foot_pos].strip()
    foot_part = html[foot_pos:]

    cpp_path = os.path.join(SRC_WEB, "WebPortalDashboard.cpp")
    with open(cpp_path, "r", encoding="utf-8") as f:
        cpp_code = f.read()

    cpp_code = re.sub(r'DASHBOARD_CSS\[\] PROGMEM = R"rawcss\(.*?\)rawcss";',
                      lambda m: f'DASHBOARD_CSS[] PROGMEM = R"rawcss(\n{css_part})rawcss";', cpp_code, flags=re.DOTALL)
    cpp_code = re.sub(r'DASHBOARD_STATUS_TEMPLATE\[\] PROGMEM = R"rawstat\(.*?\)rawstat";',
                      lambda m: f'DASHBOARD_STATUS_TEMPLATE[] PROGMEM = R"rawstat({status_part})rawstat";', cpp_code, flags=re.DOTALL)
    cpp_code = re.sub(r'DASHBOARD_LIVE_CONSOLE\[\] PROGMEM = R"rawconsole\(.*?\)rawconsole";',
                      lambda m: f'DASHBOARD_LIVE_CONSOLE[] PROGMEM = R"rawconsole({console_part}\n)rawconsole";', cpp_code, flags=re.DOTALL)
    cpp_code = re.sub(r'DASHBOARD_BODY_PART1\[\] PROGMEM = R"rawb1\(.*?\)rawb1";',
                      lambda m: f'DASHBOARD_BODY_PART1[] PROGMEM = R"rawb1({body1_part}\n)rawb1";', cpp_code, flags=re.DOTALL)
    cpp_code = re.sub(r'DASHBOARD_BODY_PART2\[\] PROGMEM = R"rawb2\(.*?\)rawb2";',
                      lambda m: f'DASHBOARD_BODY_PART2[] PROGMEM = R"rawb2({body2_part}\n)rawb2";', cpp_code, flags=re.DOTALL)
    cpp_code = re.sub(r'DASHBOARD_FOOT\[\] PROGMEM = R"rawfoot\(.*?\)rawfoot";',
                      lambda m: f'DASHBOARD_FOOT[] PROGMEM = R"rawfoot({foot_part}\n)rawfoot";', cpp_code, flags=re.DOTALL)

    with open(cpp_path, "w", encoding="utf-8") as f:
        f.write(cpp_code)
    print("Synced dashboard.html -> WebPortalDashboard.cpp")
    return True

def sync_faces():
    path = os.path.join(TEMPLATES_DIR, "faces.html")
    if not os.path.exists(path):
        return False
    with open(path, "r", encoding="utf-8") as f:
        html = f.read()
    cpp_path = os.path.join(SRC_WEB, "WebPortalFaces.cpp")
    with open(cpp_path, "r", encoding="utf-8") as f:
        cpp = f.read()
    cpp = re.sub(r'FACE_PAGE_HTML\[\] PROGMEM = R"RAWFACE\(.*?\)RAWFACE";',
                 lambda m: f'FACE_PAGE_HTML[] PROGMEM = R"RAWFACE({html})RAWFACE";', cpp, flags=re.DOTALL)
    with open(cpp_path, "w", encoding="utf-8") as f:
        f.write(cpp)
    print("Synced faces.html -> WebPortalFaces.cpp")
    return True

def sync_wiring():
    # Wiring page has been removed from UI
    return True

def sync_ota():
    path = os.path.join(TEMPLATES_DIR, "update.html")
    if not os.path.exists(path):
        return False
    with open(path, "r", encoding="utf-8") as f:
        html = f.read()
    cpp_path = os.path.join(SRC_WEB, "WebPortalOta.cpp")
    with open(cpp_path, "r", encoding="utf-8") as f:
        cpp = f.read()
    cpp = re.sub(r'OTA_HTML\[\] PROGMEM = R"rawliteral\(.*?\)rawliteral";',
                 lambda m: f'OTA_HTML[] PROGMEM = R"rawliteral({html})rawliteral";', cpp, flags=re.DOTALL)
    with open(cpp_path, "w", encoding="utf-8") as f:
        f.write(cpp)
    print("Synced update.html -> WebPortalOta.cpp")
    return True

if __name__ == "__main__":
    sync_dashboard()
    sync_faces()
    sync_wiring()
    sync_ota()
    print("All UI changes synced back to C++ source files successfully!")
