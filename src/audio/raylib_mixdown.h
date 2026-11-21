#pragma once

// What the engine adds to raylib's audio (raylib_mixdown.c): the mix pulled block by block, with
// no sound device, as a recording needs it. Every other audio call of raylib works as it does on a
// device.

#ifdef __cplusplus
extern "C" {
#endif

// Opens raylib's audio on no device, mixing at `sampleRate` in interleaved float stereo. Nothing
// plays by itself: the mix moves on only as RenderAudioMix asks for it. IsAudioDeviceReady says
// whether it opened; CloseAudioDevice closes it.
void InitAudioMixdown(unsigned int sampleRate);

// The next `frameCount` frames of the mix, as the device would have played them: every stream
// that plays, through its processors, then the mixed processors, at the master volume.
void RenderAudioMix(float *frames, unsigned int frameCount);

#ifdef __cplusplus
}
#endif
