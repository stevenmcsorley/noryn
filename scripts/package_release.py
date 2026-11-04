#!/usr/bin/env python3
"""Build reviewed native/plugin/source archives without machine-local credentials."""
from pathlib import Path
import hashlib
import io
import json
import subprocess
import tarfile
import zipfile

ROOT = Path(__file__).resolve().parents[1]
DIST = ROOT / "dist"
VERSION = "0.4.0"
DIST.mkdir(exist_ok=True)


def plugin_files():
    fixed = ["plugin.json", "mcp.json", "package.json", "package-lock.json", "tsconfig.json", "wrangler.jsonc", "README.md", "LICENSE", "start.sh"]
    files = [ROOT / "mcp" / name for name in fixed]
    for folder in ["server", "skills", "assets"]:
        files += [p for p in (ROOT / "mcp" / folder).rglob("*") if p.is_file() and not p.is_symlink()]
    return sorted(files)


def project_files():
    fixed = ["CMakeLists.txt", "README.md", "LICENSE", "THIRD_PARTY.md", "AUTHENTICITY.md", "character.png", "sequencer-verification.json", "run.sh", ".gitignore", ".gitattributes", ".clang-format", "PRESETS.md", "validation.json", "control-sensitivity.csv", "native-knob-verification.json", "mcp-verification.json", "preview.png", "preview-small.png", "sequencer.png", "lfos.png", "scales.png", "demo.wav", "signature-demo.wav", "stolen-dog-study.wav", "signature-bank.png", "preset-levels.csv"]
    files = [ROOT / name for name in fixed]
    for folder in ["Source", "scripts", "licenses", "presets", "examples", ".github"]:
        files += [p for p in (ROOT / folder).rglob("*") if p.is_file() and not p.is_symlink()]
    return sorted(set(files + plugin_files()))


plugin = DIST / f"NORYN-Control-{VERSION}.zip"
with zipfile.ZipFile(plugin, "w", zipfile.ZIP_DEFLATED, compresslevel=8) as archive:
    for file in plugin_files():
        archive.write(file, "noryn-control/" + file.relative_to(ROOT / "mcp").as_posix())

native = DIST / f"NORYN-{VERSION}-linux-x86_64.zip"
base = ROOT / "build/Noryn_artefacts/Release"
with zipfile.ZipFile(native, "w", zipfile.ZIP_DEFLATED, compresslevel=8) as archive:
    archive.write(base / "Standalone/NORYN", "NORYN/Standalone/NORYN")
    for file in (base / "VST3/NORYN.vst3").rglob("*"):
        if file.is_file():
            archive.write(file, "NORYN/VST3/" + file.relative_to(base / "VST3").as_posix())
    for name in ["README.md", "LICENSE", "THIRD_PARTY.md", "AUTHENTICITY.md", "character.png", "sequencer-verification.json", "PRESETS.md", "validation.json", "control-sensitivity.csv", "native-knob-verification.json", "mcp-verification.json", "preview.png", "sequencer.png", "lfos.png", "scales.png", "preview-small.png", "demo.wav", "signature-demo.wav", "stolen-dog-study.wav", "signature-bank.png", "preset-levels.csv"]:
        if name == "README.md":
            archive.writestr("NORYN/README.md", (ROOT / name).read_text().replace("](mcp/README.md)", "](noryn-control/README.md)"))
        else:
            archive.write(ROOT / name, "NORYN/" + name)
    for file in (ROOT / "licenses").rglob("*"):
        if file.is_file():
            archive.write(file, "NORYN/" + file.relative_to(ROOT).as_posix())
    for folder in ["presets","examples"]:
        for file in (ROOT/folder).rglob("*"):
            if file.is_file():archive.write(file,"NORYN/"+file.relative_to(ROOT).as_posix())
    for file in plugin_files():
        archive.write(file, "NORYN/noryn-control/" + file.relative_to(ROOT / "mcp").as_posix())
    launcher = zipfile.ZipInfo("NORYN/run.sh")
    launcher.external_attr = 0o100755 << 16
    archive.writestr(launcher, (ROOT / "run.sh").read_text())
    archive.writestr("NORYN/START-HERE.txt", "NORYN 0.4 for Linux x86_64\n\nRun ./run.sh or Standalone/NORYN. Select audio and MIDI devices in Options.\nCopy the complete VST3/NORYN.vst3 bundle into ~/.vst3/ and rescan your DAW.\nSee README.md for the synth, sequencer, scale engine and four LFOs.\nThe noryn-control directory is the optional local Cloudflare Code Mode MCP plugin.\nAGPL-3.0 license and matching source are provided with this release.\n")

source = DIST / f"NORYN-{VERSION}-source.tar.gz"
juce = ROOT / "build/_deps/juce-src"
# git archive retains original framework source and notices, and excludes its .git/build data.
framework = subprocess.check_output(["git", "-C", str(juce), "archive", "--format=tar", "HEAD"])
with tarfile.open(source, "w:gz", compresslevel=8) as archive:
    for file in project_files():
        archive.add(file, arcname=f"NORYN-{VERSION}/" + file.relative_to(ROOT).as_posix(), recursive=False)
    with tarfile.open(fileobj=io.BytesIO(framework), mode="r:") as upstream:
        for item in upstream:
            item.name = f"NORYN-{VERSION}/vendor/JUCE/" + item.name
            archive.addfile(item, upstream.extractfile(item) if item.isfile() else None)

# Inspect final archives, not just the source directory.
for package in [native, plugin]:
    with zipfile.ZipFile(package) as archive:
        assert archive.testzip() is None
        for name in archive.namelist():
            parts = Path(name).parts
            assert "node_modules" not in parts and ".wrangler" not in parts
            assert not any(part in [".dev.vars", ".bridge-token", ".mcp-token", "mcp.local.json", "WORKPLAN.md"] for part in parts)
with zipfile.ZipFile(plugin) as archive:
    manifest = json.loads(archive.read("noryn-control/plugin.json"))
    assert manifest["name"] == "noryn-control" and manifest["version"] == VERSION
    assert "noryn-control/server/bootstrap.mjs" in archive.namelist()
with tarfile.open(source) as archive:
    assert f"NORYN-{VERSION}/vendor/JUCE/modules/juce_core/juce_core.h" in archive.getnames()
    assert not any(name.endswith((".mcp-token", ".bridge-token", ".dev.vars", "WORKPLAN.md")) for name in archive.getnames())

assets = [native, plugin, source] + [ROOT / name for name in ["preview.png", "sequencer.png", "lfos.png", "scales.png", "demo.wav", "signature-demo.wav", "stolen-dog-study.wav", "signature-bank.png", "character.png", "preset-levels.csv"]]
checksums = DIST / "SHA256SUMS.txt"
checksums.write_text("".join(f"{hashlib.sha256(file.read_bytes()).hexdigest()}  {file.name}\n" for file in assets))
print("\n".join(f"{file.name}: {file.stat().st_size:,} bytes" for file in [native, plugin, source, checksums]))
