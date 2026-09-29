#include "spotify_adapter.h"

#include <algorithm>
#include <atomic>
#include <cstring>
#include <deque>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <string_view>
#include <vector>
#include <Arduino.h>
#include <WiFi.h>
// Arduino's F() macro collides with a template parameter in Bell's fmt.
#undef F
#include "CircularBuffer.h"
#include "CSpotContext.h"
#include "LoginBlob.h"
#include "Logger.h"
#include "MAX98357AAudioSink.h"
#include "SpircHandler.h"
#include "TrackPlayer.h"
#include "esp_heap_caps.h"
#include "esp_pthread.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "nvs.h"

namespace {
// Keep the S1 pairing identity while reusing its saved login blob.
constexpr char kDeviceName[] = "Radiohead Native Test";
// A captured CDN/decode gap consumed about 270 KiB more PCM than it supplied.
// Keep a larger PSRAM-backed reserve before starting output.
constexpr size_t kPcmCapacity = 384 * 1024;
constexpr size_t kPcmPrebuffer = 320 * 1024;
constexpr uint8_t kSignalCapacity = 16;
portMUX_TYPE signalMux = portMUX_INITIALIZER_UNLOCKED;
SpotifySignal signals[kSignalCapacity];
uint8_t signalHead = 0;
uint8_t signalCount = 0;
uint32_t nextSignalSequence = 0;
std::atomic<bool> ready{false}, sessionReady{false};
std::atomic<bool> paired{false};
std::shared_ptr<cspot::LoginBlob> pairingBlob;
std::atomic<bool> outputRequested{false}, outputOwned{false};
std::atomic<bool> acceptPcm{false}, paused{true}, depleted{false};
std::atomic<bool> pendingActivation{false};
std::atomic<uint8_t> requestedVolume{0};
std::atomic<int8_t> bassTone{0}, middleTone{0}, trebleTone{0};
std::atomic<uint32_t> toneRevision{0};
std::atomic<uint32_t> pcmGeneration{0};
std::atomic<bool> outputBuffering{false};
std::atomic<uint32_t> lastPcmReportAt{0}, lastPcmBytes{0}, lastProducedBytes{0};
std::atomic<uint32_t> lastEmptyPolls{0}, lastMinimumFill{0}, lastPartialWrites{0}, lastWriteFailures{0};
std::mutex pcmMutex;
std::unique_ptr<bell::CircularBuffer> pcm;
std::deque<uint64_t> boundaries;
std::string producerTrack;
uint64_t producedBytes = 0, consumedBytes = 0;
std::shared_ptr<cspot::SpircHandler> handler;
StaticTask_t outputTcb, connectTcb;
StackType_t outputStack[8 * 1024 / sizeof(StackType_t)];
StackType_t connectStack[16 * 1024 / sizeof(StackType_t)];

class RedactedLogger final : public bell::AbstractLogger {
public:
    void debug(std::string, int, std::string, const char*, ...) override {}
    void info(std::string, int, std::string, const char*, ...) override {}
    void error(std::string filename, int line, std::string, const char*, ...) override {
        const size_t slash = filename.find_last_of("/\\");
        const char* base = slash == std::string::npos ? filename.c_str() : filename.c_str() + slash + 1;
        Serial.printf("[spotify] upstream error %s:%d (details redacted)\n", base, line);
    }
};
RedactedLogger redactedLogger;

template<size_t N>
void copyMetadata(char (&target)[N], const std::string& source) {
    size_t written = 0;
    for (size_t i = 0; i < source.size() && written + 1 < N;) {
        const uint8_t first = static_cast<uint8_t>(source[i]);
        const size_t bytes = first < 0x80 ? 1 : (first & 0xe0) == 0xc0 ? 2 :
            (first & 0xf0) == 0xe0 ? 3 : (first & 0xf8) == 0xf0 ? 4 : 0;
        if (!bytes || i + bytes > source.size() || written + bytes >= N ||
            first == 0xc0 || first == 0xc1 || first > 0xf4) break;
        if (first >= 0x20 && first != 0x7f) {
            bool valid = true;
            for (size_t j = 1; j < bytes; ++j)
                valid &= (static_cast<uint8_t>(source[i + j]) & 0xc0) == 0x80;
            if (bytes == 3) {
                const uint8_t second = static_cast<uint8_t>(source[i + 1]);
                valid &= !(first == 0xe0 && second < 0xa0) &&
                    !(first == 0xed && second >= 0xa0);
            } else if (bytes == 4) {
                const uint8_t second = static_cast<uint8_t>(source[i + 1]);
                valid &= !(first == 0xf0 && second < 0x90) &&
                    !(first == 0xf4 && second >= 0x90);
            }
            if (!valid) break;
            memcpy(target + written, source.data() + i, bytes);
            written += bytes;
        }
        i += bytes;
    }
    target[written] = '\0';
}

void post(SpotifySignalType type, uint16_t volume = 0, bool isPaused = false,
          const cspot::TrackInfo* track = nullptr) {
    SpotifySignal signal;
    signal.type = type;
    signal.volume = volume;
    signal.paused = isPaused;
    if (track) {
        copyMetadata(signal.title, track->name);
        copyMetadata(signal.artist, track->artist);
        copyMetadata(signal.album, track->album);
    }
    portENTER_CRITICAL(&signalMux);
    if (signalCount == kSignalCapacity) {
        // Stop always gets a slot and invalidates queued work.
        if (type == SpotifySignalType::Stop) {
            signalHead = 0;
            signalCount = 0;
        } else {
            signalHead = (signalHead + 1) % kSignalCapacity;
            --signalCount;
        }
    }
    signal.sequence = ++nextSignalSequence;
    signals[(signalHead + signalCount) % kSignalCapacity] = signal;
    ++signalCount;
    portEXIT_CRITICAL(&signalMux);
}

bool readNvs(const char* key, std::string& value, size_t limit) {
    nvs_handle_t h;
    if (nvs_open("spotify", NVS_READONLY, &h) != ESP_OK) return false;
    size_t bytes = 0;
    esp_err_t err = nvs_get_str(h, key, nullptr, &bytes);
    if (err != ESP_OK || bytes < 2 || bytes > limit + 1) {
        nvs_close(h);
        return false;
    }
    std::string candidate(bytes, '\0');
    err = nvs_get_str(h, key, candidate.data(), &bytes);
    nvs_close(h);
    if (err != ESP_OK || bytes < 2) return false;
    candidate.resize(bytes - 1);
    value.swap(candidate);
    return true;
}

bool writeNvs(const char* key, const std::string& value) {
    nvs_handle_t h;
    if (nvs_open("spotify", NVS_READWRITE, &h) != ESP_OK) return false;
    esp_err_t err = nvs_set_str(h, key, value.c_str());
    if (err == ESP_OK) err = nvs_commit(h);
    nvs_close(h);
    return err == ESP_OK;
}

void flush() {
    pcmGeneration.fetch_add(1, std::memory_order_relaxed);
    std::lock_guard<std::mutex> lock(pcmMutex);
    if (pcm) pcm->emptyBuffer();
    boundaries.clear();
    producerTrack.clear();
    consumedBytes = producedBytes;
}

size_t feed(uint8_t* data, size_t length, std::string_view trackId) {
    // Backpressure the decoder while a local source owns I2S. Discarding PCM
    // lets it decrypt and decode at network speed with no output pacing.
    if (!acceptPcm.load(std::memory_order_acquire)) return 0;
    const uint32_t generation = pcmGeneration.load(std::memory_order_relaxed);
    std::lock_guard<std::mutex> lock(pcmMutex);
    if (!acceptPcm.load(std::memory_order_relaxed) ||
        generation != pcmGeneration.load(std::memory_order_relaxed) || !pcm) return 0;
    if (trackId != producerTrack) {
        if (boundaries.size() >= 32) return 0;
        producerTrack.assign(trackId.data(), trackId.size());
        boundaries.push_back(producedBytes);
    }
    const size_t written = pcm->write(data, length);
    producedBytes += written;
    return written;
}

void outputTask(void*) {
    std::unique_ptr<MAX98357AAudioSink> sink;
    uint32_t appliedToneRevision = UINT32_MAX;
    uint32_t lastReportAt = millis();
    uint64_t lastReportedBytes = 0;
    uint64_t lastReportedProduced = 0;
    uint32_t emptyPolls = 0;
    size_t minimumFill = kPcmCapacity;
    uint32_t outputGeneration = UINT32_MAX;
    bool buffering = true;
    uint8_t buffer[1024];
    for (;;) {
        if (!outputRequested.load(std::memory_order_acquire)) {
            if (sink) {
                sink.reset();
                appliedToneRevision = UINT32_MAX;
                outputOwned.store(false, std::memory_order_release);
                outputBuffering.store(false, std::memory_order_release);
            }
            vTaskDelay(pdMS_TO_TICKS(5));
            continue;
        }
        if (!sink) {
            sink = std::make_unique<MAX98357AAudioSink>();
            if (!sink->setParams(44100, 2, 16)) {
                sink.reset();
                outputRequested.store(false, std::memory_order_release);
                vTaskDelay(pdMS_TO_TICKS(10));
                continue;
            }
            outputOwned.store(true, std::memory_order_release);
            outputGeneration = UINT32_MAX;
            buffering = true;
            outputBuffering.store(true, std::memory_order_release);
            lastReportedBytes = 0;
            {
                std::lock_guard<std::mutex> lock(pcmMutex);
                lastReportedProduced = producedBytes;
            }
            lastReportAt = millis();
            emptyPolls = 0;
            minimumFill = kPcmCapacity;
        }
        sink->volumeChanged(requestedVolume.load(std::memory_order_relaxed));
        const uint32_t revision = toneRevision.load(std::memory_order_acquire);
        if (revision != appliedToneRevision) {
            sink->setTone(bassTone.load(std::memory_order_relaxed),
                middleTone.load(std::memory_order_relaxed),
                trebleTone.load(std::memory_order_relaxed));
            appliedToneRevision = revision;
        }
        if (paused.load(std::memory_order_relaxed)) {
            vTaskDelay(pdMS_TO_TICKS(10));
            continue;
        }
        size_t bytes = 0;
        uint32_t reached = 0;
        const uint32_t generation = pcmGeneration.load(std::memory_order_relaxed);
        if (generation != outputGeneration) {
            outputGeneration = generation;
            buffering = true;
        }
        bool waitingForBuffer = false;
        {
            std::lock_guard<std::mutex> lock(pcmMutex);
            waitingForBuffer = buffering && pcm && pcm->size() < kPcmPrebuffer &&
                !depleted.load(std::memory_order_relaxed);
            if (pcm && !waitingForBuffer) {
                buffering = false;
                bytes = pcm->read(buffer, sizeof(buffer));
                minimumFill = std::min(minimumFill, pcm->size());
            }
            consumedBytes += bytes;
            while (!boundaries.empty() && boundaries.front() < consumedBytes) {
                boundaries.pop_front();
                ++reached;
            }
        }
        if (bytes == 0 && !waitingForBuffer) {
            ++emptyPolls;
            buffering = true;
        }
        outputBuffering.store(buffering, std::memory_order_release);
        if (bytes && generation == pcmGeneration.load(std::memory_order_relaxed) &&
            outputRequested.load(std::memory_order_relaxed)) {
            sink->feedPCMFrames(buffer, bytes); // Bounded 200 ms I2S write.
            for (uint32_t n = 0; n < reached &&
                generation == pcmGeneration.load(std::memory_order_relaxed); ++n)
                if (handler) handler->notifyAudioReachedPlayback();
        } else if (depleted.exchange(false, std::memory_order_relaxed) && handler) {
            handler->notifyAudioEnded();
        } else {
            vTaskDelay(pdMS_TO_TICKS(5));
        }
        const uint32_t now = millis();
        if (now - lastReportAt >= 5000) {
            // Never print here: USB CDC can block for ~2 s without a reader,
            // starving I2S. The web diagnostics endpoint reads these counters.
            uint64_t produced = 0;
            {
                std::lock_guard<std::mutex> lock(pcmMutex);
                produced = producedBytes;
            }
            const uint64_t total = sink->pcmBytes();
            const uint32_t intervalBytes = static_cast<uint32_t>(total - lastReportedBytes);
            const uint32_t intervalProduced = static_cast<uint32_t>(produced - lastReportedProduced);
            lastPcmBytes.store(intervalBytes, std::memory_order_relaxed);
            lastProducedBytes.store(intervalProduced, std::memory_order_relaxed);
            lastEmptyPolls.store(emptyPolls, std::memory_order_relaxed);
            lastMinimumFill.store(static_cast<uint32_t>(minimumFill), std::memory_order_relaxed);
            lastPartialWrites.store(sink->partialWrites(), std::memory_order_relaxed);
            lastWriteFailures.store(sink->writeFailures(), std::memory_order_relaxed);
            lastPcmReportAt.store(now, std::memory_order_release);
            lastReportedBytes = total;
            lastReportedProduced = produced;
            emptyPolls = 0;
            minimumFill = kPcmCapacity;
            lastReportAt = now;
        }
    }
}

void onEvent(std::unique_ptr<cspot::SpircHandler::Event> event) {
    using Type = cspot::SpircHandler::EventType;
    switch (event->eventType) {
        case Type::ACTIVATE:
            pendingActivation.store(true, std::memory_order_release);
            post(SpotifySignalType::Activate);
            break;
        case Type::PLAY_PAUSE:
            paused.store(std::get<bool>(event->data), std::memory_order_relaxed);
            post(SpotifySignalType::Playback, 0,
                 paused.load(std::memory_order_relaxed));
            break;
        case Type::TRACK_INFO:
            post(SpotifySignalType::Metadata, 0, false,
                 &std::get<cspot::TrackInfo>(event->data));
            break;
        case Type::VOLUME:
            post(SpotifySignalType::Volume,
                static_cast<uint16_t>(std::clamp(std::get<int>(event->data), 0, 65535)));
            break;
        case Type::DISC:
            pendingActivation.store(false, std::memory_order_release);
            paused.store(true, std::memory_order_relaxed);
            depleted.store(false, std::memory_order_relaxed);
            flush();
            post(SpotifySignalType::Stop);
            break;
        case Type::FLUSH:
        case Type::SEEK:
        case Type::PLAYBACK_START:
            depleted.store(false, std::memory_order_relaxed);
            flush();
            break;
        case Type::DEPLETED:
            depleted.store(true, std::memory_order_relaxed);
            break;
        default:
            break;
    }
}

void connectTask(void*) {
    bell::bellGlobalLogger = &redactedLogger;
    esp_pthread_cfg_t cfg = esp_pthread_get_default_config();
    cfg.stack_alloc_caps = MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT;
    cfg.inherit_cfg = true;
    esp_pthread_set_cfg(&cfg);
    std::string clientId, clientSecret;
    if (!readNvs("client_id", clientId, 128) ||
        !readNvs("client_secret", clientSecret, 128)) {
        Serial.println("[s2] Spotify app credentials absent");
        vTaskDelete(nullptr);
        return;
    }
    auto blob = std::make_shared<cspot::LoginBlob>(kDeviceName);
    std::string savedBlob;
    std::atomic<bool> gotBlob{false};
    if (readNvs("login_blob", savedBlob, 1024)) {
        try {
            blob->loadJson(savedBlob);
            gotBlob.store(!blob->authData.empty() && !blob->getUserName().empty());
        } catch (...) {
            Serial.println("[s2] Stored Spotify pairing invalid");
        }
    }
    pairingBlob = blob;
    paired.store(gotBlob.load(), std::memory_order_release);
    ready.store(true, std::memory_order_release);
    Serial.printf("[s2] discovery ready paired=%d internal=%u largest=%u\n",
        paired.load(),
        static_cast<unsigned>(heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT)),
        static_cast<unsigned>(heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT)));
    while (!paired.load(std::memory_order_acquire)) vTaskDelay(pdMS_TO_TICKS(100));
    try {
        auto ctx = cspot::Context::createFromBlob(blob);
        ctx->config.clientId = clientId;
        ctx->config.clientSecret = clientSecret;
        bool authenticated = false;
        for (int attempt = 0; attempt < 10 && !authenticated; ++attempt) {
            Serial.printf("[s2] AP attempt=%d internal=%u largest=%u\n", attempt + 1,
                static_cast<unsigned>(heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT)),
                static_cast<unsigned>(heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT)));
            try {
                ctx->session->connectWithRandomAp();
                authenticated = !ctx->session->authenticate(blob).empty();
            } catch (const std::exception&) {
                // A short AP or auth failure is common on weak Wi-Fi.
            }
            if (!authenticated) vTaskDelay(pdMS_TO_TICKS(3000));
        }
        if (!authenticated) throw std::runtime_error("AP authentication failed");
        ctx->session->startTask();
        handler = std::make_shared<cspot::SpircHandler>(ctx);
        handler->getTrackPlayer()->setDataCallback(feed);
        handler->setEventHandler(onEvent);
        pcm = std::make_unique<bell::CircularBuffer>(kPcmCapacity);
        if (!xTaskCreateStaticPinnedToCore(outputTask, "spotify_output", sizeof(outputStack),
            nullptr, 4, outputStack, &outputTcb, 0))
            throw std::runtime_error("output task creation failed");
        sessionReady.store(true, std::memory_order_release);
        handler->subscribeToMercury();
        Serial.printf("[s2] authenticated internal=%u largest=%u\n",
            static_cast<unsigned>(heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT)),
            static_cast<unsigned>(heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT)));
        for (;;) ctx->session->handlePacket();
    } catch (const std::exception&) {
        Serial.println("[s2] Spotify session stopped");
        acceptPcm.store(false, std::memory_order_release);
        pendingActivation.store(false, std::memory_order_release);
        outputRequested.store(false, std::memory_order_release);
        post(SpotifySignalType::Stop);
    }
    // Detached Bell tasks have no general safe join; their callback targets
    // retain process lifetime until a controlled reboot.
    vTaskDelete(nullptr);
}
} // namespace

