# gbapu
Gameboy APU emulator

This is a static library for a gameboy APU emulator. Based off of trackerboy's emulation based synthesizer.
I'm rewriting it here as an actual emulator with accuracy as a priority. On completion this library
will replace Trackerboy's synthesizer.

## Usage

Use the `Apu` class for modifying registers and the `Buffer` for getting
audio samples.

```cpp
// samplerate = 48000 Hz
Buffer buffer(48000);
Apu apu(buffer);
```

When your emulator accesses a sound register via memory mapped IO, just call
`Apu::readRegister` or `Apu::writeRegister`. These methods have overloads for
either the register number or the register's mapped memory address.

Then run the APU for the necessary amount of cycles.

```cpp
// run the APU for a given number of cycles
apu.step(cycles);
```

The Buffer must be read before it fills up, to read from the buffer
```cpp
apu.endFrame(); // must call first before reading samples
size_t samples = buffer.available();
// output is a buffer of interleaved int16_t audio samples
buffer.read(output, samples);
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
apu.writeRegister(Apu::REG_NR12, 0); // a was zero
```

## Notes

 * Step the APU alongside your emulator, while periodically reading samples
   from the buffer.
 * `Apu::endFrame` must be called before the buffer fills up completely, if
   you do not need to read samples, just clear the buffer.
 * Performance can be improved by lowering the quality setting of the buffer.
   Lower quality settings result in a linear interpolated step instead of a
   bandlimited one. Default quality setting is the highest.
 * The Buffer has a volume setting, default is 100% or 0.0 dB
 * The Buffer size and samplerate can be changed at any time, just call
   `Buffer::resize()` after making the change. Just be sure to call `Apu::endFrame`
   first.

## Obscure behavior

The following is a list of obscure behavior / hardware bugs present in the gameboy APU. In order to
ensure accurate emulation, these behaviors will be emulated as well by the emulator. [Reference](https://gbdev.gg8.se/wiki/articles/Gameboy_sound_hardware#Obscure_Behavior)

 2. First nibble in waveram is not played first on retrigger

This is due to the waveposition being incremented first on clock, the sample
is taken at this new position

 3. On CH1/CH2 trigger, low 2 bits of frequency timer are not modified

 Since the period for CH1/CH2 is a multiple of 4, the low 2 bits should always
 be 0.

 4. CH4 receives no clocks when SCF >= 14

 5. If the wave channel is enabled, accessing any byte from $FF30-$FF3F is
    equivalent to accessing the current byte selected by the waveform position.
    Further, on the DMG accesses will only work in this manner if made within a
    couple of clocks of the wave channel accessing wave RAM; if made at any
    other time, reads return $FF and writes have no effect.

Implemented, if the access was made within 4 clocks of the channel access then
the read/write will work.

### Zombie mode

Writing to the NRx2 register without retrigger immediately afterwards results in
strange behavior. This is known as zombie mode, where the channel volume can be
altered manually.

The exact behavior is not well documented and varies across models, but the most
consistent is the following:
 * Write V8 to the register where V is the initial volume
 * Retrigger the channel
 * Then the volume can be manually controlled by writing $08 to the register, which
   increments the current volume by 1.

## Not implemented

The following is a list of behavior that is not implemented by this emulator:
 * Vin terminal mixing