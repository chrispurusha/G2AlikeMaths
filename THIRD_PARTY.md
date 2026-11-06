# Third-Party Software

## G2Alike

- **What**: the sound engine itself, copied from [G2Alike](https://github.com/chrispurusha/G2Alike) into `engine/`
  by `./sync-from-g2alike`, which records the G2Alike commit it came from (Docs/design.md §8).
- **License**: GPLv3, the same author.

## SynthLib

- **What**: six header files the engine's types include (`synthlibDefs.h`, `synthlibTypes.h`, `synthlibGlobals.h`,
  `synthlibQueue.h`, `geometry.h`, `floatingPanel.h`), copied with the engine. No SynthLib code is compiled in.
- **Source Code**: [https://github.com/chrispurusha/SynthLib](https://github.com/chrispurusha/SynthLib)
- **License**: GPLv3, the same author.

## miniaudio (planned, for the optional audio device layer)

- **Description**: A single-file audio playback and capture library: CoreAudio, WASAPI and ALSA/PulseAudio from
  the same code.
- **Website**: https://miniaud.io
- **Source Code**: https://github.com/mackron/miniaudio - to be vendored as one header
- **License**: Public domain (Unlicense) or MIT No Attribution, at the user's choice

---

For detailed license terms, please refer to the license files provided with each library or visit their official
license pages.
