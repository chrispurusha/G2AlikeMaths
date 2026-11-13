# additions/soundEngineAdditions.c - notes

Appended to the copy of `soundEngine.c` by `./sync-from-g2alike`; see `design.md` §11.

1. (Removed 2026-11-13: one slot's morph from its knob. The engine reads the knobs and their modes itself now,
   G2Alike's reference §26.4.)
2. **One slot's readouts.** Meters, LEDs and voice counts are read from the engine of the selected slot. Each
   wrapper points the thread at the named slot for the one call, the way the engine's own per-slot loops do.
3. (Removed 2026-11-13: an oversampling query, replaced by `g2am_latency()` working from the requested setting.)
4. **LFO link** (for ModAlike's Wide). A Phaser's LFO is a counter in its filter state (`gLadder[0][n][7]`, -1 to 1
   a cycle; the FX pass runs on voice 0); a Flanger's is the 24-bit word in its FX buffer state, drawn at load from
   its line. The link writes slot B's from slot A's plus the offset, called on the audio thread between blocks
   (g2alikemaths notes §8). Both then advance at the same rate, so the gap holds through a change of Rate. Jumping
   the phase jumps the sweep: an audible step when the offset is changed.
