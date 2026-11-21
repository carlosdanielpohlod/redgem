// What raylib's raudio.c did before cmake/PatchRaylibAudioClose.cmake: the lock destroyed first.
// (Its InitAudioDevice stops the device too, when it fails, before CloseAudioDevice is reached.)
void InitAudioDevice(void)
{
        ma_device_uninit(&AUDIO.System.device);
}

void CloseAudioDevice(void)
{
    if (AUDIO.System.isReady)
    {
        ma_mutex_uninit(&AUDIO.System.lock);
        ma_device_uninit(&AUDIO.System.device);
    }
}
