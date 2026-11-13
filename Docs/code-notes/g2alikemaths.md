# g2alikemaths.c - notes

1. **96 kHz, serial.** Economy (the graph at the device rate) is off, so every module runs at the G2's own rate.
   The voices render on the calling thread; G2Alike's voice worker thread is not used, so a render is
   deterministic and the library starts no audio threads of its own.
2. **`g2am_prepare()`** restarts the engine at the new rate and grows the scratch buffer. It frees memory the audio
   thread uses, so the caller must have stopped rendering.
3. **Morph knobs.** The engine takes a morph group's amount from controllers only. At each commit the façade gives
   each slot the knob values of the variation it plays, but only the knobs whose value changed since the last
   commit, so a wheel already moved is not snapped back by an unrelated edit.
4. **Events.** MIDI and keys from any thread go into a ring claimed with one atomic add and published by a sequence
   number, as the engine's own note queue does. Each render takes what is ready (up to 256), sorts it by frame
   offset keeping arrival order, and renders up to each offset before playing it. Events posted from a control
   thread carry offset 0 and land at the start of the next block. With no render running they wait.
5. **The rebuild worker.** A controller moving a morph or a performance setting changes the snapshot the audio
   thread plays, which only a rebuild makes. A thread per engine checks every 4 ms, as G2Alike's plug-in does, and
   rebuilds under the database's read lock. Commit and worker share one mutex.
6. **The keyboard's channel** is the selected slot's in patch mode and the global channel in a performance. MIDI on
   it plays as the keyboard (`note_stack`, morph controllers, bend). Any other channel reaches the slots set to it.
   Every channel reaches CtrlRcv / NoteRcv.

Edits are checked when they are called, against a shadow of which module type sits at each index, and applied in
order under the database's write lock at commit, so a commit cannot fail part way.
