// What raylib's raudio.c did before cmake/PatchRaylibAudioClose.cmake: the lock destroyed first.
void InitAudioDevice(void)
{
        ma_device_uninit(&AUDIO.System.device);
        ma_mutex_uninit(&AUDIO.System.lock);
}

void CloseAudioDevice(void)
{
    if (AUDIO.System.isReady)
    {
        ma_mutex_uninit(&AUDIO.System.lock);
        ma_device_uninit(&AUDIO.System.device);
    }
}
