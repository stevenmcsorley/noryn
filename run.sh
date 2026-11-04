#!/usr/bin/env bash
set -euo pipefail
project_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
if [[ -x "$project_dir/Standalone/NORYN" ]]; then
    app_path="$project_dir/Standalone/NORYN"
else
    app_path="$project_dir/build/Noryn_artefacts/Release/Standalone/NORYN"
fi
if [[ ! -x "$app_path" ]]; then
    cmake -S "$project_dir" -B "$project_dir/build" -DCMAKE_BUILD_TYPE=Release
    cmake --build "$project_dir/build" --target Noryn_Standalone -j2
fi
# Install the bundled example without replacing an edited user patch.
noryn_preset_dir="${XDG_CONFIG_HOME:-$HOME/.config}/NORYN/presets"
if [[ -d "$project_dir/presets" ]]; then
    mkdir -p "$noryn_preset_dir"
    for noryn_example in "$project_dir"/presets/*.noryn; do
        [[ -f "$noryn_example" ]] || continue
        noryn_target="$noryn_preset_dir/$(basename -- "$noryn_example")"
        if [[ ! -e "$noryn_target" ]]; then cp -- "$noryn_example" "$noryn_target"; fi
    done
fi
# A repeated launch focuses the current build instead of creating another synth.
if command -v pgrep >/dev/null; then
    while IFS= read -r noryn_pid; do
        if [[ "$(readlink "/proc/$noryn_pid/exe" 2>/dev/null || true)" == "$app_path" ]]; then
            if command -v xdotool >/dev/null; then
                noryn_window=$(xdotool search --onlyvisible --pid "$noryn_pid" --name '^NORYN$' 2>/dev/null | head -1 || true)
                if [[ -n "$noryn_window" ]]; then xdotool windowactivate "$noryn_window" || true; fi
            fi
            exit 0
        fi
    done < <(pgrep -x NORYN || true)
fi
exec "$app_path" "$@"
