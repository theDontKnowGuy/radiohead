#pragma once
#include <cstddef>
#include <cstdint>
using esp_err_t = int;
constexpr esp_err_t ESP_OK = 0, ESP_ERR_INVALID_ARG = 1, ESP_ERR_NVS_NOT_FOUND = 2;
constexpr int NVS_READONLY = 0, NVS_READWRITE = 1;
enum nvs_type_t {
    NVS_TYPE_U8=0x01, NVS_TYPE_I8=0x11, NVS_TYPE_U16=0x02, NVS_TYPE_I16=0x12,
    NVS_TYPE_U32=0x04, NVS_TYPE_I32=0x14, NVS_TYPE_U64=0x08, NVS_TYPE_I64=0x18,
    NVS_TYPE_STR=0x21, NVS_TYPE_BLOB=0x42, NVS_TYPE_ANY=0xff
};
struct Handle;
struct Iterator;
using nvs_handle_t = Handle*;
using nvs_iterator_t = Iterator*;
struct nvs_entry_info_t { char namespace_name[16]; char key[16]; nvs_type_t type; };
esp_err_t nvs_open(const char*, int, nvs_handle_t*);
void nvs_close(nvs_handle_t);
esp_err_t nvs_erase_all(nvs_handle_t);
esp_err_t nvs_commit(nvs_handle_t);
esp_err_t nvs_entry_find(const char*, const char*, nvs_type_t, nvs_iterator_t*);
esp_err_t nvs_entry_info(nvs_iterator_t, nvs_entry_info_t*);
esp_err_t nvs_entry_next(nvs_iterator_t*);
void nvs_release_iterator(nvs_iterator_t);
#define SCALAR(suffix, type) \
 esp_err_t nvs_get_##suffix(nvs_handle_t, const char*, type*); \
 esp_err_t nvs_set_##suffix(nvs_handle_t, const char*, type);
SCALAR(u8, uint8_t) SCALAR(i8, int8_t) SCALAR(u16, uint16_t) SCALAR(i16, int16_t)
SCALAR(u32, uint32_t) SCALAR(i32, int32_t) SCALAR(u64, uint64_t) SCALAR(i64, int64_t)
#undef SCALAR
esp_err_t nvs_get_str(nvs_handle_t, const char*, char*, size_t*);
esp_err_t nvs_get_blob(nvs_handle_t, const char*, void*, size_t*);
esp_err_t nvs_set_str(nvs_handle_t, const char*, const char*);
esp_err_t nvs_set_blob(nvs_handle_t, const char*, const void*, size_t);
