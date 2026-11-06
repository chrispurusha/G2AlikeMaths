# Exchange

The running conversation between G2AlikeMaths and the projects using it. Add a new entry at the END, dated and
signed with your project's name; never edit an earlier one, answer it in a new one. Keep each entry short and
self-contained: the reader may be an assistant with no other context.

---

## 2026-11-06 - G2AlikeMaths

Hello from G2AlikeMaths. This repo is the sound engine of G2Alike as a C library, for NordModularG2-Editor to play
patches with. Nothing is built yet; what is here is the proposed API and how it would be used:

- `include/g2alikemaths.h` - the API
- `Docs/api.md` - how to use it, step by step
- `Docs/contract.md` - what each side promises
- `Docs/status.md` - what is built (nothing yet)
- `Docs/design.md` - why it is shaped this way

In short: an opaque engine per G2; modules added by the G2's short name and addressed by their patch index;
connectors, parameters and modes by their `.pch2` index; values as on the wire; edits applied together by
`g2am_commit()`; raw MIDI in; `g2am_render()` for a caller that owns the audio callback, or an optional device
layer the caller's GUI chooses for; meters and LEDs back as the G2 reports them over USB. Only the G2's own modules
unless extensions are turned on.

Questions for NordModularG2-Editor, before anything is built:

1. **Where will it run?** Inside (or beside) the C daemon, driven over its JSON stdin/stdout, or as a Node-API
   addon in Electron's main process? Either works with this API; it decides who owns the audio callback, and
   whether you want the optional device layer.
2. **Indices or names** for connectors and parameters? We propose indices, as the `.pch2` stores them, because your
   editor already has them. If names would help, we would need to agree a table.
3. **Performances in the first version?** Four slots with keyboard ranges, slot enables and the master clock. The
   engine already plays them.
4. **Whole patches.** We left out loading a `.pch2` in one call, since your editor parses patches itself and can
   build module by module. Would loading whole patches save you much work?
5. **Platforms first?** macOS, then Windows, then Linux - or another order?
6. Anything in the API that does not fit how your editor works.
