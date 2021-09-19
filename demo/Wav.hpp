/*
** Wav.hpp
** 
** A header-only library for writing wav files. To use, define
** WAV_IMPLEMENTATION in one of your source files that include this header.
**
** To create a file, construct a Wav object with a filepath, number of channels
** and samplerate. Then write as many samples you want to it via the write
** method. Note that for multichannel data, the samples are interleaved.
**
** This library only writes wav files using 32-bit float samples.
**
** ----------------------------------------------------------------------------
**
** This is free and unencumbered software released into the public domain.
**
** Anyone is free to copy, modify, publish, use, compile, sell, or
** distribute this software, either in source code form or as a compiled
** binary, for any purpose, commercial or non-commercial, and by any
** means.
**
** In jurisdictions that recognize copyright laws, the author or authors
** of this software dedicate any and all copyright interest in the
** software to the public domain. We make this dedication for the benefit
** of the public at large and to the detriment of our heirs and
** successors. We intend this dedication to be an overt act of
** relinquishment in perpetuity of all present and future rights to this
** software under copyright law.
** 
** THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
** EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
** MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
** IN NO EVENT SHALL THE AUTHORS BE LIABLE FOR ANY CLAIM, DAMAGES OR
** OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
** ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
** OTHER DEALINGS IN THE SOFTWARE.
**
** For more information, please refer to <https://unlicense.org>
*/


#pragma once

#include <cstddef>
#include <fstream>
#include <string>


class Wav {

public:


    //
    // Opens a wav file for writing sample data with the given channel count
    // and samplerate. Existing files will be overwritten.
    //
    explicit Wav(std::string const& filename, int channels, int samplerate);

    //
    // Adjusts the wav header with the final number of samples written and
    // closes the file.
    //
    ~Wav();

    //
    // Writes the given number of samples from the given buffer to the wav
    // file. The buffer should be at least the size of nsamples * channels.
    //
    void write(float buf[], std::size_t nsamples);

private:

    // non-copyable
    Wav(Wav const& wav) = delete;
    Wav& operator=(Wav const& wav) = delete;

    // non-movable
    Wav(Wav &&wav) = delete;
    Wav& operator=(Wav &&wav) = delete;

    std::ofstream mStream;
    std::size_t mSampleCount;

    int mChannels;
    int mSamplingRate;

};


#ifdef WAV_IMPLEMENTATION

#include <cassert>
#include <cstdint>


namespace WavPrivate {

#pragma pack(push, 1)

//
// Header for wav files with 32-bit floating point samples
//
struct WavHeader {

    // [B] indicates that this field is set in the Wav constructor
    // [F] indicates that this field is set in the Wav destructor
    // = indicates the field's initial value

    // RIFF chunk
    char riffId[4];             // = "RIFF"
    uint32_t chunkSize;         // [F]
    char waveId[4];             // = "WAVE"
    // fmt subchunk
    char fmtId[4];              // = "fmt "
    uint32_t fmtChunkSize;      // = 18
    uint16_t fmtTag;            // = 0x3 for IEEE_FLOAT
    uint16_t fmtChannels;       // [B]
    uint32_t fmtSampleRate;     // [B]
    uint32_t fmtAvgBytesPerSec; // [B] = 4 * fmtSampleRate * fmtChannels
    uint16_t fmtBlockAlign;     // [B] = 4 * fmtChannels
    uint16_t fmtBitsPerSample;  // = 32
    uint16_t fmtCbSize;         // = 0
    // fact subchunk
    char factId[4];             // = "fact"
    uint32_t factChunkSize;     // = 4
    uint32_t factSampleCount;   // [F]
    // data subchunk
    char dataId[4];             // = "data"
    uint32_t dataChunkSize;     // [F]
    // sampled data follows...
    // extra padding byte if dataChunkSize is odd [F]

    WavHeader() :
        riffId{'R', 'I', 'F', 'F'},
        chunkSize(0),
        waveId{'W', 'A', 'V', 'E'},
        fmtId{'f', 'm', 't', ' '},
        fmtChunkSize(18),
        fmtTag(0x3),
        fmtChannels(0),
        fmtSampleRate(0),
        fmtAvgBytesPerSec(0),
        fmtBlockAlign(0),
        fmtBitsPerSample(32),
        fmtCbSize(0),
        factId{'f', 'a', 'c', 't'},
        factChunkSize(4),
        factSampleCount(0),
        dataId{'d', 'a', 't', 'a'},
        dataChunkSize(0)
    {
    }

};

#pragma pack(pop)

}



Wav::Wav(std::string const& filename, int channels, int samplerate) :
    mStream(filename, std::ios::out | std::ios::binary),
    mSampleCount(0),
    mChannels(channels),
    mSamplingRate(samplerate)
{
    assert(channels > 0);
    assert(samplerate > 0);

    WavPrivate::WavHeader header;
    header.fmtChannels = mChannels;
    header.fmtSampleRate = mSamplingRate;
    uint16_t bytesPerChannel = mChannels * 4;
    header.fmtAvgBytesPerSec = bytesPerChannel * mSamplingRate;
    header.fmtBlockAlign = bytesPerChannel;


    mStream.write(reinterpret_cast<const char *>(&header), sizeof(header));

}

Wav::~Wav() {
    uint32_t totalSamples = static_cast<uint32_t>(mSampleCount);
    uint32_t dataChunkSize = totalSamples * mChannels * 4;

    // chunk size totals
    // 4: riff chunk
    // 18 + 8: fmt chunk
    // 4 + 8: fact chunk
    // = 42
    // 8 + dataChunkSize + (0 or 1)
    uint32_t chunkSize = 50 + dataChunkSize;

    // do we need a pad byte?
    if (dataChunkSize & 1) {
        // yes, write a single 0 to the end of the data chunk
        uint8_t zero = 0;
        mStream.write(reinterpret_cast<const char *>(&zero), sizeof(zero));
        ++chunkSize;
    }

    // overwrite the chunk size for the entire file (also equal to the filesize - 8)
    mStream.seekp(offsetof(WavPrivate::WavHeader, chunkSize));
    mStream.write(reinterpret_cast<const char *>(&chunkSize), sizeof(chunkSize));

    // overwrite the sample count in the fact subchunk
    mStream.seekp(offsetof(WavPrivate::WavHeader, factSampleCount));
    mStream.write(reinterpret_cast<const char *>(&totalSamples), sizeof(totalSamples));

    // overwrite the chunk size of the data subchunk
    mStream.seekp(offsetof(WavPrivate::WavHeader, dataChunkSize));
    mStream.write(reinterpret_cast<const char *>(&dataChunkSize), sizeof(dataChunkSize));
}

void Wav::write(float buf[], std::size_t nsamples) {

    std::size_t totalSamples = mChannels * nsamples;
    mStream.write(reinterpret_cast<const char*>(buf), totalSamples * sizeof(float));
    if (!mStream.good()) {
        return;
    }

    mSampleCount += nsamples;

}



#endif // WAV_IMPLEMENTATION
