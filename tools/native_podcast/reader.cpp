#include "podcast_json_reader.h"
#include <ArduinoJson.h>
#include <cassert>
#include <cstdio>
#include <cstring>
#include <string>

uint32_t clockMs = 0;
uint32_t millis() { return clockMs; }
void delay(uint32_t milliseconds) { clockMs += milliseconds; }

// Match NetworkClientSecure: return -1 during a packet gap while connected.
struct SegmentedClient {
    std::string body;
    size_t split;
    uint32_t resumeAt;
    bool closeAtEnd = true;
    size_t position = 0;

    int read(uint8_t* buffer, size_t length) {
        assert(length == 1);
        if ((position >= split && clockMs < resumeAt) || position == body.size()) return -1;
        *buffer = static_cast<uint8_t>(body[position++]);
        return 1;
    }
    bool connected() const { return position < body.size() || !closeAtEnd; }
};

// Reproduce the old ArduinoJson -> NetworkClient::readBytes path: a negative
// secure-client read exits immediately, without waiting for the next packet.
struct PrematureEofReader {
    SegmentedClient& client;
    int read() { uint8_t value; return client.read(&value, 1) == 1 ? value : -1; }
    size_t readBytes(char* buffer, size_t length) {
        size_t count = 0;
        while (count < length) {
            const int value = read();
            if (value < 0) break;
            buffer[count++] = static_cast<char>(value);
        }
        return count;
    }
};

int main() {
    std::string body = "{\"Clips\":[";
    size_t afterThree = 0;
    for (int i = 0; i < 8; ++i) {
        if (i) body += ',';
        body += "{\"Id\":\"" + std::to_string(i) + "\",\"Title\":\"episode\"}";
        if (i == 2) afterThree = body.size();
    }
    body += "]}";
    JsonDocument document;
    SegmentedClient broken{body, afterThree, 69};
    PrematureEofReader oldReader{broken};
    assert(deserializeJson(document, oldReader) == DeserializationError::IncompleteInput);
    assert(document["Clips"].size() == 3 && clockMs == 0);

    // Every possible packet split, including inside a string or before any data.
    for (size_t split = 0; split < body.size(); ++split) {
        clockMs = 0;
        SegmentedClient client{body, split, 69};
        PodcastJsonReader<SegmentedClient> reader(client);
        assert(!deserializeJson(document, reader));
        assert(document["Clips"].size() == 8);
        assert(reader.bytesRead() == body.size() && clockMs == 69);
        assert(std::strcmp(reader.stopReason(), "none") == 0);
    }
    clockMs = 0;
    SegmentedClient truncated{body.substr(0, afterThree), afterThree, 0};
    PodcastJsonReader<SegmentedClient> closedReader(truncated);
    assert(deserializeJson(document, closedReader) == DeserializationError::IncompleteInput);
    assert(std::strcmp(closedReader.stopReason(), "closed") == 0 && clockMs == 0);

    clockMs = 0;
    SegmentedClient stalled{"", 0, 0, false};
    PodcastJsonReader<SegmentedClient> idleReader(stalled, 10, 50);
    assert(idleReader.read() == -1 && clockMs == 10);
    assert(std::strcmp(idleReader.stopReason(), "idle-timeout") == 0);

    clockMs = 0;
    PodcastJsonReader<SegmentedClient> deadlineReader(stalled, 100, 20);
    assert(deadlineReader.read() == -1 && clockMs == 20);
    assert(std::strcmp(deadlineReader.stopReason(), "deadline") == 0);

    clockMs = UINT32_MAX - 5;
    PodcastJsonReader<SegmentedClient> wrapReader(stalled, 10, 50);
    assert(wrapReader.read() == -1 && clockMs == 4);
    assert(std::strcmp(wrapReader.stopReason(), "idle-timeout") == 0);
    puts("PASS: reproduced early EOF; all packet splits parse; truncation, idle/total deadlines, clock wrap");
}
