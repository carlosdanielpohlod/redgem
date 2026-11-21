# Script mode, run in raylib's sources once they are fetched (everywhere): closing the audio device
# stops it before it destroys the lock its playing thread takes. raylib destroyed the lock first, so
# a sound buffer the device asked for in that moment locked a destroyed lock; on Android that ends
# the app (an Android game closed while it played sound aborted, "pthread_mutex_lock called on a
# destroyed mutex"). Stopped first, the device asks for nothing more. Running it twice changes nothing.
#   cmake -P PatchRaylibAudioClose.cmake      (in raylib's source folder)
set(audio_file "src/raudio.c")
set(lock_first
"        ma_mutex_uninit(&AUDIO.System.lock);
        ma_device_uninit(&AUDIO.System.device);
")
set(device_first
"        ma_device_uninit(&AUDIO.System.device);  // (RedGem) the device stopped before its lock goes
        ma_mutex_uninit(&AUDIO.System.lock);
")

# raylib, already stopping the device first by itself: nothing to do.
set(upstream_device_first
"        ma_device_uninit(&AUDIO.System.device);
        ma_mutex_uninit(&AUDIO.System.lock);
")

file(READ "${audio_file}" code)
# (raylib has no .gitattributes: a Windows checkout may end its lines with CR LF. Matched, and written,
# with LF alone, which every compiler reads the same.)
string(REPLACE "\r\n" "\n" code "${code}")
string(FIND "${code}" "${device_first}" patched)
string(FIND "${code}" "${upstream_device_first}" fixed_upstream)
if(patched EQUAL -1 AND fixed_upstream EQUAL -1)
  string(FIND "${code}" "${lock_first}" found)
  if(found EQUAL -1)
    message(FATAL_ERROR "${audio_file} is not the raylib this patch was written for: look at it again")
  endif()
  string(REPLACE "${lock_first}" "${device_first}" code "${code}")
  file(WRITE "${audio_file}" "${code}")
endif()
