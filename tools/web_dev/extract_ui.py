import os
import re
import json

WORKSPACE = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
SRC_WEB = os.path.join(WORKSPACE, "src", "web")
TEMPLATES_DIR = os.path.join(os.path.dirname(__file__), "templates")

os.makedirs(TEMPLATES_DIR, exist_ok=True)

def extract_dashboard():
    src_file = os.path.join(SRC_WEB, "WebPortalDashboard.cpp")
    with open(src_file, "r", encoding="utf-8") as f:
        code = f.read()

    css = re.search(r'DASHBOARD_CSS\[\] PROGMEM = R"rawcss\((.*?)\)rawcss";', code, re.DOTALL).group(1)
    stat = re.search(r'DASHBOARD_STATUS_TEMPLATE\[\] PROGMEM = R"rawstat\((.*?)\)rawstat";', code, re.DOTALL).group(1)
    console = re.search(r'DASHBOARD_LIVE_CONSOLE\[\] PROGMEM = R"rawconsole\((.*?)\)rawconsole";', code, re.DOTALL).group(1)
    b1 = re.search(r'DASHBOARD_BODY_PART1\[\] PROGMEM = R"rawb1\((.*?)\)rawb1";', code, re.DOTALL).group(1)
    b2 = re.search(r'DASHBOARD_BODY_PART2\[\] PROGMEM = R"rawb2\((.*?)\)rawb2";', code, re.DOTALL).group(1)
    foot = re.search(r'DASHBOARD_FOOT\[\] PROGMEM = R"rawfoot\((.*?)\)rawfoot\";', code, re.DOTALL).group(1)

    full_html = css + stat + "\n" + console + "\n" + b1 + "\n" + b2 + "\n" + foot
    out_path = os.path.join(TEMPLATES_DIR, "dashboard.html")
    with open(out_path, "w", encoding="utf-8") as f:
        f.write(full_html)
    print(f"Extracted dashboard.html ({len(full_html)} bytes)")

def extract_faces():
    src_file = os.path.join(SRC_WEB, "WebPortalFaces.cpp")
    with open(src_file, "r", encoding="utf-8") as f:
        code = f.read()
    match = re.search(r'FACE_PAGE_HTML\[\] PROGMEM = R"RAWFACE\((.*?)\)RAWFACE";', code, re.DOTALL)
    if match:
        html = match.group(1)
        out_path = os.path.join(TEMPLATES_DIR, "faces.html")
        with open(out_path, "w", encoding="utf-8") as f:
            f.write(html)
        print(f"Extracted faces.html ({len(html)} bytes)")

def extract_wiring():
    src_file = os.path.join(SRC_WEB, "WebPortalWiring.cpp")
    with open(src_file, "r", encoding="utf-8") as f:
        code = f.read()
    match = re.search(r'WIRING_PAGE_HTML\[\] PROGMEM = R"RAWWIRING\((.*?)\)RAWWIRING";', code, re.DOTALL)
    if match:
        html = match.group(1)
        out_path = os.path.join(TEMPLATES_DIR, "wiring.html")
        with open(out_path, "w", encoding="utf-8") as f:
            f.write(html)
        print(f"Extracted wiring.html ({len(html)} bytes)")

def extract_ota():
    src_file = os.path.join(SRC_WEB, "WebPortalOta.cpp")
    with open(src_file, "r", encoding="utf-8") as f:
        code = f.read()
    match = re.search(r'OTA_HTML\[\] PROGMEM = R"rawliteral\((.*?)\)rawliteral";', code, re.DOTALL)
    if match:
        html = match.group(1)
        out_path = os.path.join(TEMPLATES_DIR, "update.html")
        with open(out_path, "w", encoding="utf-8") as f:
            f.write(html)
        print(f"Extracted update.html ({len(html)} bytes)")

if __name__ == "__main__":
    extract_dashboard()
    extract_faces()
    extract_wiring()
    extract_ota()
    print("All UI templates successfully extracted!")
