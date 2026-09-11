# Patch Editor UI refresh — 2026-09-10f

The main toolbar now contains Connect, Apply changes, Save to pedal, Library,
and Settings. Patch name/number, connection state and an edited-state indicator
sit beside them. The six slots are the main workspace; offline users see an
explanation of how to start.

Library is a modal side drawer with Pedal memory and My Library tabs. Arrow keys
switch tabs; dragging over a tab reveals its drop targets. Import/export and
copy/paste live in Share a patch. Pedal memory and browser storage remain labeled.

Settings groups Appearance, MIDI controller, Advanced and Diagnostics. Existing
LCD/P4/P3 skins are preserved. Bank backup/restore sits under Advanced. Diagnostic
probes are hidden from the effect catalogue by default, with an explicit checkbox
to reveal them; already-selected probes remain visible. Empty slots say Add effect,
and slot switches have visible On/Off text.

Existing control elements and MIDI handlers are retained when moved into drawers.
Native dialogs provide modal focus, Escape dismissal and focus return. Notices
appear inside an open drawer. Save confirmation text is shorter and uses the new
label; a failed reload no longer claims a confirmed save.

Checked in the local browser: clean start, Settings groups, amber skin, Library
keyboard tabs, paste into Rooms, knob Home endpoint and focus, loaded-slot layout.
Eight MIDI regression tests, inline script syntax, release metadata consistency,
and whitespace checks pass. No pedal connected during this UI pass. Small-screen
CSS is included; physical mobile and assistive-technology validation remain.

## 2026-09-10g: bottom notifications

Replaced the red, stacking top banners with one neutral, theme-aware bottom
status strip. It retains the latest notice until replaced or cleared; history
remains in Diagnostics. Reserved bottom spacing keeps controls clear. The strip
follows open modal drawers so it remains readable and dismissible there too.
Browser checked invalid-import notice, opening Settings, clearing the notice and
closing Settings. Eight MIDI regression tests and script syntax checks pass.
