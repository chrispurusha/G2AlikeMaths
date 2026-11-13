# g2alikemaths.c - notes

1. **96 kHz, serial.** Economy (the graph at the device rate) is off, so every module runs at the G2's own rate.
   The voices render on the calling thread; G2Alike's voice worker thread is not used, so a render is
   deterministic and the library starts no audio threads of its own.
2. **`g2am_prepare()`** restarts the engine at the new rate and grows the scratch buffer. It frees memory the audio
   thread uses, so the caller must have stopped rendering.
3. **Morph knobs** are the engine's own since G2Alike's §26.4 (2026-11-13): each group follows its knob in Knob mode
   (settings parameters 8-15) and its controller otherwise. The façade's earlier workaround - pushing knob values into
   the controller amounts at each commit - went with it.
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
7. **Latency.** Measured, not derived: an impulse through 2-In -> 2-Out comes out 31 frames late at 44.1 and
   48 kHz, and on the same frame at 88.2 kHz and above, where the graph runs at the device's rate. `make test`
   measures it at each rate against `g2am_latency()`. It is worked out from the rate and the economy setting asked
   for - the audio thread applies a change of economy at its next block, so asking the engine would lag.
8. **LFO links are made on the audio thread.** The engine renders one slot's whole block, then the next; a link
   copied from a control thread mid-block can catch slot A a block ahead of slot B, and the Phaser's sides came out
   a block apart (ModAlike's test caught it). So `g2am_lfo_link()` only posts the request, and `g2am_render()`
   makes it before the block, when both slots stand at the same sample - re-making it on 8 blocks, so a patch
   rebuild that resets the state in between does not lose it, and waiting while a module has not started.
