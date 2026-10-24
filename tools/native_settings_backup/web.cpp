#include "settings_backup.h"
#include "fake_nvs.h"
#include <cstdio>
using String=std::string;
enum UploadStatus { UPLOAD_FILE_START, UPLOAD_FILE_WRITE, UPLOAD_FILE_END, UPLOAD_FILE_ABORTED };
struct HTTPUpload {
    UploadStatus status=UPLOAD_FILE_START;
    String name="backup";
    size_t currentSize=0,totalSize=0;
    uint8_t* buf=nullptr;
};
struct {
    std::map<String,String> requestHeaders,responseHeaders;
    HTTPUpload current;
    String body;
    int status=0;
    size_t contentLength=0;
    String header(const char* key) { return requestHeaders[key]; }
    void sendHeader(const char* k,const char* v) { responseHeaders[k]=v; }
    void setContentLength(size_t n) { contentLength=n; }
    void send(int code,const char*,const String& value) { status=code;body=value; }
    void sendContent(const char* value,size_t n) { body.append(value,n); }
    HTTPUpload& upload() { return current; }
} server;
struct {
    bool busy=false;
    int releases=0;
    bool tryBeginLocalMaintenance(){if(busy)return false;busy=true;return true;}
    void endLocalMaintenance(){assert(busy);busy=false;++releases;}
} firmwareUpdater;
bool webRestartScheduled=false;
bool webMaintenanceBusy(){return firmwareUpdater.busy;}
void sendMaintenanceBusy(){server.status=409;}
bool mediaLocalAudioAvailable(){return true;}
struct {int loops=0;void loop(){++loops;}} audio;
struct Restart {};
struct {void restart(){throw Restart{};}} ESP;
void delay(int ms){assert(ms==100);}
#include "web.inc"
using Bytes=std::vector<uint8_t>;
void resetRequest(bool authorized=true) {
    clearSettingsUpload();
    server.requestHeaders.clear();server.responseHeaders.clear();
    if(authorized)server.requestHeaders["X-Radiohead-Backup"]="1";
    server.body.clear();server.status=0;server.current={};
}
void event(UploadStatus status) {server.current.status=status;handleSettingsRestoreUpload();}
void upload(const Bytes& bytes) {
    event(UPLOAD_FILE_START);
    for(size_t i=0;i<bytes.size();++i){
        server.current.buf=const_cast<uint8_t*>(bytes.data()+i);
        server.current.currentSize=1;event(UPLOAD_FILE_WRITE);
    }
    server.current.totalSize=bytes.size();event(UPLOAD_FILE_END);
}
int main() {
    for(const char* ns:{"radio","wifi","favorites","touch","spotify"})storage[ns];
    storage["radio"]["vol"]={NVS_TYPE_I32,{5,0,0,0}};
    storage["spotify"]["test"]={NVS_TYPE_STR,{'f','a','k','e',0}};
    const auto original=storage;
    resetRequest(false);handleSettingsBackup();assert(server.status==403&&storage==original);
    resetRequest();handleSettingsBackup();assert(server.status==200);
    assert(server.responseHeaders["Cache-Control"]=="no-store");
    assert(server.responseHeaders["Content-Disposition"].find("attachment;")==0);
    assert(server.contentLength==server.body.size()&&!firmwareUpdater.busy);
    const Bytes backup(server.body.begin(),server.body.end());
    assert(SettingsBackup::validate(backup.data(),backup.size()));
    resetRequest();firmwareUpdater.busy=true;handleSettingsBackup();assert(server.status==409);
    firmwareUpdater.busy=false;
    resetRequest(false);upload(backup);finishSettingsRestore();assert(server.status==403&&storage==original);
    resetRequest();finishSettingsRestore();assert(server.status==400&&storage==original);
    resetRequest();server.current.name="wrong";upload(backup);finishSettingsRestore();assert(server.status==400&&storage==original);
    resetRequest();webRestartScheduled=true;upload(backup);finishSettingsRestore();assert(server.status==409&&storage==original);webRestartScheduled=false;
    resetRequest();firmwareUpdater.busy=true;upload(backup);finishSettingsRestore();
    assert(server.status==409&&firmwareUpdater.busy&&storage==original);firmwareUpdater.busy=false;
    resetRequest();upload(backup);assert(storage==original&&firmwareUpdater.busy); // END is not commit.
    event(UPLOAD_FILE_ABORTED);assert(!settingsUpload.data&&!firmwareUpdater.busy&&storage==original);
    resetRequest();upload(backup);upload(backup);finishSettingsRestore();assert(server.status==400&&storage==original&&!firmwareUpdater.busy);
    resetRequest();upload(Bytes(SettingsBackup::MaxBytes+1,42));finishSettingsRestore();assert(server.status==413&&storage==original&&!firmwareUpdater.busy);
    resetRequest();auto broken=backup;broken.back()^=1;upload(broken);finishSettingsRestore();assert(server.status==400&&storage==original);
    resetRequest();upload(backup);server.current.totalSize++;event(UPLOAD_FILE_END);finishSettingsRestore();assert(server.status==400&&storage==original);
    resetRequest();upload(backup);clearSettingsUpload();assert(!firmwareUpdater.busy&&!settingsUpload.data&&storage==original); // Parser cleanup.
    resetRequest();upload(backup);mutations=0;failAt=12;finishSettingsRestore();
    assert(server.status==500&&storage==original&&!firmwareUpdater.busy);failAt=-1;
    resetRequest();storage["radio"]["vol"].bytes[0]=9;upload(backup);
    bool restarted=false;
    try{finishSettingsRestore();}catch(const Restart&){restarted=true;}
    assert(restarted&&server.status==202&&firmwareUpdater.busy&&!settingsUpload.data);
    // Missing namespaces become empty but all stored entries match the file.
    for(const auto& ns:original)assert(storage[ns.first]==ns.second);
    assert(audio.loops>0&&handles==0&&iterators==0);
    clearSettingsUpload();
    puts("Settings HTTP handlers: header/cache/export, malformed/extra/oversized/aborted uploads, maintenance, rollback and immediate restart passed.");
}
