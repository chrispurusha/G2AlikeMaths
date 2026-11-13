# Makefile - notes

1. **One object, only `g2am_*` global.** The engine's ~480 internal names (`get_module`, `init_patch`, ...) would
   otherwise be visible to every caller. Its copy of SynthLib's log globals (`gSynthLibLogLevel`,
   `gSynthLibLogSink`) collided with SynthLib's own the first time ModAlike linked both. macOS: `ld -r` with
   `-exported_symbol '_g2am_*'`; elsewhere `objcopy --keep-global-symbol` (not yet tried on Linux). The internal
   `g2am_engine_*` and `g2am_type_*` helpers match the pattern too and are visible, though not in the header.
2. **`ARCHS`** builds universal (`ARCHS="-arch arm64 -arch x86_64"`), and **`BUILD`** sends the output to another
   folder, so a plug-in's universal build (`build-universal/`) leaves the native one alone. The combine step needs
   the deployment target as well, or the linker warns the object is newer than macOS 12.
