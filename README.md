
[EqualizerLab-Windows.zip](https://github.com/user-attachments/files/33684630/EqualizerLab-Windows.zip)

**EQUALIZERLAB**
Free 24-band EQ for every major DAW

Available as VST3 (Windows + Mac), AU (Mac) and a standalone app

The main goal when creating this was to emulate FabFilter Pro-Q 2 and offer a clean, powerful mixing and mastering EQ for free
with no licensing BS. Built on the same foundation as RetuneLab.

**FEATURES**

- Up to 24 bands
- 8 filter shapes: Bell, Low Shelf, Low Cut, High Shelf, High Cut, Notch, Band Pass, Tilt Shelf
- Slopes from 6 to 96 dB/oct on cuts and band pass
- Resonant cuts (raise Q above 0.71 for a bump at the cutoff)
- Per-band stereo placement: Stereo, Left, Right, Mid, Side
- Zero Latency mode
- Natural Phase mode (analog-matched curves, no cramping near 20 kHz)
- Linear Phase mode with 5 resolutions (Low to Maximum)
- Real-time spectrum analyzer: Pre, Post or Pre + Post
- Sidechain spectrum view
- Analyzer Freeze (peak hold), Resolution, Speed, Tilt and Range settings
- Spectrum Grab: grab a peak in the analyzer and drag it to make a band
- EQ Match: match your track to a reference with 1-24 bands
- Band Solo: hear only what a band is affecting
- Gain-Q interaction (bells narrow as you boost or cut)
- Gain Scale (-100% to 200%) to scale every band at once
- Output Gain, Output Pan and Polarity
- Auto Gain for level-matched before/after comparisons
- Bypass
- Display range +/-3, 6, 12 or 30 dB
- Piano roll with note readouts
- Undo / Redo
- A/B compare with Copy
- 25 factory presets plus save/load your own
- Multi-select and move several bands together
- Resizable window
- Every control is host-automatable

**FORMATS / DAWS**

- VST3 (Windows + Mac): Ableton Live, FL Studio, Cubase/Nuendo, Studio One, Reaper, Bitwig, Cakewalk, and other VST3 hosts
- AU (Mac): Logic Pro, GarageBand (also Ableton, Reaper, Studio One on Mac)
- Standalone app (Windows + Mac)
- Pro Tools (AAX) is not included: it needs Avid's AAX SDK and a paid PACE signing account
- Mono or stereo tracks, with an optional stereo sidechain input for the analyzer and EQ Match

The GitHub Actions build makes EqualizerLab-Windows (VST3 + .exe) and EqualizerLab-Mac (VST3 + AU + .app), each with an INSTALL.txt.

**WORKFLOW**

Put EqualizerLab on any track or bus and press play. The analyzer shows the sound before (grey) and after (blue) the EQ.

Adding and editing bands

- Double-click space to add a band (Bell, or Low Cut at the far left and High Cut at the far right).
- Right-click empty space to add any shape at that spot.
- Drag a band: left/right = frequency, up/down = gain (Q for cuts, notches and band passes).
- Shift while dragging = fine adjust. Ctrl/Cmd while dragging = lock to one direction.
- Mouse wheel over a band = Q. Shift + wheel = gain.
- Alt-click a band to switch it on/off. Double-click a band to delete it.
- Right-click a band for shape, slope, stereo placement, solo, reset gain/Q, invert gain and delete.
- The band panel at the bottom edits the selected band. Use < > to step through bands.

Selecting

- Ctrl/Cmd-click bands or drag a box around them to select several. They move together.
- Ctrl/Cmd+A selects all, Delete removes the selection, Esc deselects.
- Ctrl/Cmd+Z undo, Ctrl/Cmd+Shift+Z or Ctrl/Cmd+Y redo.

Spectrum Grab

- Hover near a peak in the analyzer until a circle appears, then click and drag down to cut it (or up to boost it).

EQ Match

1. Click EQ Match.
2. Pick the reference: the sidechain input (route the reference track there in your DAW), or "Capture this track".
3. Switch on Capture Reference while the reference plays, then switch it off.
4. Switch on Capture Current while your track plays, then switch it off.
5. Set the number of bands and the amount, then click Match.

Processing modes

- Zero Latency: lowest CPU, no delay. Good for tracking and live use.
- Natural Phase: analog-style curves and phase with no delay. A good default for mixing.
- Linear Phase: no phase shift at all; adds latency (shown under the mode box). Your DAW compensates automatically. Best for mastering and parallel signals.

Output section

- Gain Scale shrinks or exaggerates every band at once (negative values flip the EQ).
- Auto Gain keeps the volume the same so bypass comparisons are fair.
- A/B stores two settings; Copy puts the current one into the other slot.
- Presets: < > step through them, click the name for the full list, Save stores your own.
