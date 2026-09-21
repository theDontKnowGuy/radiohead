# Optional live-stream prebuffer for ESP32-audioI2S v4

Status: **disabled after the A/B test**.

The radio is currently running ESP32-audioI2S v4.0.0 with its unmodified
web-stream startup rule: playback begins after the decoder has one complete
audio frame. NPR was smooth during the comparison test, so there is no current
evidence that a larger startup reserve is required.

## Why this patch exists

Before the v4 upgrade, live streams reported `slow stream` with only about
18 KB of input buffered. The optional patch makes startup wait for 64 KB of
input, which is several seconds for NPR's 64 kb/s AAC stream. It only affects
continuous HTTP web streams; it does not change HLS startup thresholds or
finite-file playback.

## Restore procedure

Apply all of the following changes together to the v4 vendored library and
`src/main.cpp`. The same changes are available as an apply-ready patch:

```sh
git apply docs/patches/esp32-audioi2s-v4-stream-prebuffer.patch
```

1. In `lib/ESP32-audioI2S-master/src/Audio.h`, immediately after
   `setConnectionTimeout`, add:

   ```cpp
   // For continuous web streams, wait for this many input bytes before
   // starting decode. Zero preserves the library's one-frame startup behavior.
   void setStreamPrebuffer(size_t bytes);
   ```

2. In the same header, after `m_audioDataSize`, add:

   ```cpp
   size_t m_streamPrebufferSize = 0;  // Web-stream startup reserve.
   ```

3. In `Audio.cpp`, immediately after `Audio::setConnectionTimeout`, add:

   ```cpp
   void Audio::setStreamPrebuffer(size_t bytes) {
       m_streamPrebufferSize = bytes;
   }
   ```

4. In `Audio::processWebStream`, replace the normal `m_pwst.maxFrameSize`
   start condition with:

   ```cpp
   size_t playbackThreshold = m_pwst.maxFrameSize;
   if (m_streamPrebufferSize > playbackThreshold) {
       playbackThreshold = m_streamPrebufferSize;
   }
   if (playbackThreshold >= InBuff.getBufsize()) {
       playbackThreshold = InBuff.getBufsize() - 1;
   }
   if (((InBuff.bufferFilled() > playbackThreshold) || m_f_allDataReceived) &&
       !m_f_stream) {
       info(*this, evt_info, "stream ready");
       m_f_stream = true;
   }
   ```

5. In `src/main.cpp`, define and configure the reserve before station playback:

   ```cpp
   constexpr size_t kStreamPrebufferBytes = 64 * 1024;
   // ...
   audio.setPinout(I2S_BCK, I2S_LRC, I2S_DIN);
   audio.setStreamPrebuffer(kStreamPrebufferBytes);
   ```

## When to restore it

Restore the patch only if a repeatable listening test shows live-stream
underruns and serial diagnostics show the input buffer repeatedly approaching
zero or emit `slow stream` / `Stream lost`. Build with `pio run -e esp32s3`,
then test NPR and another direct live stream for several minutes. Do not use
this patch as a workaround for a Wi-Fi disconnect, HLS behavior, or a blocking
operation in the Arduino loop.
