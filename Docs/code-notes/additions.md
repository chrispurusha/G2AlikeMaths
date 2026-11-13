# additions/soundEngineAdditions.c - notes

Appended to the copy of `soundEngine.c` by `./sync-from-g2alike`; see `design.md` §11.

1. **One slot's morph.** `set_morph_one()` on the named slot's engine. The engine's own `sound_engine_set_morph()`
   reaches only the slots the keyboard plays, which suits a controller but not a patch's knob.
2. **One slot's readouts.** Meters, LEDs and voice counts are read from the engine of the selected slot. Each
   wrapper points the thread at the named slot for the one call, the way the engine's own per-slot loops do.
