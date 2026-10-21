"""Exercise production Audio validation and render its C++ page with synthetic state."""
from pathlib import Path
import subprocess

root = Path(__file__).resolve().parents[1]
out = root / '.pio/audio_native'
out.mkdir(parents=True, exist_ok=True)
web = (root / 'src/web_server.cpp').read_text()
media = (root / 'src/media.cpp').read_text()

def function(source, signature):
    start = source.index(signature)
    body = source.index('{', start)
    end, depth = body + 1, 1
    while depth:
        depth += (source[end] == '{') - (source[end] == '}')
        end += 1
    return source[start:end]

fixture = r'''
#include "Arduino.h"
#include "audio_notch.h"
#include <algorithm>
#include <cassert>
#include <cerrno>
#include <cstdlib>
#include <fstream>
#include <map>
int gB=0,gM=0,gT=0,mainVal=7,audioNotchHz=3000,audioNotchEndHz=0;
bool radioMuted=false,audioNotchEnabled=false,forceRedraw=false;
int queued=0;
void queueSettingsSave(){++queued;}
template<class T> T constrain(T x,T low,T high){return std::clamp(x,low,high);}
struct {void setTone(int,int,int){}} audio;
void mediaSetTone(int,int,int){}
struct {
 std::map<std::string,String> args;
 int status=0;
 String response;
 bool hasArg(const char* key){return args.count(key);}
 String arg(const char* key){return args[key];}
 void send(int code,const char*,const String& body){status=code;response=body;}
} server;
void sendPlayerState(int code=200){server.status=code;}
'''
fixture += function(media, 'void mediaSetNotchFilter(')
fixture += function(web, 'bool parseIntegerArg(')
fixture += function(web, 'uint32_t audioSettingsRevision(')
fixture += function(web, 'uint32_t playerRevision(')
route = web[web.index('    server.on("/api/audio",'):]
route = route[route.index('[] {') + 4:route.index('\n    });')]
fixture += '\nvoid applyAudio(){' + route + '\n}\n'
fixture += web[web.index('enum class WebSection'):web.index('enum class WebSection') + web[web.index('enum class WebSection'):].index(';')+1]
for signature in ['const char* sectionPath(', 'const char* sectionDesktopLabel(',
                  'const char* sectionMobileLabel(', 'const char* sectionIcon(']:
    fixture += function(web, signature)