void spotifyAdapterBegin() {
    if (WiFi.status() != WL_CONNECTED) return;
    // The standalone receiver used WIFI_PS_NONE. Modem sleep's long listen
    // interval starves the integrated decoder's real-time PCM supply.
    if (!WiFi.setSleep(false))
        Serial.println("[s2] Wi-Fi modem sleep disable failed");
    xTaskCreateStaticPinnedToCore(connectTask, "spotify_connect", sizeof(connectStack),
        nullptr, 5, connectStack, &connectTcb, 1);
}

String spotifyAdapterInfoJson() {
    if (!ready.load(std::memory_order_acquire) || !pairingBlob) return String();
    return String(pairingBlob->buildZeroconfInfo().c_str());
}

String spotifyAdapterDiagnosticsJson() {
    size_t fill = 0;
    {
        std::lock_guard<std::mutex> lock(pcmMutex);
        if (pcm) fill = pcm->size();
    }
    String json;
    json.reserve(340);
    json = "{\"ready\":" + String(ready.load() ? "true" : "false") +
        ",\"sessionReady\":" + String(sessionReady.load() ? "true" : "false") +
        ",\"outputOwned\":" + String(outputOwned.load() ? "true" : "false") +
        ",\"buffering\":" + String(outputBuffering.load() ? "true" : "false") +
        ",\"paused\":" + String(paused.load() ? "true" : "false") +
        ",\"capacity\":" + String(kPcmCapacity) +
        ",\"fill\":" + String(fill) +
        ",\"reportAtMs\":" + String(lastPcmReportAt.load()) +
        ",\"pcmBytes5s\":" + String(lastPcmBytes.load()) +
        ",\"producedBytes5s\":" + String(lastProducedBytes.load()) +
        ",\"emptyPolls5s\":" + String(lastEmptyPolls.load()) +
        ",\"minimumFill5s\":" + String(lastMinimumFill.load()) +
        ",\"partialWrites\":" + String(lastPartialWrites.load()) +
        ",\"writeFailures\":" + String(lastWriteFailures.load()) + "}";
    return json;
}

