**EQUALIZERLAB**
Free 24-band mixing/mastering EQ for every major DAW

Available as VST3 (Windows + Mac), AU (Mac) and a standalone app

Built from the RetuneLab codebase, EqualizerLab is modelled on FabFilter Pro-Q 2: a big interactive display, a real-time
spectrum analyser, three processing modes, Mid/Side and Left/Right bands and EQ Match. Free, no licensing BS.

**FEATURES**

- Up to 24 bands, added with a double-click anywhere on the display
- 8 filter shapes: Bell, Low Shelf, Low Cut, High Shelf, High Cut, Notch, Band Pass, Tilt Shelf
- Cut and band pass slopes from 6 to 96 dB/oct; resonant cuts (raise Q above 0.71)
- Per-band stereo placement: Stereo, Left, Right, Mid, Side
- Zero Latency, Natural Phase (analog-matched, no cramping near 20 kHz) and Linear Phase (5 resolutions) modes
- Real-time spectrum analyser: Pre, Post, Pre + Post, sidechain spectrum, Freeze (peak hold), resolution, speed, tilt, range
- Spectrum Grab: hover a peak in the analyser and drag it to create a band right on it
- EQ Match: capture a reference (sidechain or a track you play through it) and the current track, then match with 1-24 bands
- Band Solo to hear just the part of the sound a band affects
- Gain-Q interaction (bells narrow as you boost/cut, like an analog console)
- Gain Scale (-100% to 200%), Output gain, Output pan, Polarity, Auto Gain (level-matched bypass), Bypass
- Display range +/-3, 6, 12 or 30 dB, optional piano roll with note readouts
- Undo / Redo, A/B compare with Copy, 25 factory presets, save/load your own presets
- Multi-select (Ctrl/Cmd-click or drag a box) and move several bands together
- Resizable window, every control host-automatable

**FORMATS / DAWS**

- VST3 (Windows + Mac): Ableton Live, FL Studio, Cubase/Nuendo, Studio One, Reaper, Bitwig, Cakewalk, and other VST3 hosts
- AU (Mac): Logic Pro, GarageBand (also Ableton, Reaper, Studio One on Mac)
- Standalone app (Windows + Mac)
- Pro Tools (AAX) is not included: it needs Avid's AAX SDK and a paid PACE signing account
- Mono or stereo tracks, optional stereo sidechain input (for the analyser and EQ Match)

The GitHub Actions build makes EqualizerLab-Windows (VST3 + .exe) and EqualizerLab-Mac (VST3 + AU + .app), each with an INSTALL.txt.

**USING IT**

- Double-click empty space: new band (Bell; Low Cut at the far left, High Cut at the far right). Right-click: add any shape.
- Drag a band: left/right = frequency, up/down = gain (Q for cuts, notches and band passes). Shift = fine, Ctrl/Cmd = lock to one axis.
- Mouse wheel over a band: Q. Shift + wheel: gain.
- Alt-click a band: switch it on/off. Double-click a band: delete it. Right-click a band: shape, slope, stereo placement, solo, reset, delete.
- Ctrl/Cmd-click or drag a box to select several bands; they move together. Delete key removes the selection, Ctrl/Cmd+A selects all.
- Ctrl/Cmd+Z undo, Ctrl/Cmd+Shift+Z or Ctrl/Cmd+Y redo.
- The band panel at the bottom edits the selected band; < > step through bands.
- Analyzer button: analyser mode, sidechain spectrum, Freeze, Spectrum Grab, resolution, speed, tilt, range and the piano roll.
- Linear Phase adds latency (shown under the mode box); your DAW compensates automatically.
