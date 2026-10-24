#include "settings_backup.h"

#include <cstring>
#include <new>
#include <nvs.h>

namespace SettingsBackup {
namespace {
constexpr char Namespaces[][10] = {"radio", "wifi", "favorites", "touch", "spotify"};
constexpr size_t NamespaceCount = sizeof(Namespaces) / sizeof(Namespaces[0]);
constexpr uint8_t Magic[8] = {'R', 'H', 'S', 'E', 'T', '0', '1', 0};
constexpr size_t HeaderBytes = 16;
constexpr size_t MaxEntries = 256;
constexpr size_t MaxValueBytes = 16384;

uint32_t read32(const uint8_t* p) {
    return uint32_t(p[0]) | uint32_t(p[1]) << 8 | uint32_t(p[2]) << 16 | uint32_t(p[3]) << 24;
}
void write32(uint8_t* p, uint32_t value) {
    for (int i = 0; i < 4; ++i) p[i] = value >> (8 * i);
}
uint32_t checksum(const uint8_t* p, size_t size) {
    uint32_t crc = 0xffffffffU;
    while (size--) {
        crc ^= *p++;
        for (int i = 0; i < 8; ++i) crc = (crc >> 1) ^ (0xedb88320U & (0U - (crc & 1U)));
    }
    return ~crc;
}
size_t scalarSize(nvs_type_t type) {
    switch (type) {
        case NVS_TYPE_U8: case NVS_TYPE_I8: return 1;
        case NVS_TYPE_U16: case NVS_TYPE_I16: return 2;
        case NVS_TYPE_U32: case NVS_TYPE_I32: return 4;
        case NVS_TYPE_U64: case NVS_TYPE_I64: return 8;
        default: return 0;
    }
}
struct Record {
    uint8_t ns = 0;
    nvs_type_t type = NVS_TYPE_ANY;
    char key[16] = {};
    const uint8_t* value = nullptr;
    size_t length = 0;
};
bool nextRecord(const uint8_t* data, size_t size, size_t& offset, Record& record) {
    if (offset > size || size - offset < 8) return false;
    const uint8_t* p = data + offset;
    const size_t keyLength = p[2];
    const size_t length = read32(p + 4);
    if (p[0] >= NamespaceCount || p[3] != 0 || keyLength == 0 || keyLength > 15 ||
        length == 0 || length > MaxValueBytes || keyLength > size - offset - 8 ||
        length > size - offset - 8 - keyLength) return false;
    record = {};
    record.ns = p[0];
    record.type = static_cast<nvs_type_t>(p[1]);
    for (size_t i = 0; i < keyLength; ++i) {
        if (p[8 + i] < 0x21 || p[8 + i] > 0x7e) return false;
        record.key[i] = p[8 + i];
    }
    record.value = p + 8 + keyLength;
    record.length = length;
    if (record.type == NVS_TYPE_STR) {
        // NVS strings include exactly one trailing NUL. Bound allocation by
        // existing app consumers; binary settings (e.g. programs) use blobs.
        if (length > 4096 || record.value[length - 1] != 0 ||
            memchr(record.value, 0, length - 1)) return false;
    } else if (record.type != NVS_TYPE_BLOB && scalarSize(record.type) != length) return false;
    offset += 8 + keyLength + length;
    return true;
}

esp_err_t readValue(nvs_handle_t handle, const char* key, nvs_type_t type,
                    uint8_t* output, size_t& size) {
    // ESP32-S3 scalar representation is little-endian, as is this file format.
#define READ_SCALAR(tag, ctype, fn) case tag: { ctype v; const auto e = fn(handle, key, &v); if (e == ESP_OK) memcpy(output, &v, sizeof(v)); size = sizeof(v); return e; }
    switch (type) {
        READ_SCALAR(NVS_TYPE_U8, uint8_t, nvs_get_u8)
        READ_SCALAR(NVS_TYPE_I8, int8_t, nvs_get_i8)
        READ_SCALAR(NVS_TYPE_U16, uint16_t, nvs_get_u16)
        READ_SCALAR(NVS_TYPE_I16, int16_t, nvs_get_i16)
        READ_SCALAR(NVS_TYPE_U32, uint32_t, nvs_get_u32)
        READ_SCALAR(NVS_TYPE_I32, int32_t, nvs_get_i32)
        READ_SCALAR(NVS_TYPE_U64, uint64_t, nvs_get_u64)
        READ_SCALAR(NVS_TYPE_I64, int64_t, nvs_get_i64)
        case NVS_TYPE_STR: return nvs_get_str(handle, key, reinterpret_cast<char*>(output), &size);
        case NVS_TYPE_BLOB: return nvs_get_blob(handle, key, output, &size);
        default: return ESP_ERR_INVALID_ARG;
    }
#undef READ_SCALAR
}
esp_err_t writeValue(nvs_handle_t handle, const Record& record) {
#define WRITE_SCALAR(tag, ctype, fn) case tag: { ctype v; memcpy(&v, record.value, sizeof(v)); return fn(handle, record.key, v); }
    switch (record.type) {
        WRITE_SCALAR(NVS_TYPE_U8, uint8_t, nvs_set_u8)
        WRITE_SCALAR(NVS_TYPE_I8, int8_t, nvs_set_i8)
        WRITE_SCALAR(NVS_TYPE_U16, uint16_t, nvs_set_u16)
        WRITE_SCALAR(NVS_TYPE_I16, int16_t, nvs_set_i16)
        WRITE_SCALAR(NVS_TYPE_U32, uint32_t, nvs_set_u32)
        WRITE_SCALAR(NVS_TYPE_I32, int32_t, nvs_set_i32)
        WRITE_SCALAR(NVS_TYPE_U64, uint64_t, nvs_set_u64)
        WRITE_SCALAR(NVS_TYPE_I64, int64_t, nvs_set_i64)
        case NVS_TYPE_STR: return nvs_set_str(handle, record.key, reinterpret_cast<const char*>(record.value));
        case NVS_TYPE_BLOB: return nvs_set_blob(handle, record.key, record.value, record.length);
        default: return ESP_ERR_INVALID_ARG;
    }
#undef WRITE_SCALAR
}

bool replace(const uint8_t* data, size_t size) {
    // Erase application keys first to make room for a replacement on the small
    // NVS partition. System namespaces are never opened or erased.
    for (const auto& name : Namespaces) {
        nvs_handle_t handle;
        const esp_err_t opened = nvs_open(name, NVS_READWRITE, &handle);
        if (opened != ESP_OK) return false;
        const bool erased = nvs_erase_all(handle) == ESP_OK && nvs_commit(handle) == ESP_OK;
        nvs_close(handle);
        if (!erased) return false;
    }
    for (size_t ns = 0; ns < NamespaceCount; ++ns) {
        nvs_handle_t handle;
        if (nvs_open(Namespaces[ns], NVS_READWRITE, &handle) != ESP_OK) return false;
        size_t offset = HeaderBytes;
        Record record;
        bool saved = true;
        while (offset < size && saved) {
            saved = nextRecord(data, size, offset, record);
            if (saved && record.ns == ns) saved = writeValue(handle, record) == ESP_OK;
        }
        saved = saved && nvs_commit(handle) == ESP_OK;
        nvs_close(handle);
        if (!saved) return false;
    }
    return true;
}
}

Buffer::~Buffer() { clear(); }
void Buffer::clear() {
    if (data) {
        volatile uint8_t* p = data.get();
        for (size_t i = 0; i < MaxBytes; ++i) p[i] = 0;
        data.reset();
    }
    size = 0;
}
bool Buffer::allocate() {
    clear();
    data.reset(new (std::nothrow) uint8_t[MaxBytes]);
    return bool(data);
}

bool validate(const uint8_t* data, size_t size) {
    if (!data || size < HeaderBytes || size > MaxBytes || memcmp(data, Magic, sizeof(Magic)) ||
        read32(data + 8) != size - HeaderBytes ||
        read32(data + 12) != checksum(data + HeaderBytes, size - HeaderBytes)) return false;
    size_t offset = HeaderBytes;
    size_t count = 0;
    while (offset < size) {
        const size_t start = offset;
        Record record;
        if (++count > MaxEntries || !nextRecord(data, size, offset, record)) return false;
        // A duplicate must never silently replace an earlier key during restore.
        size_t previous = HeaderBytes;
        while (previous < start) {
            Record other;
            if (!nextRecord(data, start, previous, other) ||
                (record.ns == other.ns && strcmp(record.key, other.key) == 0)) return false;
        }
    }
    return offset == size;
}

bool capture(Buffer& output) {
    if (!output.allocate()) return false;
    output.size = HeaderBytes;
    size_t count = 0;
    for (size_t ns = 0; ns < NamespaceCount; ++ns) {
        nvs_handle_t handle;
        const esp_err_t opened = nvs_open(Namespaces[ns], NVS_READONLY, &handle);
        if (opened == ESP_ERR_NVS_NOT_FOUND) continue;
        if (opened != ESP_OK) { output.clear(); return false; }
        nvs_iterator_t iterator = nullptr;
        esp_err_t result = nvs_entry_find("nvs", Namespaces[ns], NVS_TYPE_ANY, &iterator);
        bool valid = true;
        while (result == ESP_OK && valid) {
            nvs_entry_info_t info;
            valid = nvs_entry_info(iterator, &info) == ESP_OK && ++count <= MaxEntries;
            if (!valid) break;
            const size_t keyLength = strnlen(info.key, sizeof(info.key));
            const size_t prefix = 8 + keyLength;
            size_t length = scalarSize(info.type);
            if (info.type == NVS_TYPE_STR || info.type == NVS_TYPE_BLOB) {
                valid = readValue(handle, info.key, info.type, nullptr, length) == ESP_OK;
            }
            valid = valid && keyLength > 0 && keyLength <= 15 && length > 0 && length <= MaxValueBytes &&
                prefix <= MaxBytes - output.size && length <= MaxBytes - output.size - prefix;
            if (!valid) break;
            uint8_t* p = output.data.get() + output.size;
            p[0] = ns; p[1] = info.type; p[2] = keyLength; p[3] = 0;
            write32(p + 4, length);
            memcpy(p + 8, info.key, keyLength);
            const size_t expected = length;
            valid = readValue(handle, info.key, info.type, p + prefix, length) == ESP_OK && length == expected;
            if (!valid) break;
            output.size += prefix + length;
            result = nvs_entry_next(&iterator);
        }
        nvs_release_iterator(iterator);
        nvs_close(handle);
        if (!valid || result != ESP_ERR_NVS_NOT_FOUND) { output.clear(); return false; }
    }
    memcpy(output.data.get(), Magic, sizeof(Magic));
    write32(output.data.get() + 8, output.size - HeaderBytes);
    write32(output.data.get() + 12, checksum(output.data.get() + HeaderBytes, output.size - HeaderBytes));
    if (!validate(output.data.get(), output.size)) { output.clear(); return false; }
    return true;
}

RestoreResult restore(const uint8_t* data, size_t size) {
    if (!validate(data, size)) return RestoreResult::Invalid;
    Buffer previous;
    if (!capture(previous)) return RestoreResult::SnapshotFailed;
    if (replace(data, size)) return RestoreResult::Ok;
    return replace(previous.data.get(), previous.size) ? RestoreResult::RolledBack : RestoreResult::RollbackFailed;
}
}
