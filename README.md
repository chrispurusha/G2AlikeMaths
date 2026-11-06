# G2AlikeMaths

The sound engine of [G2Alike](https://github.com/chrispurusha/G2Alike) as a stand-alone C library: a clean-room
reproduction inspired by the Nord G2's sound engine, for other programs to build and play G2 patches with.

An unofficial project. Not affiliated with or endorsed by Clavia.

**State: design and API contract. Nothing is built yet.** See [Docs/status.md](Docs/status.md) for what works.

## What it does

- Builds a patch from the G2's own modules, named as the G2 names them (`"OscB"`, `"FltClassic"`, `"2-Out"`),
  with cables, parameters, modes, variations and morphs addressed as the `.pch2` file and the USB protocol
  address them.
- Plays it: MIDI in, the G2's In 1-4 in, Out 1-4 out.
- Reports what the G2's panel would show: meters, LEDs, variation changes, voices, CPU load.
- Leaves the audio device and MIDI ports to the caller's own GUI, with an optional device layer for callers that
  have no real-time audio of their own.
- Plays only the modules the G2 has, unless the caller turns on G2Alike's extensions.

Its first user is Jan den Besten's
[NordModularG2-Editor](https://github.com/JanBurp/NordModularG2-Editor).

## Documents

| | |
|---|---|
| [include/g2alikemaths.h](include/g2alikemaths.h) | The API, every call's contract in its comment |
| [Docs/api.md](Docs/api.md) | How to use it, step by step |
| [Docs/contract.md](Docs/contract.md) | What the library promises, and what it expects of the caller |
| [Docs/exchange.md](Docs/exchange.md) | The running conversation between this project and the projects using it |
| [Docs/status.md](Docs/status.md) | What is built and working |
| [Docs/design.md](Docs/design.md) | Why it is shaped as it is, and what comes later |

## Working with LLMs

Development started as a solo project and now uses LLM-assisted development as a tool for research, coding,
debugging and experimentation. The documents above are written so that an assistant working on a calling
project can read them, check where things stand, and leave its questions in [Docs/exchange.md](Docs/exchange.md).

## Support

If this is useful to you, you can [buy me a coffee](https://www.buymeacoffee.com/chrispurusha).

## Licence

GPLv3. See [LICENSE](LICENSE). Third-party code: [THIRD_PARTY.md](THIRD_PARTY.md).
