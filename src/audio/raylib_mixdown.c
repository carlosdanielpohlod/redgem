// Raylib's audio module, compiled here in whole (the build leaves raylib's own raudio.c out), with
// a way to pull its mix with no sound device: a recording mixes exactly the frames of each picture,
// in step with the game, however long a picture takes to draw. The mix is raylib's own callback, so
// what is recorded is what a player hears: every stream, its processors (the engine's effects,
// panning from mono, meters) and the mixed processors (the reverb buses).

#include "raudio.c"

#include "raylib_mixdown.h"

// The device a mixdown opens: miniaudio's null backend, which plays nothing. It is never started,
// so no thread of its own calls the mix.
static bool OpenMixdownDevice(unsigned int sampleRate)
{
    ma_backend backends[] = { ma_backend_null };
    ma_context_config contextConfig = ma_context_config_init();
    if (ma_context_init(backends, 1, &contextConfig, &AUDIO.System.context) != MA_SUCCESS) return false;

    ma_device_config config = ma_device_config_init(ma_device_type_playback);
    config.playback.format = AUDIO_DEVICE_FORMAT;
    config.playback.channels = AUDIO_DEVICE_CHANNELS;
    config.sampleRate = sampleRate;
    config.dataCallback = OnSendAudioDataToDevice;
    if (ma_device_init(&AUDIO.System.context, &config, &AUDIO.System.device) == MA_SUCCESS) return true;
    ma_context_uninit(&AUDIO.System.context);
    return false;
}

void InitAudioMixdown(unsigned int sampleRate)
{
    if (!OpenMixdownDevice(sampleRate))
    {
        TRACELOG(LOG_WARNING, "AUDIO: Failed to open the mixdown");
        return;
    }
    if (ma_mutex_init(&AUDIO.System.lock) != MA_SUCCESS)
    {
        TRACELOG(LOG_WARNING, "AUDIO: Failed to create mutex for mixing");
        ma_device_uninit(&AUDIO.System.device);
        ma_context_uninit(&AUDIO.System.context);
        return;
    }
    AUDIO.System.isReady = true;
}

// As miniaudio finishes a device's frames: the master volume, then each sample held to [-1, 1].
void RenderAudioMix(float *frames, unsigned int frameCount)
{
    OnSendAudioDataToDevice(&AUDIO.System.device, frames, NULL, frameCount);
    float volume = 1.0f;
    ma_device_get_master_volume(&AUDIO.System.device, &volume);
    const ma_uint64 samples = (ma_uint64)frameCount*AUDIO_DEVICE_CHANNELS;
    ma_apply_volume_factor_f32(frames, samples, volume);
    ma_clip_samples_f32(frames, frames, samples);
}
