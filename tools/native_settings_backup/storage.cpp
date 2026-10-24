#include "settings_backup.h"
#include "fake_nvs.h"
#include <cstdio>
using namespace SettingsBackup;
using Bytes = std::vector<uint8_t>;
void seal(Bytes& bytes) {
    uint32_t crc = 0xffffffffU;
    for (size_t i=16;i<bytes.size();++i) {
        crc ^= bytes[i];
        for (int bit=0;bit<8;++bit) crc = (crc>>1) ^ (0xedb88320U & (0U-(crc&1U)));
    }
    crc = ~crc;
    for (int i=0;i<4;++i) { bytes[8+i]=(bytes.size()-16)>>(i*8); bytes[12+i]=crc>>(i*8); }
}
void rejected(Bytes bytes) {
    seal(bytes);
    const auto before=storage;
    const int writes=mutations;
    assert(!validate(bytes.data(),bytes.size()));
    assert(restore(bytes.data(),bytes.size())==RestoreResult::Invalid);
    assert(storage==before && mutations==writes);
}
int main() {
    nvs_handle_t h;
    for (const char* ns : {"radio","wifi","favorites","touch","spotify","system"}) {
        assert(nvs_open(ns,NVS_READWRITE,&h)==ESP_OK);
        nvs_set_str(h,"text","synthetic only"); nvs_set_str(h,"empty","");
        uint8_t blob[]={0,255,0,42}; nvs_set_blob(h,"blob",blob,sizeof(blob));
        nvs_set_i8(h,"i8",-127); nvs_set_u8(h,"u8",255);
        nvs_set_i16(h,"i16",-30000); nvs_set_u16(h,"u16",65000);
        nvs_set_i32(h,"i32",-2000000000); nvs_set_u32(h,"u32",4000000000U);
        nvs_set_i64(h,"i64",INT64_MIN); nvs_set_u64(h,"u64",UINT64_MAX);
        nvs_close(h);
    }
    const auto expected=storage;
    Buffer backup;
    assert(capture(backup) && validate(backup.data.get(),backup.size));
    Bytes bytes(backup.data.get(),backup.data.get()+backup.size);
    storage["radio"].clear(); storage["wifi"]["stale"]={NVS_TYPE_U8,{4}};
    assert(restore(bytes.data(),bytes.size())==RestoreResult::Ok && storage==expected);
    assert(handles==0 && iterators==0);
    // Every truncation and single-byte corruption is rejected before mutation.
    for (size_t n=0;n<bytes.size();++n) assert(!validate(bytes.data(),n));
    for (size_t i=0;i<bytes.size();++i) {
        auto bad=bytes; bad[i]^=0x80; assert(!validate(bad.data(),bad.size()));
    }
    auto bad=bytes; bad[16]=5; rejected(bad); // System namespace escape.
    bad=bytes; bad[17]=0xff; rejected(bad); // Unknown type.
    bad=bytes; bad[18]=16; rejected(bad); // Key too long.
    bad=bytes; bad[19]=1; rejected(bad); // Reserved byte.
    bad=bytes; bad[20]=0xff; bad[21]=0xff; rejected(bad); // Value beyond limit.
    bad=bytes; bad[24]=0; rejected(bad); // Embedded NUL in key.
    // Duplicate the first record with a corrected checksum.
    const size_t firstSize=8+bytes[18]+bytes[20]+(size_t(bytes[21])<<8);
    bad=bytes; bad.insert(bad.end(),bytes.begin()+16,bytes.begin()+16+firstSize); rejected(bad);
    // Invalid string termination and scalar sizes with otherwise valid framing.
    size_t offset=16;
    while (offset<bytes.size()) {
        const size_t n=bytes[offset+4]+(size_t(bytes[offset+5])<<8);
        if (bytes[offset+1]==NVS_TYPE_STR) {
            bad=bytes; bad[offset+8+bytes[offset+2]+n-1]=1; rejected(bad); break;
        }
        offset+=8+bytes[offset+2]+n;
    }
    bad=bytes; bad[17]=NVS_TYPE_U64; rejected(bad); // First record is a four-byte blob.
    bad=bytes; bad.resize(MaxBytes+1); rejected(bad);
    // Capture failures must happen before any erase/write.
    readFailure=true; const int oldMutations=mutations;
    assert(restore(bytes.data(),bytes.size())==RestoreResult::SnapshotFailed);
    assert(storage==expected && mutations==oldMutations); readFailure=false;
    // Inject a failure at every erase/set/commit in the apply path. Rollback must
    // recover the complete OLD snapshot, including keys not in the imported one.
    storage["radio"]["oldonly"]={NVS_TYPE_U8,{7}};
    const auto previous=storage;
    failAt=-1; mutations=0;
    assert(restore(bytes.data(),bytes.size())==RestoreResult::Ok);
    const int applyOperations=mutations;
    for (int fail=1;fail<=applyOperations;++fail) {
        storage=previous; mutations=0; failAt=fail;
        assert(restore(bytes.data(),bytes.size())==RestoreResult::RolledBack);
        assert(storage==previous && handles==0 && iterators==0);
    }
    storage=previous; mutations=0; failAt=12; persistentFailure=true;
    assert(restore(bytes.data(),bytes.size())==RestoreResult::RollbackFailed);
    failAt=-1; persistentFailure=false;
    // Empty namespaces/fresh chip round-trip and too-large stored value fail closed.
    storage.clear(); assert(capture(backup) && backup.size==16);
    assert(restore(backup.data.get(),backup.size)==RestoreResult::Ok);
    storage["radio"]["large"]={NVS_TYPE_BLOB,Bytes(16385,42)};
    assert(!capture(backup) && !backup.data && backup.size==0);
    assert(handles==0 && iterators==0);
    puts("Settings backup: all NVS types/namespaces, exact replacement, corruption/bounds, rollback and failure injection passed.");
}
