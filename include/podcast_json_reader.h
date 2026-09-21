#pragma once

#include <Arduino.h>

// Arduino-ESP32 3.3.11's inherited NetworkClient::readBytes() treats a
// negative read as EOF, but NetworkClientSecure::read() also returns -1 when
// waiting for the next TLS record. Give ArduinoJson an explicit timed reader
// instead. Used only by the podcast worker; never in the audio servicing loop.
template <typename Client>
class PodcastJsonReader {
public:
    explicit PodcastJsonReader(Client& client, uint32_t idleTimeoutMs = 10000,
                               uint32_t totalTimeoutMs = 20000)
        : client_(client), startedAt_(millis()), idleTimeoutMs_(idleTimeoutMs),
          totalTimeoutMs_(totalTimeoutMs) {}

    int read() {
        const uint32_t waitingSince = millis();
        while (true) {
            if (static_cast<uint32_t>(millis() - startedAt_) >= totalTimeoutMs_) {
                stopReason_ = "deadline";
                return -1;
            }
            uint8_t value;
            if (client_.read(&value, 1) == 1) {
                ++bytesRead_;
                return value;
            }
            if (!client_.connected()) {
                stopReason_ = "closed";
                return -1;
            }
            if (static_cast<uint32_t>(millis() - waitingSince) >= idleTimeoutMs_) {
                stopReason_ = "idle-timeout";
                return -1;
            }
            delay(1);
        }
    }

    size_t readBytes(char* buffer, size_t length) {
        size_t count = 0;
        while (count < length) {
            const int value = read();
            if (value < 0) break;
            buffer[count++] = static_cast<char>(value);
        }
        return count;
    }

    size_t bytesRead() const { return bytesRead_; }
    const char* stopReason() const { return stopReason_; }

private:
    Client& client_;
    const uint32_t startedAt_;
    const uint32_t idleTimeoutMs_;
    const uint32_t totalTimeoutMs_;
    size_t bytesRead_ = 0;
    const char* stopReason_ = "none";
};
