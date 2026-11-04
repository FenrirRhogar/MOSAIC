#!/usr/bin/env bash
# Installs the MOSAIC VST3. Default: for the current user (~/.vst3).
# --system: for all users (/usr/lib/vst3), using sudo. The Standalone app runs from this folder.
set -euo pipefail

here="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
vst3_dir="$HOME/.vst3"
sudo=""

if [[ "${1:-}" == "--system" ]]; then
    vst3_dir="/usr/lib/vst3"
    sudo="sudo"
fi

echo "Installing MOSAIC:"
$sudo mkdir -p "$vst3_dir"
$sudo rm -rf "${vst3_dir:?}/MOSAIC.vst3"
$sudo cp -r "$here/VST3/MOSAIC.vst3" "$vst3_dir/"
echo "  $vst3_dir/MOSAIC.vst3"
echo "Done. Rescan plugins in your host, or run Standalone/MOSAIC directly."
