#include "spotify_adapter.h"

#include <algorithm>
#include <atomic>
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
constexpr size_t kPcmCapacity = 128 * 1024;
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
std::mutex pcmMutex;
std::unique_ptr<bell::CircularBuffer> pcm;
std::deque<uint64_t> boundaries;
std::string producerTrack;
uint64_t producedBytes = 0, consumedBytes = 0;
std::shared_ptr<cspot::SpircHandler> handler;
StaticTask_t outputTcb, connectTcb;
StackType_t outputStack[8 * 1024 / sizeof(StackType_t)];
StackType_t connectStack[16 * 1024 / sizeof(StackType_t)];

void post(SpotifySignalType type, uint16_t volume = 0) {
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
    signals[(signalHead + signalCount) % kSignalCapacity] =
        {type, ++nextSignalSequence, volume};
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
    if (!acceptPcm.load(std::memory_order_acquire))
        return pendingActivation.load(std::memory_order_acquire) ? 0 : length;
    const uint32_t generation = pcmGeneration.load(std::memory_order_relaxed);
    std::lock_guard<std::mutex> lock(pcmMutex);
    if (!acceptPcm.load(std::memory_order_relaxed) ||
        generation != pcmGeneration.load(std::memory_order_relaxed) || !pcm) return length;
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
    uint8_t buffer[1024];
    for (;;) {
        if (!outputRequested.load(std::memory_order_acquire)) {
            if (sink) {
                sink.reset();
                appliedToneRevision = UINT32_MAX;
                outputOwned.store(false, std::memory_order_release);
                Serial.println("[s2] spotify output released");
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
            Serial.println("[s2] spotify output acquired");
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
        {
            std::lock_guard<std::mutex> lock(pcmMutex);
            if (pcm) bytes = pcm->read(buffer, sizeof(buffer));
            consumedBytes += bytes;
            while (!boundaries.empty() && boundaries.front() < consumedBytes) {
                boundaries.pop_front();
                ++reached;
            }
        }
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
            if (paused.load(std::memory_order_relaxed)) post(SpotifySignalType::Pause);
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
    bell::setDefaultLogger();
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
    xTaskCreateStaticPinnedToCore(connectTask, "spotify_connect", sizeof(connectStack),
        nullptr, 5, connectStack, &connectTcb, 1);
}

String spotifyAdapterInfoJson() {
    if (!ready.load(std::memory_order_acquire) || !pairingBlob) return String();
    return String(pairingBlob->buildZeroconfInfo().c_str());
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
    pendingActivation.store(false, std::memory_order_release);
    return true;
}

bool spotifyAdapterReleaseOutput(uint32_t timeoutMs) {
    acceptPcm.store(false, std::memory_order_release);
    pendingActivation.store(false, std::memory_order_release);
    outputRequested.store(false, std::memory_order_release);
    flush();
    const uint32_t start = millis();
    while (outputOwned.load(std::memory_order_acquire) && millis() - start < timeoutMs)
        vTaskDelay(pdMS_TO_TICKS(2));
    return !outputOwned.load(std::memory_order_acquire);
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
