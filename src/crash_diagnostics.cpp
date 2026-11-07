#include "crash_diagnostics.h"
#include <Arduino.h>
#include <esp_core_dump.h>
#include <esp_partition.h>
#include <esp_rom_crc.h>
#include <esp_system.h>
#include <cstring>

namespace {
static_assert(APP_ELF_SHA256_SZ >= 65, "Crash decoding requires the full ELF hash");
const char* state = "uninitialized";
char id[9]{};
size_t dumpBytes = 0;
esp_err_t checkError = ESP_OK;
esp_core_dump_summary_t summary{};
const char* reason = "unknown";
bool empty = false;

const char* classify(const char* text) {
    // Panic/assert text may include arbitrary application data. Export only
    // these fixed categories, never that text, general registers or stack bytes.
    if (strstr(text, "Radiohead controlled crash test")) return "controlled_test";
    if (strstr(text, "assert failed")) return "assertion";
    if (strstr(text, "Stack canary") || strstr(text, "stack overflow")) return "stack_overflow";
    if (strstr(text, "wdt") || strstr(text, "watchdog")) return "watchdog";
    if (strstr(text, "abort()")) return "abort";
    return "exception_or_panic";
}
}

void crashDiagnosticsBegin() {
    summary = {};
    id[0] = 0;
    empty = false;
    dumpBytes = 0;
    reason = "unknown";
    const auto* partition = esp_partition_find_first(ESP_PARTITION_TYPE_DATA,
        ESP_PARTITION_SUBTYPE_DATA_COREDUMP, nullptr);
    if (!partition || partition->size != 0x10000) { state = "partition_error"; return; }
    uint32_t length = 0;
    checkError = esp_partition_read(partition, 0, &length, sizeof(length));
    if (checkError != ESP_OK) { state = "read_error"; return; }
    // IDF's no-overwrite policy treats an erased first word as armed/empty.
    if (length == UINT32_MAX) { state = "empty"; empty = true; return; }
    // A torn/invalid dump is evidence too. Fingerprint the entire bounded
    // partition so stale clients cannot accidentally clear different evidence.
    uint8_t buffer[256];
    uint32_t crc = 0;
    for (size_t at = 0; at < partition->size; at += sizeof(buffer)) {
        checkError = esp_partition_read(partition, at, buffer, sizeof(buffer));
        if (checkError != ESP_OK) { state = "read_error"; return; }
        crc = esp_rom_crc32_le(crc, buffer, sizeof(buffer));
    }
    snprintf(id, sizeof(id), "%08lx", static_cast<unsigned long>(crc));
    checkError = esp_core_dump_image_check();
    if (checkError != ESP_OK) { state = "invalid"; return; }
    size_t address = 0;
    checkError = esp_core_dump_image_get(&address, &dumpBytes);
    if (checkError != ESP_OK || address != partition->address || dumpBytes > partition->size) {
        state = "invalid"; dumpBytes = 0; return;
    }
    checkError = esp_core_dump_get_summary(&summary);
    if (checkError != ESP_OK) { state = "summary_error"; return; }
    char panic[192]{};
    if (esp_core_dump_get_panic_reason(panic, sizeof(panic)) == ESP_OK) reason = classify(panic);
    memset(panic, 0, sizeof(panic));
    state = "valid";
}

bool crashDiagnosticsEmpty() { return empty; }

bool crashDiagnosticsClear(const String& expectedId) {
    if (!id[0] || expectedId != id) return false;
    if (esp_core_dump_image_erase() != ESP_OK) return false;
    crashDiagnosticsBegin();
    return empty;
}

String crashDiagnosticsJson() {
    String json;
    if (!json.reserve(1400)) return String();
    char text[400];
    snprintf(text, sizeof(text),
        "{\"schema\":1,\"state\":\"%s\",\"id\":\"%s\",\"bytes\":%u,"
        "\"error\":%d,\"captureArmed\":%s,\"testBuild\":%s",
        state, id, unsigned(dumpBytes), int(checkError), empty ? "true" : "false",
#if defined(RADIOHEAD_CRASH_TEST)
        "true"
#else
        "false"
#endif
    );
    json += text;
    if (strcmp(state, "valid") == 0) {
        // IDs/task names originate in the dump: validate before JSON insertion.
        char build[65]{};
        bool validBuild = true;
        for (size_t i = 0; i < 64; ++i) {
            const char c = summary.app_elf_sha256[i];
            if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f'))) { validBuild = false; break; }
            build[i] = c;
        }
        if (!validBuild) build[0] = 0;
        char task[17]{};
        for (size_t i = 0; i < sizeof(summary.exc_task) && summary.exc_task[i]; ++i) {
            const char c = summary.exc_task[i];
            task[i] = ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
                (c >= '0' && c <= '9') || c == '_' || c == '-') ? c : '?';
        }
        snprintf(text, sizeof(text),
            ",\"build\":\"%s\",\"task\":\"%s\",\"reason\":\"%s\",\"pc\":%lu,"
            "\"exceptionCause\":%lu,\"faultAddress\":%lu,\"backtraceCorrupt\":%s,\"backtrace\":[",
            build, task, reason, static_cast<unsigned long>(summary.exc_pc),
            static_cast<unsigned long>(summary.ex_info.exc_cause),
            static_cast<unsigned long>(summary.ex_info.exc_vaddr),
            summary.exc_bt_info.corrupted ? "true" : "false");
        json += text;
        const size_t depth = std::min<size_t>(summary.exc_bt_info.depth, 16);
        for (size_t i = 0; i < depth; ++i) {
            snprintf(text, sizeof(text), "%s%lu", i ? "," : "", static_cast<unsigned long>(summary.exc_bt_info.bt[i]));
            json += text;
        }
        json += "]";
    }
    json += "}";
    return json;
}

#if defined(RADIOHEAD_CRASH_TEST)
bool crashDiagnosticsReadTestPartition(const String& expectedId, size_t offset, uint8_t* bytes, size_t count) {
    if (!id[0] || expectedId != id || offset > 0x10000 || count > 0x10000 - offset || !bytes) return false;
    const auto* partition = esp_partition_find_first(ESP_PARTITION_TYPE_DATA,
        ESP_PARTITION_SUBTYPE_DATA_COREDUMP, nullptr);
    return partition && partition->size == 0x10000 && esp_partition_read(partition, offset, bytes, count) == ESP_OK;
}
__attribute__((noinline)) [[noreturn]] void radioCrashTestFault() {
    esp_system_abort("Radiohead controlled crash test");
}
#endif
