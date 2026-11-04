#!/usr/bin/env bash
# Removes the MOSAIC VST3 installed by install.sh. Pass --system for a system-wide install.
# Your presets are kept.
set -euo pipefail

vst3_dir="$HOME/.vst3"
sudo=""

if [[ "${1:-}" == "--system" ]]; then
    vst3_dir="/usr/lib/vst3"
    sudo="sudo"
fi

if [[ -e "$vst3_dir/MOSAIC.vst3" ]]; then
    $sudo rm -rf "$vst3_dir/MOSAIC.vst3"
    echo "Removed $vst3_dir/MOSAIC.vst3"
fi
echo "Done. Your presets were left in place."
