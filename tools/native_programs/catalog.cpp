#include "podcast_catalog.h"
#include "Preferences.h"
#include <cassert>
#include <cstdio>
#include <string>
#include "defaults.inc"
uint16_t podcastShowFavoriteMask=0;
int main() {
    PodcastShow defaults[PODCAST_SHOW_COUNT];
    for(int slot=0;slot<PODCAST_SHOW_COUNT;++slot)defaults[slot]=podcastShows[slot];
    podcastShowFavoriteMask=(1U<<1)|(1U<<8);loadPodcastCatalog();
    assert(savedPodcastShowCount()==10&&podcastShowFavoriteMask==258&&fakeNvs::writes==0);
    assert(podcastShows[1].favoriteId=="ilanadayan/podcast");
    assert(savePodcastCatalog());const auto initial=fakeNvs::values;
    const auto revision=podcastCatalogRevision();
    PodcastShow renamed=podcastShows[1];renamed.webName="אילנה דיין edited";renamed.tftName=renamed.webName;
    assert(savePodcastShow(1,renamed,true));assert(podcastShows[1].favoriteId=="ilanadayan/podcast");assert(podcastCatalogRevision()!=revision);
    const auto renamedStore=fakeNvs::values;const auto renamedId=podcastShows[1].favoriteId;
    PodcastShow draft;draft.webName="New show";draft.tftName=draft.webName;draft.feedUrl="https://example.org/podcast.rss";
    fakeNvs::failWrite=true;assert(!savePodcastShow(1,draft,false));assert(podcastShows[1].favoriteId==renamedId&&podcastShowFavoriteMask==258&&fakeNvs::values==renamedStore);
    assert(!removePodcastShow(1));assert(isSavedPodcastShow(1));fakeNvs::failWrite=false;
    assert(removePodcastShow(1));assert(!isSavedPodcastShow(1)&&savedPodcastShowCount()==9&&podcastShowFavoriteMask==256);
    assert(savePodcastShow(1,draft,false));const auto newId=podcastShows[1].favoriteId;assert(newId!=renamedId&&podcastShowFavoriteMask==256);
    assert(!savePodcastShow(2,draft,false));assert(!savePodcastShow(-1,draft,false));assert(!savePodcastShow(10,draft,false));
    for(auto& show:podcastShows)show={};podcastShowFavoriteMask=0;loadPodcastCatalog();
    assert(savedPodcastShowCount()==10&&podcastShows[1].favoriteId==newId&&podcastShowFavoriteMask==256);
    assert(podcastShows[1].feedUrl==draft.feedUrl);
    assert(validPodcastSourceUrl("http://example.org/feed"));
    for(const char* value:{"https://","ftp://host/feed","https:///feed","http://?query","http://:80/","https://user:secret@host/feed","https://host/a b","https://host/\\a","https://host/a\n"})assert(!validPodcastSourceUrl(value));
    for(const char* slug:{"../unsafe","a/b","a?x=1","a b","","שלום"}){auto bad=defaults[0];bad.program=slug;assert(!validPodcastShow(bad));}
    auto bad=draft;bad.webName="";assert(!validPodcastShow(bad));bad=draft;bad.webName=std::string(161,'a');assert(!validPodcastShow(bad));bad=draft;bad.playlist="podcast";assert(!validPodcastShow(bad));
    for(int slot=0;slot<PODCAST_SHOW_COUNT;++slot)assert(removePodcastShow(slot));assert(savedPodcastShowCount()==0);
    loadPodcastCatalog();assert(savedPodcastShowCount()==0); // An intentionally empty catalog must not reseed defaults.
    fakeNvs::values=initial;loadPodcastCatalog();assert(savedPodcastShowCount()==10&&podcastShowFavoriteMask==258);
    fakeNvs::failOpen=true;assert(!savePodcastShow(0,draft,true));fakeNvs::failOpen=false;
    const auto valid=fakeNvs::values["radio/programs"];
    fakeNvs::values["radio/programs"]=std::vector<char>{'{'};loadPodcastCatalog();assert(savedPodcastShowCount()==10);assert(!savePodcastShow(0,draft,true));
    fakeNvs::values["radio/programs"]=valid;loadPodcastCatalog();assert(savePodcastCatalog());
    fakeNvs::values["radio/programs"]=std::vector<char>(20000,'x');loadPodcastCatalog();assert(!savePodcastCatalog());
    fakeNvs::values["radio/programs"]=valid;loadPodcastCatalog();
    assert(removePodcastShow(0));assert(savePodcastShow(0,draft,true));assert(podcastShowFavoriteMask&1U);
    const auto committed=fakeNvs::values;loadPodcastCatalog();assert(podcastShowFavoriteMask&1U);assert(fakeNvs::values==committed);
    puts("Production program catalog: migration/identity/favorites/empty/reboot/duplicates/validation/NVS failures/corruption checks passed.");
}