fixture += function(web, 'void appendNavigation(')
start = web.index('    case WebSection::Audio:', web.index('void appendSectionContent'))
end = web.index('    case WebSection::Stations: {', start)
fixture += 'void appendSectionContent(String& html,WebSection section){switch(section){' + web[start:end] + 'default:break;}}\n'
fixture += r'''
enum class MediaSource {None,Radio,Podcast,Spotify};
struct MediaSnapshot {MediaSource source=MediaSource::Radio;String title="Radio fixture",artist,album;};
MediaSnapshot state;
const MediaSnapshot& mediaSnapshot(){return state;}
String localAddress(){return "radio.local";}
String connectionState(){return "Connected";}
String htmlEscape(const String& s){return s;}
String playbackLabel(){return "Now playing";}
String currentPlaybackName(){return "Radio fixture";}
'''
fixture += function(web, 'void handleConfigShell(').replace('media.artist + (media.album.isEmpty() ? \"\" : \" · \" + media.album)', 'String(media.artist + (media.album.isEmpty() ? \"\" : \" · \" + media.album))')
fixture += r'''
int main(int argc,char**argv){
 assert(argc==2);
 auto defaults=[](){server.args={{"bass","-15"},{"mid","0"},{"treble","15"},{"notchEnabled","1"},{"notchHz","3000"},{"revision",String(int(audioSettingsRevision()))}};};
 defaults();applyAudio();assert(server.status==200&&gB==-15&&gT==15&&audioNotchEnabled&&queued==1);
 assert(radioNotchConfiguration.load()==(kNotchEnabledBit|3000));
 // Legacy request without an upper endpoint retains the original single notch.
 assert(audioNotchEndHz==0);
 defaults();server.args["notchHz"]="5000";server.args["notchEndHz"]="5100";applyAudio();
 assert(server.status==200 && audioNotchHz==5000 && audioNotchEndHz==5100);
 assert(radioNotchConfiguration.load()==radioNotchConfigurationValue(true,5000,5100));
 auto rangeRevision=audioSettingsRevision();auto rangePlayer=playerRevision();
 mediaSetNotchFilter(true,5000,5200);
 assert(audioSettingsRevision()!=rangeRevision && playerRevision()!=rangePlayer);
 for(auto value:{"0.1","-1","99","5000","4999","10001","5100Hz","","99999999999999999"}){
  defaults();server.args["notchHz"]="5000";server.args["notchEndHz"]=value;
  auto before=audioSettingsRevision();int saves=queued;applyAudio();
  assert(server.status==400 && audioSettingsRevision()==before && queued==saves);
 }
 // Persistence/default normalization keeps old single-frequency behavior.
 mediaSetNotchFilter(true,3000,0);assert(audioNotchEndHz==0);
 mediaSetNotchFilter(true,3000,2000);assert(audioNotchEndHz==0);
 mediaSetNotchFilter(true,3000,10001);assert(audioNotchEndHz==0);
 mediaSetNotchFilter(true,3000,4000);assert(audioNotchEndHz==4000);
 defaults();applyAudio();assert(audioNotchHz==3000&&audioNotchEndHz==0);
 queued=1;
 const auto before=audioSettingsRevision();mainVal=20;radioMuted=true;assert(audioSettingsRevision()==before);
 for(auto field:{"bass","mid","treble","notchHz","notchEnabled","revision"}){
  defaults();server.args.erase(field);applyAudio();assert(server.status==400&&queued==1);
 }
 for(auto value:{"-16","16","1.5","x","999999999999999999999999999999"}){
  defaults();server.args["bass"]=value;applyAudio();assert(server.status==400&&gB==-15&&queued==1);
 }
 for(auto value:{"99","10001","3000Hz","3000.0","", "999999999999999999999999"}){
  defaults();server.args["notchHz"]=value;applyAudio();assert(server.status==400&&audioNotchHz==3000&&queued==1);
 }
 for(auto value:{"-1","2","true"}){
  defaults();server.args["notchEnabled"]=value;applyAudio();assert(server.status==400&&queued==1);
 }
 defaults();server.args["revision"]="0";applyAudio();assert(server.status==409&&queued==1);
 for(auto frequency:{"100","10000"}){
  defaults();server.args["notchHz"]=frequency;server.args["notchEnabled"]="0";applyAudio();assert(server.status==200&&!audioNotchEnabled);
 }
 audioNotchHz=5000;audioNotchEndHz=5100;mainVal=7;radioMuted=false;gB=gM=gT=0;
 handleConfigShell(WebSection::Audio);
 std::ofstream(argv[1])<<server.response;
 assert(server.response.find("id='audio-form'")!=std::string::npos);
 assert(server.response.find("value='5000-5100'")!=std::string::npos);
 assert(server.response.find("Sound · custom tone")==std::string::npos);
 handleConfigShell(WebSection::Network);
 assert(server.response.find("tone-bass")==std::string::npos);
 puts("Production Audio route rejects invalid/stale input without partial mutation; volume changes do not conflict.");
}
'''
(out / 'routes.cpp').write_text(fixture)
subprocess.run(['c++','-std=c++17','-O1','-fsanitize=address,undefined','-I',str(root/'tools/native_ui'),
                '-I',str(root/'include'),str(out/'routes.cpp'),'-o',str(out/'routes')],check=True)
subprocess.run([str(out/'routes'),str(out/'audio.html')],check=True)
