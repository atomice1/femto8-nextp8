# femto8 Editor Reference

The built-in editor is accessed from the emulator by pressing Escape (or the
menu button on NEXTP8). The six editor tabs are shown across the top of the
screen. Click a tab or use keyboard shortcuts to switch between them.

Tab spacing between tabs is 2 px. The active tab is highlighted.

Global shortcuts available in editor tabs:

| Key | Action |
|-----|--------|
| F1 | Switch to Code tab |
| F2 | Switch to Sprite tab |
| F3 | Switch to Map tab |
| F4 | Switch to SFX tab |
| F5 | Switch to Music tab |
| F6 | Quit editor |
| F7 | Open REPL |
| Ctrl+R | Run cart |
| Ctrl+S | Save cart |
| Ctrl+A | Save As (except where overridden by tab-specific behavior) |
| Ctrl+Z / Ctrl+Y | Undo / Redo (tab-local) |

---

## Code Editor

The code editor displays the cart Lua source. The cursor blinks; a selection is
shown as a highlighted region.

### Navigation

| Key | Action |
|-----|--------|
| Arrow keys | Move cursor one character / line |
| Ctrl+Left / Ctrl+Right | Move one word left / right |
| Home | Move to start of line |
| End | Move to end of line |
| PgUp | Scroll up one page |
| PgDn | Scroll down one page |
| Ctrl+Home | Jump to first line |
| Ctrl+End | Jump to last line |

Hold Shift with navigation keys to extend the selection.

Selection behavior details:

- Selection uses an exclusive end column internally.
- Shift+Home and Shift+End extend selection to line start/end.
- Shift+PgUp and Shift+PgDn extend selection by page movement.

### Editing

| Key | Action |
|-----|--------|
| Type characters | Insert at cursor |
| Backspace / Delete | Delete left / right |
| Enter | Insert newline |
| Tab | Insert two spaces |
| Ctrl+A | Select all |
| Ctrl+C | Copy selection |
| Ctrl+X | Cut selection |
| Ctrl+V | Paste |
| Ctrl+Z | Undo |

---

## REPL

The REPL tab provides an interactive Lua prompt. When first opened, a Lua state
is created automatically so expressions can be evaluated immediately (no need to
load a cartridge first).

Input keys:

| Key | Action |
|-----|--------|
| Enter | Execute current input line |
| Backspace | Delete one character |
| Printable ASCII | Insert character |
| Escape | Leave REPL |

Behavior notes:

- Prompt lines are recorded as >input.
- Successful expression/statement output is not echoed to history.
- Errors are shown in history with inline location prefixes stripped.
- The REPL overlay does not paint a full solid background, so cart output
  remains visible behind REPL text.
- REPL history is not persisted across sessions.

---

## Sprite Editor

The sprite editor has two modes: sheet mode (browse all sprites) and edit mode
(pixel paint a single sprite).

### Sheet Mode

The sprite sheet shows up to 13 rows of 16 sprites at a time. Scroll the sheet
with the mouse wheel or PgUp/PgDn.

- Click a sprite to select it (highlighted in white).
- Click the selected sprite again to enter edit mode.
- The flags row sits above the status bar and shows the 8 flags for the
  selected sprite. Click a flag to toggle it.
- Flag boxes are taller than glyph height for easier mouse interaction.
- PgUp/PgDn scroll by 13 rows.
- Clicking a different sprite does not auto-scroll the sheet.

Status bar shows: spr:NNN  1/2:col  enter:edit

### Edit Mode

The left 64x64 area shows the sprite zoomed 8x. The right panel shows a 16-color
palette (2x8), the 8 sprite flags, and a two-row tool palette.

#### Tools

| Icon | Tool | Shortcut |
|------|------|----------|
| Pencil | Draw single pixels | P |
| Bucket | Flood fill | F |
| Dashed rect | Select rectangle | S |
| Diagonal line | Draw line | L |
| Hollow rect | Draw rectangle | click tool |
| Oval | Draw ellipse | click tool |
| Four-arrow | Pan / shift pixels | click tool |

