# MobileDAW 2.0 — simpler touch screen

Owner's request: users find 1.0 (Ableton-style) hard to understand. 2.0 is a second, switchable frame inspired by the
workflow and gestures of mobile DAWs, with MobileDAW's own look. 1.0 stays as it is.

- **Inspiration, not a copy.** The workflow ideas come from the owner's reference screenshots: a bottom transport bar, an
  icon column per track, bubbles on a tapped clip, a main menu sliding in from the right, and the range menu.
- **Our own look.**
  - Icons are outline icons drawn for MobileDAW.
  - Buttons are rounded squares.
  - Colours come from the active theme.
  - The logo is MobileDAW's own.
  - Labels are in Indonesian.
  - No store or cloud tabs, because the app works offline.
- **Same engine and data.** Projects (.mdaw) are unchanged, and the timeline canvas, editors, devices and mixer are shared.
  Switching between 1.0 and 2.0 autosaves the project and reloads the page.

## Switching

- **1.0 → 2.0:** project menu (⋮) → "Tampilan 2.0 (lebih sederhana)…".
- **2.0 → 1.0:** logo button → PENGATURAN → Versi tampilan → "1.0 Klasik".
- The setting is stored as `prefs.ui` (`'v1'` is the default, `'v2'` turns 2.0 on).

## Phase 1 (done): main screen — `js/ui2/shell.js`, `css/ui2.css`

| Part | Behaviour |
|---|---|
| **Bottom bar, left** | MIXER (mixer view), RACK (device panel), SUARA (browser) |
| **Bottom bar, centre** | REKAM, AWAL, Play / Pause, BPM, METRO, LOOP. BPM: tap opens the Tempo window, a vertical drag changes the tempo. |
| **Bottom bar, right** | Undo, Redo, CPU % / memory, keyboard (Keys panel) |
| **Track column (left)** | One coloured chip per track, showing the track's kind icon. Tap a chip to widen or narrow the column. The wide column shows the name, on/off, Solo, pan (vertical drag) and volume (horizontal drag); a double tap resets pan or volume. Long-press a chip for the track menu. A vertical drag scrolls the tracks. The "+" button adds a track. |
| **Snap (magnet, top left)** | Tap turns snap on or off; long-press shows the grid menu. |
| **Tap a clip** | Bubbles appear: Salin (copy after the clip), Hapus, Snap (edges to the grid), Edit (editor), Lainnya. |
| **Lainnya** | Potong di titik ketuk, Lepas loop, Gabung dengan klip berikutnya, Bisukan/Aktifkan, Ganti nama, and all clip commands. |
| **Double tap** | On a clip: editor (unchanged). On empty space: a new clip. |
| **Ruler range** | Drag sideways to select a range. Its menu has the 1.0 items (Deselect, Insert space, Duplicate, Delete, Delete space, Trim) plus Salin / Pindah ke playhead. |
| **Logo button (top right)** | Opens the main menu. |
| **Main menu tabs** | LAGU (new, open, import, recent songs), PROYEK (save, save as, quick render, song info, tempo − / +), EKSPOR, PENGATURAN (UI version, theme, column, follow, snap, all settings), BANTUAN (tutorial, about, gesture list) |

In 2.0 the timeline runs in `TL.setV2(true)` mode:

- The canvas has no header column and no bottom time ruler.
- A tapped clip calls `hooks.clipTap` instead of opening the context menu.
- The selected clip gets a white frame.
- The shell reads the row geometry through the `TL.v2*` exports.

Test: `tools/ui2-test.mjs`. It covers the bar, the column, the bubbles, undo, volume / mute, the range → playhead command, the menu, and switching back to 1.0 with the project kept.

## Next phases

2. **Rack panel from the right.** Module cards (on / off, icon, name, preset ▸, ☰), a module list from the left with search, rack presets, and Sends.
3. **Piano roll.** Note bubbles (Salin / Hapus / Snap / Lainnya), and a Lainnya menu with Legato, Snap ke skala, Humanize, Double up / down, Toggle slide, set length, Transpose, and a ↑ / ↓ stepper. Also a Vel / Pan panel.
4. **Step sequencer.** Big channel names, Swing, and Mixer / Sample / Envelope / Seq tabs.
5. **Mixer.** Vertical strips with a pan ring and on / Solo buttons, plus a split view with the keyboard (key, scale, keyboard type).
