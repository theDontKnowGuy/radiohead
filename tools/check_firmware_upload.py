"""Compile production local-OTA callbacks against a flash/HTTP fixture."""
from pathlib import Path
import subprocess
import tempfile

root = Path(__file__).resolve().parents[1]
source = (root / 'src/web_server.cpp').read_text()
state = source[source.index('bool otaUploadStarted ='):source.index('bool artworkUploadAccepted =')]
callbacks = source[source.index('void failFirmwareUpload('):source.index('String programsJson()')]
parser = source[source.index('bool parseIntegerArg('):source.index('void sendBadRequest(')]
fixture = r'''
#include <cassert>
#include <cerrno>
#include <cstdlib>
#include <cstring>
#include <cstdint>
#include <string>
#include <vector>
#include <algorithm>
class String : public std::string {
public:
 using std::string::string;
 String(const std::string& s):std::string(s){}
 bool isEmpty()const{return empty();}
 String substring(size_t start)const{return substr(start);}
 bool equalsIgnoreCase(const char* other)const{
  auto a=*this;std::transform(a.begin(),a.end(),a.begin(),::tolower);return a==other;
 }
};
struct __attribute__((packed)) esp_image_header_t {uint8_t magic;uint8_t pad[11];uint16_t chip_id;uint8_t tail[10];};
struct esp_image_segment_header_t {uint32_t address,size;};
constexpr uint8_t ESP_IMAGE_HEADER_MAGIC=0xe9;
constexpr uint16_t ESP_CHIP_ID_ESP32S3=9;
constexpr uint32_t ESP_APP_DESC_MAGIC_WORD=0xabcd5432;
constexpr int U_FLASH=0;
enum {UPLOAD_FILE_START,UPLOAD_FILE_WRITE,UPLOAD_FILE_END,UPLOAD_FILE_ABORTED};
struct HTTPUpload {int status=0;String name="firmware",filename="firmware.bin";size_t totalSize=0,currentSize=0;uint8_t buf[4096];};
struct {size_t getFreeSketchSpace(){return 6553600;}} ESP;
struct {
 bool busy=false,allow=true;
 unsigned locks=0,unlocks=0;
 bool tryBeginLocalMaintenance(){++locks;if(busy||!allow)return false;return busy=true;}
 void endLocalMaintenance(){assert(busy);busy=false;++unlocks;}
} firmwareUpdater;
struct {
 bool beginOk=true,writeOk=true,endOk=true,running=false;
 unsigned begins=0,ends=0,aborts=0;
 size_t expected=0;
 std::vector<uint8_t> written;
 bool begin(size_t size,int){assert(firmwareUpdater.busy);++begins;expected=size;written.clear();return running=beginOk;}
 size_t write(const uint8_t* p,size_t n){assert(running);if(!writeOk)return 0;written.insert(written.end(),p,p+n);return n;}
 void abort(){++aborts;running=false;}
 bool end(){++ends;assert(running&&written.size()==expected);running=false;return endOk;}
} Update;
struct {
 HTTPUpload data;
 String size="128",body;
 int code=0;
 HTTPUpload& upload(){return data;}
 bool hasArg(const char*){return !size.empty();}
 String arg(const char*){return size;}
 void send(int c,const char*,const String& b){code=c;body=b;}
} server;
enum class WebRestartAction{None,FirmwareUpdate};
WebRestartAction webRestartAction=WebRestartAction::None;
bool webRestartScheduled=false;
unsigned long webRestartAt=0;
constexpr unsigned long WEB_RESTART_DELAY_MS=500;
unsigned long millis(){return 100;}
bool active=false;
uint8_t percent=0;
unsigned paints=0;
void setFirmwareUpdateProgress(bool a,uint8_t p){active=a;percent=p;}
void serviceFirmwareUpdateDisplay(unsigned long){}
void serviceDisplayRefresh(unsigned long,const char*,bool){++paints;}
// Settings upload cleanup shares this service entry point; its production
// behavior is exercised separately by check_settings_backup.py.
bool settingsUploadReceived=false,settingsUploadReserved=false;
void clearSettingsUpload(){assert(false);}
'''
fixture += state
fixture += 'bool webMaintenanceBusy(){return otaUploadStarted||firmwareUpdater.busy;}\n'
fixture += parser + callbacks
cleanup = source[source.index('void serviceWebNetworkRequests(unsigned long now) {'):source.index('    if (webTouchCalibrationPending &&')]
fixture += cleanup.replace('{', '{ (void)now;', 1) + '}\n'
fixture += r'''
std::vector<uint8_t> image(){std::vector<uint8_t> v(128,0x5a);v[0]=0xe9;v[12]=9;v[13]=0;uint32_t magic=ESP_APP_DESC_MAGIC_WORD;memcpy(v.data()+32,&magic,4);return v;}
void event(int status,const std::vector<uint8_t>& v={}){
 server.data.status=status;server.data.currentSize=v.size();
 std::copy(v.begin(),v.end(),server.data.buf);handleUpdateUpload();
}
void reset(){
 otaUploadStarted=otaUploadSucceeded=otaUploadReceived=otaFlashStarted=false;
 otaUploadBytes=otaUploadExpectedBytes=otaUploadHeaderBytes=0;
 otaUploadErrorStatus=400;otaUploadError="Choose one compiled firmware.bin application image.";
 firmwareUpdater={};Update={};server={};active=false;percent=0;paints=0;
 webRestartScheduled=false;webRestartAction=WebRestartAction::None;
}
int main(){
 auto v=image();
 reset();event(UPLOAD_FILE_START);assert(firmwareUpdater.busy&&active&&Update.begins==0);
 // Header split across chunks must be buffered and written exactly once.
 event(UPLOAD_FILE_WRITE,{v.begin(),v.begin()+12});assert(Update.begins==0);
 event(UPLOAD_FILE_WRITE,{v.begin()+12,v.begin()+64});assert(percent==50&&Update.written.size()==64);
 event(UPLOAD_FILE_WRITE,{v.begin()+64,v.end()});assert(percent==99&&Update.written==v);
 event(UPLOAD_FILE_END);assert(Update.ends==0&&firmwareUpdater.busy);finishFirmwareUpload();
 assert(server.code==202&&Update.ends==1&&webRestartScheduled&&percent==100&&paints>=4&&firmwareUpdater.busy);
 // Validation rejects before flash begins, even if browser checks are bypassed.
 for(int mutation:{0,12,32}){reset();auto bad=v;bad[mutation]=0;event(UPLOAD_FILE_START);event(UPLOAD_FILE_WRITE,bad);event(UPLOAD_FILE_END);finishFirmwareUpload();assert(server.code==400&&Update.begins==0&&!webRestartScheduled&&!firmwareUpdater.busy);}
 for(const char* size:{"","0","-1","35","6553601","128oops","2147483648"}){reset();server.size=size;event(UPLOAD_FILE_START);event(UPLOAD_FILE_WRITE,v);finishFirmwareUpload();assert(server.code==400&&Update.begins==0&&!firmwareUpdater.busy);}
 for(const char* filename:{"firmware.elf","firmware",""}){reset();server.data.filename=filename;event(UPLOAD_FILE_START);finishFirmwareUpload();assert(server.code==400&&Update.begins==0);}
 reset();server.data.name="other";event(UPLOAD_FILE_START);finishFirmwareUpload();assert(server.code==400);
 reset();firmwareUpdater.busy=true;event(UPLOAD_FILE_START);finishFirmwareUpload();assert(server.code==409&&firmwareUpdater.busy&&Update.aborts==0&&firmwareUpdater.unlocks==0);
 reset();firmwareUpdater.allow=false;event(UPLOAD_FILE_START);finishFirmwareUpload();assert(server.code==409&&Update.begins==0);
 reset();webRestartScheduled=true;event(UPLOAD_FILE_START);finishFirmwareUpload();assert(server.code==409&&Update.begins==0);
 reset();event(UPLOAD_FILE_START);event(UPLOAD_FILE_WRITE,{v.begin(),v.begin()+64});event(UPLOAD_FILE_END);finishFirmwareUpload();assert(server.code==400&&Update.ends==0&&Update.aborts==1&&!firmwareUpdater.busy);
 reset();server.size="64";event(UPLOAD_FILE_START);event(UPLOAD_FILE_WRITE,v);finishFirmwareUpload();assert(server.code==400&&Update.begins==0);
 reset();event(UPLOAD_FILE_START);event(UPLOAD_FILE_WRITE,v);event(UPLOAD_FILE_END);event(UPLOAD_FILE_START);finishFirmwareUpload();assert(server.code==400&&Update.ends==0&&Update.aborts==1);
 reset();event(UPLOAD_FILE_START);event(UPLOAD_FILE_WRITE,v);event(UPLOAD_FILE_ABORTED);assert(!firmwareUpdater.busy&&Update.aborts==1&&!otaUploadReceived);
 event(UPLOAD_FILE_START);event(UPLOAD_FILE_WRITE,v);event(UPLOAD_FILE_END);finishFirmwareUpload();assert(server.code==202); // retry after disconnect
 for(int failure:{0,1,2}){reset();if(failure==0)Update.beginOk=false;if(failure==1)Update.writeOk=false;if(failure==2)Update.endOk=false;event(UPLOAD_FILE_START);event(UPLOAD_FILE_WRITE,v);event(UPLOAD_FILE_END);finishFirmwareUpload();assert(server.code==500&&!firmwareUpdater.busy&&!webRestartScheduled&&!active);}
 reset();event(UPLOAD_FILE_START);event(UPLOAD_FILE_WRITE,v);event(UPLOAD_FILE_END);serviceWebNetworkRequests(0);assert(!firmwareUpdater.busy&&!otaUploadReceived&&Update.ends==0&&Update.aborts==1);
 reset();finishFirmwareUpload();assert(server.code==400&&Update.ends==0); // no multipart file
}
'''
with tempfile.TemporaryDirectory(prefix='radiohead-ota-') as directory:
    path = Path(directory)
    (path / 'check.cpp').write_text(fixture)
    subprocess.run(['clang++', '-std=c++17', '-Wall', '-Wextra', '-Werror',
                    '-fsanitize=address,undefined', str(path / 'check.cpp'), '-o', str(path / 'check')], check=True)
    subprocess.run([str(path / 'check')], check=True)
print('Production firmware upload: validation, byte accounting, progress, exclusion, errors and retry passed')
