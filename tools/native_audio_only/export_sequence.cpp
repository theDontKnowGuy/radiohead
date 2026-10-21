#include "audio_sequence.h"
#include <cassert>
#include <fstream>
#include <vector>
using namespace audio_test;

int main(int argc, char** argv) {
    assert(argc == 3);
    std::ifstream input(argv[1], std::ios::binary | std::ios::ate);
    assert(input.good());
    const size_t bytes = input.tellg();
    assert(bytes && bytes % 4 == 0);
    input.seekg(0);
    std::vector<int16_t> music(bytes / 2);
    input.read(reinterpret_cast<char*>(music.data()), bytes);
    assert(input.good());
    RecordingSequence sequence{music.data(), music.size() / 2};
    std::ofstream output(argv[2], std::ios::binary);
    // Exercise the exact DMA preload/128-frame continuation against the reference.
    RecordingSequence firmware{music.data(), music.size() / 2, 1024};
    for (uint32_t i=0; i<kSequenceReferenceFrames; ++i) {
        int16_t frame[2];
        sequence.next(frame[0], frame[1]);
        assert(frame[1-kSequenceChannel] == 0);
        if (i < 1024) assert(frame[0] == 0);
        else {
            int16_t left,right;
            firmware.next(left,right);
            assert(left == frame[0] && right == frame[1]);
        }
        assert(std::abs(static_cast<int>(frame[kSequenceChannel])) < 32767);
        // Explicit little-endian PCM, independent of the host byte order.
        for (int16_t sample : frame) {
            const uint16_t word=static_cast<uint16_t>(sample);
            output.put(static_cast<char>(word & 255));
            output.put(static_cast<char>(word >> 8));
        }
    }
    assert(output.good());
    assert(sequence.frame == kSequenceEnd);
    for (int i=0; i<44100; ++i) {
        int16_t left,right;
        sequence.next(left,right);
        assert(!left && !right && sequence.frame == kSequenceEnd);
    }
}
