"""Execute production crash route registrations/handlers in both build modes."""
from pathlib import Path
import subprocess

ROOT = Path(__file__).resolve().parents[1]
out = ROOT / '.pio/crash_web_native'
out.mkdir(parents=True, exist_ok=True)
web = (ROOT / 'src/web_server.cpp').read_text()
routes = web[web.index('    server.addMiddleware('):web.index('    server.on("/api/radio/diagnostics/roaming"')]
fixture = r'''
#include <algorithm>
#include <cassert>
#include <cstdint>
#include <cstring>
#include <functional>
#include <map>
#include <string>
#include "web_diagnostic_routes.h"
struct String:std::string {using std::string::string; bool isEmpty()const{return empty();}};
enum {HTTP_GET=1,HTTP_POST=3};
struct WebServer;
struct Middleware {using Callback=std::function<bool()>;};
struct WebServer {
 std::map<std::string,std::function<void()>> routes;
 std::map<std::string,String> headers,args;
 int status=0;
 template<class T>void addMiddleware(T){}
 void on(const char* path,int method,std::function<void()> fn){routes[std::string(path)+std::to_string(method)]=fn;}
 String arg(const char* s){return args[s];}String header(const char*s){return headers[s];}
 String uri(){return "/api/radio/crash";}int method(){return HTTP_GET;}
 void sendHeader(const char*,const char*){}
 void send(int s){status=s;}
 template<class T>void send(int s,const char*,T){status=s;}
 int responseCode(){return status;}
 void setContentLength(size_t){}
 struct Client {size_t write(const uint8_t*,size_t n){return n;}void stop(){}};
 Client client(){return {};}
 void invoke(const char* path){routes.at(std::string(path)+"3")();}
} server;
bool webRestartScheduled=false,busy=false,empty=true,eraseSuccess=true;
int clearCalls=0,crashes=0,reserves=0;
String expected="abcd1234";
struct Updater {bool tryBeginLocalMaintenance(){++reserves;return !busy;}void endLocalMaintenance(){--reserves;}}firmwareUpdater;
bool webMaintenanceBusy(){return busy;}
void sendMaintenanceBusy(){server.send(409);}
void radioDiagnosticsWebBegin(uint32_t,uint32_t){}
void radioDiagnosticsWebEnd(int32_t){}
String crashDiagnosticsJson(){return "{}";}
bool crashDiagnosticsEmpty(){return empty;}
bool crashDiagnosticsReadTestPartition(const String& id,size_t offset,uint8_t*,size_t count){return id==expected && offset+count<=65536;}
bool crashDiagnosticsClear(const String&id){++clearCalls;return eraseSuccess && id==expected;}
void mediaSilenceSpeakers(){}
bool mediaLocalAudioAvailable(){return true;}
struct {void loop(){}}audio;
void delay(int){}
[[noreturn]]void radioCrashTestFault(){++crashes;throw 42;}
void registerRoutes(){
ROUTES
}
int main(){
 registerRoutes();
 assert(radioWebRouteId("/api/radio/crash")>0);
 assert(radioWebRouteId("/arbitrary-private-name")==0);
 server.invoke("/api/radio/crash/clear");assert(server.status==403 && clearCalls==0);
 server.headers["X-Radiohead-Config"]="1";
 server.args["confirm"]="CLEAR";server.args["id"]="1234567z";
 server.invoke("/api/radio/crash/clear");assert(server.status==400 && clearCalls==0);
 server.args["id"]="abcd1234";busy=true;
 server.invoke("/api/radio/crash/clear");assert(server.status==409 && clearCalls==0 && reserves==0);
 busy=false;webRestartScheduled=true;
 server.invoke("/api/radio/crash/clear");assert(clearCalls==0);
 webRestartScheduled=false;server.args["id"]="00000000";
 server.invoke("/api/radio/crash/clear");assert(server.status==409 && reserves==0);
 server.args["id"]="abcd1234";eraseSuccess=false;
 server.invoke("/api/radio/crash/clear");assert(server.status==409 && reserves==0);
 eraseSuccess=true;server.invoke("/api/radio/crash/clear");assert(server.status==200 && reserves==0);
#ifdef RADIOHEAD_CRASH_TEST
 server.routes.at("/api/radio/crash/test-partition1")();assert(server.status==403);
 server.headers["X-Radiohead-Backup"]="1";
 server.routes.at("/api/radio/crash/test-partition1")();assert(server.status==200 && reserves==0);
 server.args["confirm"]="NO";server.invoke("/api/radio/crash/test");assert(server.status==403 && crashes==0);
 server.args["confirm"]="CRASH";empty=false;
 server.invoke("/api/radio/crash/test");assert(server.status==409 && crashes==0);
 empty=true;
 try{server.invoke("/api/radio/crash/test");assert(false);}catch(int){assert(crashes==1 && server.status==202);}
#else
 assert(!server.routes.count("/api/radio/crash/test3"));
 assert(!server.routes.count("/api/radio/crash/test-partition1"));
#endif
}
'''.replace('ROUTES', routes)
(out / 'check.cpp').write_text(fixture)
for variant in ('normal', 'crash-test'):
    command = ['c++', '-std=c++17', '-fsanitize=address,undefined', '-I', str(ROOT / 'include')]
    if variant == 'crash-test':
        command.append('-DRADIOHEAD_CRASH_TEST=1')
    subprocess.run(command + [str(out / 'check.cpp'), '-o', str(out / variant)], check=True)
    subprocess.run([str(out / variant)], check=True)
print('PASS: production/test route guards, malformed/stale clear, maintenance exclusion/release and no production panic route')