- Left-click paints or applies current tool.
- Right-click samples pixel color (eyedropper) for all tools.
- Ctrl+click with pencil/fill replaces all pixels of clicked color with current
  color.
- Active tool name is shown below the two tool rows.

#### Selection tool

Drag to create a rectangular selection (white outline). While a selection is
active:

- Pencil paints only inside the selection.
- Pan moves only the selected region.
- Switching tools does not clear the selection.
- Backspace clears selected pixels.
- Escape clears selection.

#### Pan tool

Drag the zoomed sprite area to shift pixels.

- Without a selection, shifts the whole sprite with wrap.
- With a selection, moves only selected pixels.
- Selection pan is non-destructive during drag preview (snapshot-based).

#### Mini sprite sheet (edit mode)

The bottom portion of the left panel shows a mini view of the sprite sheet
centered on the current sprite.

- Click a sprite to switch without leaving edit mode.
- Mouse wheel over mini sheet scrolls the mini viewport.

#### Keyboard shortcuts

| Key | Action |
|-----|--------|
| Arrow keys | Move 1 pixel in zoomed view |
| Shift+Arrows | Shift sprite pixels by 1 |
| Space / Enter | Paint current pixel |
| Backspace | Clear current pixel (or selection) |
| Ctrl+G | Toggle pixel grid overlay |
| Ctrl+A | Select all pixels |
| Ctrl+C / Ctrl+V | Copy / paste whole 8x8 sprite |
| H / V / R | Flip H / Flip V / Rotate CW |
| P / F / S / L | Select tool: pencil/fill/select/line |
| 1 / 2 | Previous / next draw color |
| 0-7 | Toggle sprite flags |
| Q / W | Previous / next sprite |
| Tab | Toggle between sheet/edit modes |
| Escape | Clear selection, or exit edit mode |

Status bar shows: spr:NNN  px:X,Y

---

## Map Editor

The map viewport shows a 16x12 tile window into the 128x32 tile map. Each tile
is 8x8 pixels.

### Placing tiles

- Left-click/drag on the map to place selected sprite.
- Right-click on a map cell to pick the sprite from that cell.

### Navigation

| Key | Action |
|-----|--------|
| Arrow keys | Move cursor one tile |
| PgUp | Scroll viewport up one full page (12 rows) |
| PgDn | Scroll viewport down one full page |
| Space + drag | Pan viewport |

### Tile strip

The strip at the bottom shows two rows of the sprite sheet. Use Q/W to move
through strip rows.

Status bar: m:X,Y  spr:NNN  rR

---

## SFX Editor

The SFX editor has two views:

- List view: shows all 64 sound effects.
- Edit view: shows 32 notes for selected SFX.

### Navigation

| Key | Action |
|-----|--------|
| Up / Down | Move between SFX (list) or notes (edit) |
| Left / Right | Move columns in edit view |
| PgUp / PgDn | Fast list navigation |
| Home / End | Jump to first / last note (edit) |
| Enter | Open selected SFX in edit view |
| Escape | Return from edit view to list view |
| Tab | Cycle SFX submodes |

### Status text

Common bottom-line hints include:

- enter:edit in list mode
- lr:sel enter:set tab:mode in tracker submode

---

## Music Editor

The music editor lists all 64 patterns. Each pattern has 4 SFX channels and
three flags: L (loop start), E (loop end), S (stop).

Flags are color-coded in the flags column (L green, E red, S orange).

### Navigation

| Key | Action |
|-----|--------|
| Up / Down | Select pattern |
| Left / Right | Move between flag column and 4 SFX columns |
| Space | Play/stop current pattern |

### Editing

- In flag column (col 0), press L/E/S to toggle flags.
- In SFX columns (cols 1-4), type digits to enter SFX index (0-127).
- Two digits commit immediately.
- A pending single digit is committed when you navigate away with arrow keys.
- Escape cancels pending SFX numeric input without leaving the tab.

Status bar shows either:

- ud:pattern  lr:col  l/e/s:flags (flag column), or
- current SFX typed input state in SFX columns.
