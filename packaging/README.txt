MOSAIC  -  version @VERSION@
by fenrikos

A modular sampler for MIDI pad controllers: a grid of up to 8x8 pads, a sample on each, three trigger
modes, three pitch/time engines, three LFOs, multi-out routing and MIDI learn.


REQUIREMENTS
------------
[linux]
- 64-bit Linux (x86_64), glibc @GLIBC@ or newer
- A VST3 host (REAPER, Bitwig Studio, Ardour, Carla, ...) or use the Standalone app
- X11 or XWayland for the window
[/linux]
[windows]
- Windows 10 or 11, 64-bit
- A VST3 host (REAPER, Ableton Live, FL Studio, Cubase, Studio One, Bitwig Studio, ...) or use the Standalone app
[/windows]
[macos]
- macOS 10.15 or newer, Apple Silicon or Intel (universal binaries)
- An AU or VST3 host (Logic Pro, GarageBand, Ableton Live, REAPER, ...) or use the Standalone app
[/macos]


INSTALL
-------
[linux]
Open a terminal in this folder and run:

    ./install.sh             installs the VST3 for your user (~/.vst3)
    ./install.sh --system    installs it for all users (/usr/lib/vst3; asks for sudo)

Or copy VST3/MOSAIC.vst3 into ~/.vst3/ by hand. The Standalone app runs straight from Standalone/MOSAIC.
To remove the VST3, run ./uninstall.sh (add --system if you installed system-wide).
[/linux]
[windows]
Copy the "MOSAIC.vst3" folder inside VST3\ into:

    C:\Program Files\Common Files\VST3\

Windows asks for administrator permission to copy there. The Standalone app (Standalone\MOSAIC.exe)
runs from anywhere. To remove MOSAIC, delete those files.
[/windows]
[macos]
1. Copy the plugins:
     AU/MOSAIC.component   ->  /Library/Audio/Plug-Ins/Components/
     VST3/MOSAIC.vst3      ->  /Library/Audio/Plug-Ins/VST3/
   The Standalone app (Standalone/MOSAIC.app) can go in Applications.
   (In Finder: Go > Go to Folder..., then paste the path.)
@MAC_QUARANTINE@
To remove MOSAIC, delete those files again.
[/macos]

Then rescan plugins in your host. In REAPER: Options > Preferences > Plug-ins > "Re-scan".
[macos]
Logic Pro and GarageBand pick up new AU plugins after a restart.
[/macos]


USING MOSAIC
------------
- Drag a .wav, .mp3 or .aif onto a pad. Click "L" on a pad and hit a pad on your controller to map it.
- Right-click a knob to MIDI-learn a CC for it.
- Projects and presets refer to your sample files. If you move them, the pads show MISSING:
  use Edit > "Relink missing samples" and pick the folder they are in now.


LICENCE AND SOURCE
------------------
MOSAIC is free software under the GNU Affero General Public License, version 3 (see LICENSE.txt).
It comes with ABSOLUTELY NO WARRANTY. The complete source code is at https://github.com/FenrirRhogar/MOSAIC
