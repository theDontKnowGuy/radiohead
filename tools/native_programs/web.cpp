#include "podcast_catalog.h"
#include "Preferences.h"
#include <ArduinoJson.h>
#include <cassert>
#include <cerrno>
#include <cstdlib>
#include <functional>
#include <map>
#include <cstdio>
#include "defaults.inc"
uint16_t podcastShowFavoriteMask=0;
bool forceRedraw=false;
struct PodcastEpisode { String title,publishedUtc;uint32_t durationSeconds=0; };
PodcastEpisode podcastEpisodes[MAX_EPISODES]={{"<Episode>","2026-10-22",200}};
int podcastEpisodeCount=1;
enum class PodcastLoadState { Idle,Loading,Ready,Failed };
PodcastLoadState previewState=PodcastLoadState::Ready;
uint32_t token=23;int requested=-2,changed=-1,uiChanged=-2;
uint32_t requestPodcastPreview(const PodcastShow& show) {assert(validPodcastShow(show));requested=-2;return token;}
uint32_t podcastRequestToken(){return token;}
int podcastRequestedShow(){return requested;}
PodcastLoadState podcastLoadState(){return previewState;}
bool podcastEpisodesReadyFor(int slot){return slot==requested&&previewState==PodcastLoadState::Ready;}
void podcastCatalogChanged(int slot){changed=slot;}
void uiControllerProgramsChanged(int slot){uiChanged=slot;}
bool isPodcastShowFavorite(int slot){return isSavedPodcastShow(slot)&&(podcastShowFavoriteMask&(1U<<slot));}
bool togglePodcastShowFavorite(int slot){if(!isSavedPodcastShow(slot))return false;podcastShowFavoriteMask^=1U<<slot;return true;}
constexpr int HTTP_GET=0,HTTP_POST=1;
struct Server {
 std::map<std::string,std::function<void()>> handlers;
 std::map<std::string,String> arguments;
 String body;int status=0;
 void on(const char* path,int method,std::function<void()> handler){handlers[std::to_string(method)+path]=handler;}
 bool hasArg(const char* name){return arguments.count(name);}
 String arg(const char* name){return arguments[name];}
 void send(int code,const char*,const String& text){status=code;body=text;}
 void call(const char* path,int method,std::map<std::string,String> args={}){arguments=args;status=0;handlers.at(std::to_string(method)+path)();assert(status!=0);}
} server;
String jsonEscape(const String& value){JsonDocument doc;doc.set(value);String encoded;serializeJson(doc,encoded);return encoded.substr(1,encoded.size()-2);}
#include "web.inc"
int main(){
 registerProgramApi();
 auto revision=[](){return String(static_cast<int>(podcastCatalogRevision()));};
 server.call("/api/programs",HTTP_GET);assert(server.status==200);JsonDocument doc;assert(!deserializeJson(doc,server.body.c_str()));assert(doc["programs"].size()==10);
 const auto original=podcastShows[0].favoriteId;
 server.call("/api/programs/save",HTTP_POST,{{"revision",revision()},{"slot","0"},{"kind","omny"},{"name","Edited <name>"},{"program","arba"},{"playlist","podcast"},{"favorite","1"}});
 assert(server.status==200&&podcastShows[0].favoriteId==original&&isPodcastShowFavorite(0)&&changed==0&&uiChanged==0);
 server.call("/api/programs/save",HTTP_POST,{{"revision","0"}});assert(server.status==409);
 server.call("/api/programs/save",HTTP_POST,{{"revision",revision()},{"slot","-1"}});assert(server.status==400);
 server.call("/api/programs/save",HTTP_POST,{{"revision",revision()},{"slot","10"}});assert(server.status==400);
 server.call("/api/programs/save",HTTP_POST,{{"revision",revision()},{"slot",""}});assert(server.status==409); // Full.
 server.call("/api/programs/remove",HTTP_POST,{{"revision",revision()},{"slot","0"}});assert(server.status==200&&!isSavedPodcastShow(0)&&!isPodcastShowFavorite(0));
 server.call("/api/programs/save",HTTP_POST,{{"revision",revision()},{"slot",""},{"kind","rss"},{"name","New program"},{"feedUrl","https://example.org/feed"},{"favorite","0"}});assert(server.status==200&&isSavedPodcastShow(0)&&podcastShows[0].favoriteId!=original);
 const auto id=podcastShows[0].favoriteId;
 fakeNvs::failWrite=true;
 server.call("/api/programs/remove",HTTP_POST,{{"revision",revision()},{"slot","0"}});assert(server.status==500&&podcastShows[0].favoriteId==id);
 server.call("/api/programs/favorite",HTTP_POST,{{"revision",revision()},{"slot","0"}});assert(server.status==500&&!isPodcastShowFavorite(0));fakeNvs::failWrite=false;
 server.call("/api/programs/favorite",HTTP_POST,{{"revision",revision()},{"slot","0"}});assert(server.status==200&&isPodcastShowFavorite(0));
 const auto before=fakeNvs::values;
 server.call("/api/programs/preview",HTTP_POST,{{"kind","rss"},{"name","Draft"},{"feedUrl","https://example.org/draft"}});assert(server.status==202&&fakeNvs::values==before);
 server.call("/api/programs/preview",HTTP_GET,{{"token","23"}});assert(server.status==200);assert(!deserializeJson(doc,server.body.c_str()));assert(doc["episodes"].size()==1);
 server.call("/api/programs/preview",HTTP_GET,{{"token","22"}});assert(server.status==409);
 requested=1;server.call("/api/programs/preview",HTTP_GET,{{"token","23"}});assert(server.status==409);
 server.call("/api/programs/preview",HTTP_POST,{{"kind","rss"},{"name","Bad"},{"feedUrl","javascript:alert(1)"}});assert(server.status==400);
 server.call("/api/programs/preview",HTTP_POST,{{"kind","omny"},{"name","Bad"},{"program","../escape"},{"playlist","podcast"}});assert(server.status==400);
 puts("Production Programs HTTP handlers: CRUD/capacity/bounds/revisions/identity/favorites/failure rollback/preview tokens checks passed.");
}
