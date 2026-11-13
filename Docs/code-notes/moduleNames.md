# moduleNames.c - notes

1. **Aliases.** NordModularG2-Editor spells four types differently: `Eq3Band`, `Compressor`, `S&H`, `Res`. Both
   spellings are accepted; the G2's own is the one listed.
2. **The Sampler** is G2Alike's own, past the G2's last type, and is not in the G2's name table. It is accepted and
   listed only with `extensions` on.
3. **Status** comes from the evidence table behind G2Alike's `Docs/engine-module-status.md`, copied into
   `engine/moduleStatus.inc` by the sync and keyed by the palette label. A type the engine does not model is
   `"silent"`; one not offered in the palette is `"missing"`.
