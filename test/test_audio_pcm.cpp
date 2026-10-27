#include "s31_audio_pcm.h"
#include <cassert>
#include <cstdio>

int main()
{
    int16_t out[8] = {};
    const uint8_t mono8[] = {0, 128, 255};
    assert(S31ConvertPCM(mono8, sizeof(mono8), 3, 1, 8, out, 8));
    assert(out[0] == -32768 && out[1] == -32768 && out[2] == 0 && out[3] == 0);
    assert(out[4] == 32512 && out[5] == 32512);
    const uint8_t stereo8[] = {0, 255, 128, 64};
    assert(S31ConvertPCM(stereo8, sizeof(stereo8), 2, 2, 8, out, 8));
    assert(out[0] == -32768 && out[1] == 32512 && out[2] == 0 && out[3] == -16384);
    const uint8_t mono16[] = {0x80, 0, 0x7f, 0xff};
    assert(S31ConvertPCM(mono16, sizeof(mono16), 2, 1, 16, out, 8));
    assert(out[0] == -32768 && out[1] == -32768 && out[2] == 32767 && out[3] == 32767);
    const uint8_t stereo16[] = {0x12, 0x34, 0xfe, 0xdc};
    assert(S31ConvertPCM(stereo16, sizeof(stereo16), 1, 2, 16, out, 8));
    assert(out[0] == 0x1234 && out[1] == -292);
    out[0] = 77;
    assert(!S31ConvertPCM(stereo16, 3, 1, 2, 16, out, 8));
    assert(!S31ConvertPCM(stereo16, 4, 1, 2, 16, out, 1));
    assert(!S31ConvertPCM(stereo16, 4, 1, 3, 8, out, 8));
    assert(!S31ConvertPCM(stereo16, 4, 1, 2, 24, out, 8));
    assert(!S31ConvertPCM(stereo16, 4, SIZE_MAX, 2, 16, out, 8));
    assert(out[0] == 77);
    puts("PASS: mono/stereo 8/16-bit PCM conversion, endian and buffer bounds");
}
