import os
import subprocess
import gzip

WORKSPACE = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
REACT_DIR = os.path.join(WORKSPACE, "react_ui")
DIST_HTML = os.path.join(REACT_DIR, "dist", "index.html")
SRC_WEB = os.path.join(WORKSPACE, "src", "web")

def format_progmem_header(var_name, data):
    lines = [
        "// Auto-generated pre-compressed gzip asset for Kiwi React Web Portal",
        "// Ultra-fast single-page React app bundled with vite-plugin-singlefile.",
        "#pragma once",
        "#include <Arduino.h>",
        f"const uint8_t {var_name}[] PROGMEM = {{"
    ]
    for i in range(0, len(data), 16):
        chunk = data[i:i+16]
        hex_str = ", ".join(f"0x{b:02x}" for b in chunk)
        if i + 16 < len(data):
            lines.append(f"  {hex_str},")
        else:
            lines.append(f"  {hex_str}")
    lines.append("};")
    lines.append(f"const size_t {var_name}_LEN = sizeof({var_name});")
    lines.append("")
    return "\n".join(lines)

def build_react():
    print("Building React UI via 'npm run build'...")
    npm_cmd = "npm.cmd" if os.name == "nt" else "npm"
    res = subprocess.run([npm_cmd, "run", "build"], cwd=REACT_DIR, shell=True)
    if res.returncode != 0:
        print("Error: React build failed!")
        return False
    return True

def sync_react_to_cpp():
    if not build_react():
        return

    with open(DIST_HTML, "rb") as f:
        raw_data = f.read()

    gz_data = gzip.compress(raw_data, compresslevel=9)
    header_content = format_progmem_header("DASHBOARD_HTML_GZ", gz_data)
    header_path = os.path.join(SRC_WEB, "dashboard_html_gz.h")

    # Safety backup of original html header if not already backed up
    bak_path = header_path + ".bak_classic"
    if not os.path.exists(bak_path) and os.path.exists(header_path):
        import shutil
        shutil.copy2(header_path, bak_path)
        print(f"Backed up original firmware UI to: {bak_path}")

    with open(header_path, "w", encoding="utf-8") as f:
        f.write(header_content)

    print(f"React UI synced to C++ firmware successfully!")
    print(f"  Original single-file HTML bundle: {len(raw_data) / 1024:.1f} KB")
    print(f"  Gzipped PROGMEM payload size:     {len(gz_data) / 1024:.1f} KB")
    print(f"  Target Header: {header_path}")
    print("\nTip: Run 'sync_react_to_cpp.bat' anytime after UI edits to rebuild PROGMEM.")

if __name__ == "__main__":
    sync_react_to_cpp()
