#include "podcast_catalog.h"
#include "media.h"
#include <cassert>
#include <cstdio>
#include <new>
#include "defaults.inc"
struct PodcastEpisode {String id,title,publishedUtc,audioUrl;uint32_t durationSeconds=0;};
PodcastEpisode podcastEpisodes[MAX_EPISODES];int podcastEpisodeCount=0,loadedPodcastShow=-1;bool forceRedraw=false;
bool isSavedPodcastShow(int slot){return slot>=0&&slot<PODCAST_SHOW_COUNT&&!podcastShows[slot].favoriteId.isEmpty();}
bool validPodcastShow(const PodcastShow& draft){return !draft.webName.isEmpty()&&(!draft.feedUrl.isEmpty()||!draft.program.isEmpty());}
struct {template<typename...Args>void printf(const char*,Args...) {}} Serial;
struct {unsigned getFreeHeap(){return 200000;}} ESP;
unsigned long millis(){return 100;}
#define portMUX_TYPE int
#define portMUX_INITIALIZER_UNLOCKED 0
#define portENTER_CRITICAL(mux) (void)(mux)
#define portEXIT_CRITICAL(mux) (void)(mux)
constexpr int pdPASS=1;
void podcastFetchWorker(void*){}
void* pendingTask=nullptr;bool failTask=false;
int xTaskCreate(void(*)(void*),const char*,int,void* parameter,int,void*){if(failTask)return 0;pendingTask=parameter;return pdPASS;}
#include "queue.inc"
void finish(const char* title="Episode",bool success=true){
 auto* request=static_cast<PodcastFetchRequest*>(pendingTask);assert(request);
 fetchedPodcastShow=request->showIndex;fetchedPodcastGeneration=request->generation;
 fetchedPodcastEpisodeCount=1;fetchedPodcastEpisodes[0].title=title;podcastFetchSucceeded=success;
 delete request;pendingTask=nullptr;podcastFetchResultReady=true;mediaTick(100);
}
int main(){
 assert(requestPodcastEpisodes(0));auto* first=static_cast<PodcastFetchRequest*>(pendingTask);
 const auto oldId=first->source.favoriteId;podcastShows[0].favoriteId="replacement";podcastShows[0].program="different";
 assert(first->source.favoriteId==oldId&&first->source.program=="arba");
 podcastCatalogChanged(0);finish("Stale");assert(podcastState==PodcastLoadState::Idle&&podcastEpisodeCount==0);
 assert(requestPodcastEpisodes(0));assert(requestPodcastEpisodes(1));
 first=static_cast<PodcastFetchRequest*>(pendingTask);assert(first->showIndex==0);
 finish("Old request");assert(podcastState==PodcastLoadState::Loading);assert(static_cast<PodcastFetchRequest*>(pendingTask)->showIndex==1);
 finish("Current request");assert(podcastEpisodesReadyFor(1)&&podcastEpisodes[0].title=="Current request");
 const auto generation=podcastRequestToken();assert(requestPodcastEpisodes(1));assert(podcastRequestToken()==generation&&pendingTask==nullptr);
 PodcastShow draft;draft.webName="Preview draft";draft.feedUrl="https://example.org/feed";
 const auto previewToken=requestPodcastPreview(draft);draft.feedUrl="https://example.org/changed";
 assert(previewToken&&static_cast<PodcastFetchRequest*>(pendingTask)->source.feedUrl=="https://example.org/feed");
 assert(requestPodcastEpisodes(2));finish("Preview superseded");assert(podcastState==PodcastLoadState::Loading);finish("TFT request");assert(podcastEpisodesReadyFor(2));
 requestPodcastPreview(draft);finish("Preview");assert(podcastEpisodesReadyFor(-2)&&!podcastEpisodesReadyFor(2));
 podcastShows[2]={};podcastCatalogChanged(2);assert(!requestPodcastEpisodes(2));assert(!requestPodcastEpisodes(-1));assert(!requestPodcastEpisodes(10));
 failTask=true;assert(requestPodcastEpisodes(3));assert(podcastState==PodcastLoadState::Failed&&!podcastFetchBusy&&pendingTask==nullptr);failTask=false;
 assert(requestPodcastEpisodes(3));finish("Network failure",false);assert(podcastState==PodcastLoadState::Failed&&loadedPodcastShow==-1&&podcastEpisodeCount==0);
 puts("Production podcast request coordinator: copied sources/stale-result rejection/queued replacement/preview tokens/cache/deletion/task failures passed.");
}
