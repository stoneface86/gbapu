# gbapu
Gameboy APU emulator

This is a static library for a gameboy APU emulator. This library was written
for use in [trackerboy][trackerboy-url], a gameboy music tracker, but can also
be used in other projects. The library produces high quality audio using
bandlimited synthesis.

The goal of this emulator is to produce quality sound, that sounds "close enough"
to real hardware. Most obscure behaviors and glitches that are present on the
actual hardware will not be implemented in this library.

## Building

Build the library using CMake and a C++ compiler that supports the C++17 standard.
The only dependency is blip_buf (v1.1.0) and is included in this repository.

To build the demos, set the `GBAPU_DEMOS` option to ON when configuring. The demo
program creates a bunch of wav files demonstrating the use of the emulator.

## Usage

Use the `Apu` class for modifying registers and getting audio samples.

```cpp
// samplerate = 48000 Hz, 100ms buffer
gbapu::Apu apu(48000, 4800);
```

When your emulator accesses a sound register via memory mapped IO, just call
`Apu::readRegister` or `Apu::writeRegister`. These methods take the register
number to read/write, use the `Apu::Reg` enum or convert the memory address
by AND'ing with 0xFF.

Then run the APU for the necessary amount of cycles.

```cpp
// run the APU for a given number of cycles
apu.step(cycles);
```

The Buffer must be read before it fills up, to read from the buffer
```cpp
apu.endFrame(); // must call first before reading samples
size_t samples = apu.availableSamples();
// output is a buffer of interleaved int16_t audio samples
apu.readSamples(output, samples);
```

### Example

The following example demostrates how to emulate the following:
```asm
xor a
ldh [rNR12], a      ; set CH1 envelope to 0
```

Step the APU the required number of cycles first, (changes occur after the
register write)
```cpp
apu.step(4); // 4 cycles, xor a is 1, ldh is 3
```

Then do the register write
```cpp
apu.writeRegister(Apu::REG_NR12, 0, 0); // a was zero, step 0 cycles
```

Alternatively, you can call `writeRegister` with the amount of cycles to step
before doing the write. This way you don't have to call `step` first.

```cpp
apu.writeRegister(Apu::REG_NR12, 0, 4); // a was zero, step 4 cycles first
```

By default, the read and write register methods will step 3 cycles before
accessing the register. The default is 3 since the `ldh` instruction takes
3 cycles to execute and is the most common way to access sound registers.

## Notes

 * Step the APU alongside your emulator, while periodically reading samples
   from the buffer.
 * `Apu::endFrame` must be called before the buffer fills up completely, if
   you do not need to read samples, just clear the buffer.
 * Performance can be improved by lowering the quality setting of the Apu.
   There are 3 quality settings: low, medium and high. Low quality will use
   linear interpolation on all channels. Medium uses bandlimited synthesis
   for CH1 and CH2 and linear interpolation for CH3 and CH4. High quality uses
   bandlimited synthesis on all channels. Default quality is medium.
 * The Apu has a volume setting, default is 100% or 0.0 dB. Each channel gets
   25% of this volume setting. Note that clipping may occur at 100% due to
   overshoots and/or from high pass filtering.
 * The Buffer size and samplerate can be changed at any time, just call
   `Apu::resizeBuffer()` after making the change(s). Also call `Apu::endFrame`
   beforehand.

## References

Reference material used when writing this library

 * [Pan Docs][pan-docs-url]
 * https://gbdev.gg8.se/wiki/articles/Gameboy_sound_hardware
 * [Official Gameboy programming manual][gameboy-manual-url]

## Not implemented

The following is a list of behavior that is not implemented by this emulator:
 * Vin terminal mixing
 * Zombie mode: more research is needed

# License

This project is licensed under the MIT License - See [LICENSE](LICENSE) for
details.

[trackerboy-url]: https://github.com/stoneface86/trackerboy
[obscure-behavior-reference]: https://gbdev.gg8.se/wiki/articles/Gameboy_sound_hardware#Obscure_Behavior
[pan-docs-url]: https://gbdev.io/pandocs/#sound-controller
[gameboy-manual-url]: https://archive.org/download/GameBoyProgManVer1.1/GameBoyProgManVer1.1.pdf