bool spotifyAdapterPairingSubmit(const std::map<std::string, std::string>& fields) {
    if (!ready.load(std::memory_order_acquire) || !pairingBlob) return false;
    if (paired.load(std::memory_order_acquire)) return true;
    try {
        auto mutableFields = fields;
        pairingBlob->loadZeroconfQuery(mutableFields);
        if (pairingBlob->getUserName().empty() || pairingBlob->authData.empty()) return false;
        if (!writeNvs("login_blob", pairingBlob->toJson())) return false;
        paired.store(true, std::memory_order_release);
        return true;
    } catch (...) {
        return false;
    }
}

SpotifySignal spotifyAdapterTakeSignal() {
    SpotifySignal result;
    portENTER_CRITICAL(&signalMux);
    if (signalCount) {
        result = signals[signalHead];
        signalHead = (signalHead + 1) % kSignalCapacity;
        --signalCount;
    }
    portEXIT_CRITICAL(&signalMux);
    return result;
}

void spotifyAdapterDiscardPending() {
    portENTER_CRITICAL(&signalMux);
    signalHead = signalCount = 0;
    portEXIT_CRITICAL(&signalMux);
    pendingActivation.store(false, std::memory_order_release);
}

bool spotifyAdapterAcquireOutput(uint8_t volume, uint32_t timeoutMs) {
    if (!sessionReady.load(std::memory_order_acquire)) return false;
    requestedVolume.store(volume, std::memory_order_relaxed);
    flush();
    outputRequested.store(true, std::memory_order_release);
    const uint32_t start = millis();
    while (!outputOwned.load(std::memory_order_acquire) && millis() - start < timeoutMs)
        vTaskDelay(pdMS_TO_TICKS(2));
    if (!outputOwned.load(std::memory_order_acquire)) {
        outputRequested.store(false, std::memory_order_release);
        return false;
    }
    acceptPcm.store(true, std::memory_order_release);
    if (handler) handler->getTrackPlayer()->resumeForSpotify();
    pendingActivation.store(false, std::memory_order_release);
    return true;
}

