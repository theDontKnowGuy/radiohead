#pragma once
#include "nvs.h"
#include <cassert>
#include <cstring>
#include <map>
#include <string>
#include <vector>
#include <utility>
struct Value {
    nvs_type_t type;
    std::vector<uint8_t> bytes;
    bool operator==(const Value& b) const { return type == b.type && bytes == b.bytes; }
};
using Entries = std::map<std::string, Value>;
inline std::map<std::string, Entries> storage;
inline int mutations = 0, failAt = -1, handles = 0, iterators = 0;
inline bool persistentFailure = false, readFailure = false;
struct Handle { std::string ns; };
struct Iterator { std::string ns; std::vector<std::string> keys; size_t index = 0; };
bool failure() {
    ++mutations;
    return failAt > 0 && (mutations == failAt || (persistentFailure && mutations >= failAt));
}
esp_err_t nvs_open(const char* ns, int mode, nvs_handle_t* out) {
    if (mode == NVS_READONLY && readFailure) return ESP_ERR_INVALID_ARG;
    if (mode == NVS_READONLY && !storage.count(ns)) return ESP_ERR_NVS_NOT_FOUND;
    storage[ns]; *out = new Handle{ns}; ++handles; return ESP_OK;
}
void nvs_close(nvs_handle_t h) { delete h; --handles; }
esp_err_t nvs_erase_all(nvs_handle_t h) { if (failure()) return 1; storage[h->ns].clear(); return ESP_OK; }
esp_err_t nvs_commit(nvs_handle_t) { return failure() ? 1 : ESP_OK; }
esp_err_t nvs_entry_find(const char* part, const char* ns, nvs_type_t, nvs_iterator_t* out) {
    assert(std::string(part) == "nvs");
    *out = nullptr;
    if (storage[ns].empty()) return ESP_ERR_NVS_NOT_FOUND;
    *out = new Iterator{ns, {}, 0}; ++iterators;
    for (const auto& row : storage[ns]) (*out)->keys.push_back(row.first);
    return ESP_OK;
}
esp_err_t nvs_entry_info(nvs_iterator_t it, nvs_entry_info_t* info) {
    const auto& key = it->keys[it->index];
    *info = {}; std::strncpy(info->key, key.c_str(), 15);
    std::strncpy(info->namespace_name, it->ns.c_str(), 15);
    info->type = storage[it->ns][key].type; return ESP_OK;
}
esp_err_t nvs_entry_next(nvs_iterator_t* it) {
    if (++(*it)->index < (*it)->keys.size()) return ESP_OK;
    delete *it; *it = nullptr; --iterators; return ESP_ERR_NVS_NOT_FOUND;
}
void nvs_release_iterator(nvs_iterator_t it) { if (it) { delete it; --iterators; } }
esp_err_t read(nvs_handle_t h, const char* key, nvs_type_t type, void* out, size_t* length) {
    const auto it = storage[h->ns].find(key);
    if (it == storage[h->ns].end() || it->second.type != type) return 1;
    const auto& bytes = it->second.bytes;
    if (out && *length < bytes.size()) return 1;
    if (out) memcpy(out, bytes.data(), bytes.size());
    *length = bytes.size(); return ESP_OK;
}
esp_err_t write(nvs_handle_t h, const char* key, nvs_type_t type, const void* bytes, size_t size) {
    if (failure()) return 1;
    const auto* start = static_cast<const uint8_t*>(bytes);
    storage[h->ns][key] = {type, {start, start + size}}; return ESP_OK;
}
#define SCALAR(suffix, type, tag) \
 esp_err_t nvs_get_##suffix(nvs_handle_t h, const char* k, type* v) { size_t n=sizeof(*v); return read(h,k,tag,v,&n); } \
 esp_err_t nvs_set_##suffix(nvs_handle_t h, const char* k, type v) { return write(h,k,tag,&v,sizeof(v)); }
SCALAR(u8,uint8_t,NVS_TYPE_U8) SCALAR(i8,int8_t,NVS_TYPE_I8)
SCALAR(u16,uint16_t,NVS_TYPE_U16) SCALAR(i16,int16_t,NVS_TYPE_I16)
SCALAR(u32,uint32_t,NVS_TYPE_U32) SCALAR(i32,int32_t,NVS_TYPE_I32)
SCALAR(u64,uint64_t,NVS_TYPE_U64) SCALAR(i64,int64_t,NVS_TYPE_I64)
#undef SCALAR
esp_err_t nvs_get_str(nvs_handle_t h,const char* k,char* p,size_t* s){return read(h,k,NVS_TYPE_STR,p,s);}
esp_err_t nvs_get_blob(nvs_handle_t h,const char* k,void* p,size_t* s){return read(h,k,NVS_TYPE_BLOB,p,s);}
esp_err_t nvs_set_str(nvs_handle_t h,const char* k,const char* p){return write(h,k,NVS_TYPE_STR,p,strlen(p)+1);}
esp_err_t nvs_set_blob(nvs_handle_t h,const char* k,const void* p,size_t s){return write(h,k,NVS_TYPE_BLOB,p,s);}
