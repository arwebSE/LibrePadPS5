"""Compile embedded assets and compare every byte with the offline Vue bundle."""
import argparse
import pathlib
import subprocess
import sys
import tempfile

ROOT = pathlib.Path(__file__).resolve().parents[1]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--clang", default="clang")
    args = parser.parse_args()
    subprocess.run([sys.executable, str(ROOT / "tools/embed_dashboard.py")], check=True)
    (ROOT / "build").mkdir(exist_ok=True)
    with tempfile.TemporaryDirectory(prefix="dashboard-assets-", dir=ROOT / "build") as temp:
        folder = pathlib.Path(temp)
        source = folder / "assets.c"
        source.write_text('''#include <stdio.h>
#include "dashboard/assets.h"
int main(int argc, char **argv) {
    const char *data[] = {PAGE, DASHBOARD_CSS, DASHBOARD_JS, FAVICON};
    size_t sizes[] = {sizeof(PAGE)-1, sizeof(DASHBOARD_CSS)-1, sizeof(DASHBOARD_JS)-1, sizeof(FAVICON)-1};
    if (argc != 5) return 1;
    for (int i=0; i<4; ++i) {
        FILE *file = fopen(argv[i+1], "wb");
        if (!file) return 2;
        int ok = fwrite(data[i],1,sizes[i],file) == sizes[i];
        if (fclose(file) || !ok) return 3;
    }
    return 0;
}
''', encoding="ascii")
        exe = folder / ("assets.exe" if sys.platform == "win32" else "assets")
        subprocess.run([args.clang, "-std=c11", "-Wall", "-Wextra", "-Werror", "-Wtrigraphs",
                        "-I", str(ROOT / "build/generated"), str(source), "-o", str(exe)], check=True)
        names = ["index.html", "dashboard.css", "dashboard.js", "favicon.svg"]
        subprocess.run([str(exe), *[str(folder / name) for name in names]], check=True)
        for name in names:
            assert (folder / name).read_bytes() == (ROOT / "web/dist" / name).read_bytes(), name
    print("Compiled dashboard HTML, CSS, Vue JavaScript and favicon match the bundle byte for byte")


if __name__ == "__main__":
    main()
