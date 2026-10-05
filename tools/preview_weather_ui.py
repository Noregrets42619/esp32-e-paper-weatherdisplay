"""Compile the firmware renderer for the host, test it and export exact-pixel PNGs.

Requires a native GCC, Pillow, and an ESP-IDF source checkout (for cJSON).
python tools/preview_weather_ui.py --cc gcc --idf path/to/esp-idf
All generated output stays under build/weather-ui-preview.
"""
import argparse
import subprocess
from pathlib import Path
from PIL import Image

ROOT = Path(__file__).resolve().parents[1]
PALETTE = [(22, 22, 22), (255, 255, 255), (255, 213, 0), (196, 30, 38)]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--cc", default="gcc")
    parser.add_argument("--idf", type=Path, required=True)
    args = parser.parse_args()
    directory = ROOT / "build/weather-ui-preview"
    directory.mkdir(parents=True, exist_ok=True)
    (directory / "esp_err.h").write_text(
        "#pragma once\ntypedef int esp_err_t;\n#define ESP_OK 0\n#define ESP_FAIL -1\n#define ESP_ERR_INVALID_ARG 258\n")
    # Windows CRT uses different names; this shim only participates in host tests.
    (directory / "host_compat.h").write_text(
        "#ifdef _WIN32\n#include <time.h>\n#include <stdlib.h>\n"
        "static inline struct tm *host_localtime_r(const time_t *t, struct tm *out) "
        "{ struct tm *value = localtime(t); if (!value) return NULL; *out = *value; return out; }\n"
        "#define localtime_r host_localtime_r\n#define setenv(n,v,o) _putenv_s(n,v)\n#endif\n")
    cjson = args.idf / "components/json/cJSON"
    command = [args.cc, "-std=gnu11", "-Wall", "-Wextra", "-Werror", "-O2",
               "-include", str(directory / "host_compat.h"),
               "-I" + str(directory), "-Imain", "-Icomponents/epd4in2b/include",
               "-Icomponents/weather/include", "-I" + str(cjson),
               "main/color_canvas.c", "main/weather_ui.c", "main/fonts/weather_font.c",
               "components/weather/src/weather_parse.c", "tools/preview_weather_ui.c",
               str(cjson / "cJSON.c"), "-lm", "-o", str(directory / "preview.exe")]
    subprocess.run(command, cwd=ROOT, check=True)
    subprocess.run([str(directory / "preview.exe"), str(directory)], cwd=ROOT, check=True)
    for path in directory.glob("weather-*.bin"):
        raw = path.read_bytes()
        assert len(raw) == 30000
        pixels = [PALETTE[(byte >> shift) & 3] for byte in raw for shift in (6, 4, 2, 0)]
        image = Image.new("RGB", (400, 300))
        image.putdata(pixels)
        image.save(path.with_suffix(".png"))
        image.resize((1200, 900), Image.Resampling.NEAREST).save(path.with_name(path.stem + "-3x.png"))
        print(path.with_suffix(".png"))


if __name__ == "__main__":
    main()