bool spotifyAdapterReleaseOutput(uint32_t timeoutMs) {
    acceptPcm.store(false, std::memory_order_release);
    pendingActivation.store(false, std::memory_order_release);
    outputRequested.store(false, std::memory_order_release);
    flush();
    if (handler) handler->getTrackPlayer()->suspendForLocal();
    const uint32_t start = millis();
    while ((outputOwned.load(std::memory_order_acquire) ||
            (handler && !handler->getTrackPlayer()->localSuspendAcknowledged())) &&
           millis() - start < timeoutMs)
        vTaskDelay(pdMS_TO_TICKS(2));
    const bool released = !outputOwned.load(std::memory_order_acquire);
    const bool stopped = !handler || handler->getTrackPlayer()->localSuspendAcknowledged();
    Serial.printf("[s2] producer-stop released=%d stopped=%d internal=%u largest=%u\n",
        released, stopped,
        static_cast<unsigned>(heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT)),
        static_cast<unsigned>(heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT)));
    return released && stopped;
}

void spotifyAdapterSetOutputVolume(uint8_t volume) {
    requestedVolume.store(volume, std::memory_order_relaxed);
}

void spotifyAdapterSetTone(int8_t bassDb, int8_t middleDb, int8_t trebleDb) {
    bassTone.store(bassDb, std::memory_order_relaxed);
    middleTone.store(middleDb, std::memory_order_relaxed);
    trebleTone.store(trebleDb, std::memory_order_relaxed);
    toneRevision.fetch_add(1, std::memory_order_release);
}

bool spotifyAdapterReady() { return ready.load(std::memory_order_acquire); }
