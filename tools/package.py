#!/usr/bin/env python3
"""Packages a Release build into dist/MOSAIC-<version>-<platform>.zip, ready to upload.

    tools/package.py <linux|windows|macos> [build-dir] [--juce PATH] [--signed]

--juce overrides the JUCE path (otherwise read from the build's CMakeCache, including a
FetchContent checkout). --signed marks macOS bundles as Developer ID signed and notarised,
which drops the Gatekeeper workaround from the README.
"""

import argparse
import os
import re
import shutil
import stat
import subprocess
import zipfile
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
FORMATS = {"linux": ["VST3", "Standalone"], "windows": ["VST3", "Standalone"], "macos": ["VST3", "AU", "Standalone"]}
# What to take from each artefact folder; the linker leaves .exp/.lib/.pdb files next to these on Windows.
KEEP = {"VST3": {"MOSAIC.vst3"}, "AU": {"MOSAIC.component"}, "Standalone": {"MOSAIC", "MOSAIC.exe", "MOSAIC.app"}}
LABELS = {"linux": "Linux-x64", "windows": "Windows-x64", "macos": "macOS"}

MAC_QUARANTINE = """
2. These builds are not notarised by Apple, so macOS blocks them at first. In Terminal, run:

     sudo xattr -dr com.apple.quarantine /Library/Audio/Plug-Ins/Components/MOSAIC.component
     sudo xattr -dr com.apple.quarantine /Library/Audio/Plug-Ins/VST3/MOSAIC.vst3
   and for the Standalone app, right-click it and choose Open the first time.
"""


def version():
    text = (ROOT / "CMakeLists.txt").read_text()
    return re.search(r"project\(MOSAIC VERSION ([0-9.]+)", text).group(1)


def juce_dir(build, override):
    if override:
        return Path(override)
    cache = (build / "CMakeCache.txt").read_text()
    local = re.search(r"^JUCE_DIR:PATH=(.*)$", cache, re.M)
    if local and local.group(1):
        return Path(local.group(1))
    return build / "_deps" / "juce-src"


def render_readme(platform, values):
    """Keeps [platform] ... [/platform] blocks for this platform and drops the others."""
    text = (ROOT / "packaging" / "README.txt").read_text()
    text = re.sub(r"\[(\w+)\]\n(.*?)\[/\1\]\n",
                  lambda m: m.group(2) if m.group(1) == platform else "", text, flags=re.S)
    for key, value in values.items():
        text = text.replace(f"@{key}@", value)
    return text


def minimum_glibc(stage):
    """The newest glibc symbol any binary needs is the oldest glibc the package runs on."""
    versions = set()
    for binary in list(stage.rglob("*.so")) + [stage / "Standalone" / "MOSAIC"]:
        if binary.exists():
            out = subprocess.run(["objdump", "-T", str(binary)], capture_output=True, text=True, check=True).stdout
            versions.update(re.findall(r"GLIBC_([0-9.]+)", out))
    return max(versions, key=lambda v: tuple(int(p) for p in v.split(".")))


def zip_tree(stage, archive):
    """Zips `stage` keeping executable bits, so the app and install.sh still run after unzipping."""
    with zipfile.ZipFile(archive, "w", zipfile.ZIP_DEFLATED) as zf:
        for path in sorted(stage.rglob("*")):
            if path.is_dir():
                continue
            info = zipfile.ZipInfo.from_file(path, str(path.relative_to(stage.parent)).replace(os.sep, "/"))
            info.external_attr = (stat.S_IMODE(path.stat().st_mode) | stat.S_IFREG) << 16
            info.compress_type = zipfile.ZIP_DEFLATED
            with open(path, "rb") as f:
                zf.writestr(info, f.read())


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("platform", choices=FORMATS)
    parser.add_argument("build", nargs="?", default=str(ROOT / "build"))
    parser.add_argument("--juce")
    parser.add_argument("--signed", action="store_true")
    args = parser.parse_args()

    build = Path(args.build)
    ver = version()
    name = f"MOSAIC-{ver}-{LABELS[args.platform]}"
    stage = ROOT / "dist" / name
    archive = stage.parent / f"{name}.zip"
    shutil.rmtree(stage, ignore_errors=True)
    archive.unlink(missing_ok=True)

    for fmt in FORMATS[args.platform]:
        (stage / fmt).mkdir(parents=True)
        for item in (build / "MOSAIC_artefacts" / "Release" / fmt).iterdir():
            if item.name not in KEEP[fmt]:
                continue
            if item.is_dir():
                shutil.copytree(item, stage / fmt / item.name, symlinks=True)
            else:
                shutil.copy2(item, stage / fmt / item.name)

    values = {"VERSION": ver, "GLIBC": "", "MAC_QUARANTINE": "" if args.signed else MAC_QUARANTINE}
    if args.platform == "linux":
        for binary in list(stage.rglob("*.so")) + [stage / "Standalone" / "MOSAIC"]:
            subprocess.run(["strip", "--strip-unneeded", str(binary)], check=True)
        values["GLIBC"] = minimum_glibc(stage)
        for script in ("install.sh", "uninstall.sh"):
            shutil.copy(ROOT / "packaging" / script, stage / script)
            (stage / script).chmod(0o755)

    (stage / "README.txt").write_text(render_readme(args.platform, values))
    shutil.copy(ROOT / "LICENSE", stage / "LICENSE.txt")
    shutil.copy(ROOT / "packaging" / "CHANGELOG.txt", stage / "CHANGELOG.txt")

    sdk = juce_dir(build, args.juce) / "modules" / "juce_audio_processors_headless" / "format_types"
    notices = [(ROOT / "THIRDPARTY.md").read_text(), "",
               "## VST 3 SDK (Steinberg Media Technologies GmbH)", "",
               (sdk / "VST3_SDK" / "LICENSE.txt").read_text()]
    (stage / "THIRD_PARTY_NOTICES.txt").write_text("\n".join(notices))

    if args.platform == "macos":
        # ditto keeps code signatures and extended attributes intact.
        subprocess.run(["ditto", "-c", "-k", "--keepParent", str(stage), str(archive)], check=True)
    else:
        zip_tree(stage, archive)

    note = f" (needs glibc {values['GLIBC']}+)" if values["GLIBC"] else ""
    print(f"{archive}{note}")


if __name__ == "__main__":
    main()
