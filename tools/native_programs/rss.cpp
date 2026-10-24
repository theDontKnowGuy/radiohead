#include "podcast_rss.h"
#include <cassert>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <sstream>
#include <string>

static const std::string prefix = "<?xml version='1.0'?><rss version='2.0' xmlns:itunes='http://www.itunes.com/dtds/podcast-1.0.dtd'><channel><title>Channel title</title>";
static std::string item(const std::string& title="Hello &amp; שלום", const std::string& url="https://audio.example/ep.mp3?a=1&amp;b=2", const std::string& extra="") {
    return "<item><title>"+title+"</title><guid>urn:episode:one</guid><pubDate>Wed, 21 Oct 2026 10:00:00 GMT</pubDate><enclosure type='audio/mpeg' length='100' url='"+url+"'/><itunes:duration>01:02:03</itunes:duration>"+extra+"</item>";
}
static bool parse(const std::string& xml, size_t block=17, size_t expected=1) {
    PodcastRssEpisode episodes[8];
    PodcastRssParser parser(episodes,8);
    for(size_t offset=0;offset<xml.size()&&!parser.complete();offset+=block) {
        if(!parser.feed(xml.data()+offset,std::min(block,xml.size()-offset))) return false;
    }
    if(!parser.finish()) return false;
    assert(parser.count()==expected);
    return true;
}
int main(int argc,char** argv) {
    const auto xml=prefix+item()+"</channel></rss>";
    for(size_t split=1;split<=xml.size();++split) assert(parse(xml,split));
    PodcastRssEpisode episodes[8];
    {PodcastRssParser parser(episodes,8);assert(parser.feed(xml.data(),xml.size()));assert(parser.finish());assert(!std::strcmp(episodes[0].title,"Hello & שלום"));assert(!std::strcmp(episodes[0].audioUrl,"https://audio.example/ep.mp3?a=1&b=2"));assert(episodes[0].durationSeconds==3723);}
    assert(parse(prefix+item("<![CDATA[<News> & שלום]]>")+"</channel></rss>"));
    assert(parse(prefix+item("&#x5e9;&#1500;&#x5d5;&#x5dd;")+"</channel></rss>"));
    assert(parse(prefix+item()+item()+"</channel></rss>")); // Duplicate GUID ignored.
    assert(!parse(prefix+item("Title","javascript:alert(1)")+"</channel></rss>"));
    assert(!parse(prefix+item("Title","https://user:pass@host/audio.mp3")+"</channel></rss>"));
    assert(!parse(prefix+item("Title","https://example/"+std::string(600,'a'))+"</channel></rss>"));
    assert(!parse(prefix+item()+"</channel>"));
    assert(!parse("<html><body>Not a feed</body></html>"));
    assert(!parse("<feed xmlns='http://www.w3.org/2005/Atom'></feed>"));
    assert(!parse("<!DOCTYPE rss [<!ENTITY x 'bad'>]>"+xml));
    assert(!parse("<!DOCTYPE rss SYSTEM 'file:///etc/passwd'>"+xml));
    assert(!parse(prefix+item("&undefined;")+"</channel></rss>"));
    assert(!parse(prefix+"<item><title>"+std::string(30,'<')+"</title></item></channel></rss>"));
    std::string nested=prefix+"<item>";for(int i=0;i<30;++i)nested+="<x>";assert(!parse(nested));
    assert(!parse(prefix+"<item><enclosure url='"+std::string(100000,'a')+"'/></item></channel></rss>",256));
    assert(!parse(prefix+"<!--"+std::string(1024*1024,'a')+"--></channel></rss>",256));
    const std::string longTitle=std::string(159,'a')+"שלום";
    {const auto text=prefix+item(longTitle)+"</channel></rss>";PodcastRssParser p(episodes,8);assert(p.feed(text.data(),text.size()));assert(p.finish());assert(std::strlen(episodes[0].title)==159);}
    auto duration=prefix+item();const auto pos=duration.find("01:02:03");duration.replace(pos,8,"99999999999999999");duration+="</channel></rss>";
    {PodcastRssParser p(episodes,8);assert(p.feed(duration.data(),duration.size()));assert(p.finish());assert(episodes[0].durationSeconds==0);}
    auto fallback=prefix+"<item><title>Without GUID</title><enclosure url='https://audio.example/episode'/></item></channel></rss>";
    assert(parse(fallback));
    std::string many=prefix;for(int i=0;i<20;++i)many+="<item><title>Episode "+std::to_string(i)+"</title><guid>"+std::to_string(i)+"</guid><enclosure url='https://audio.example/"+std::to_string(i)+"'/></item>";many+="</channel></rss>";assert(parse(many,256,8));
    for(int file=1;file<argc;++file) {
        std::ifstream stream(argv[file]);assert(stream.good());std::ostringstream data;data<<stream.rdbuf();
        PodcastRssParser parser(episodes,8);auto feed=data.str();
        for(size_t offset=0;offset<feed.size()&&!parser.complete();offset+=256)assert(parser.feed(feed.data()+offset,std::min(size_t(256),feed.size()-offset)));
        assert(parser.finish());printf("Live RSS fixture: %zu episodes; first: %s\n",parser.count(),episodes[0].title);
    }
    puts("Production RSS parser: split XML/UTF-8/entities/CDATA/enclosures/duration/identity/bounds/DTD/malformed checks passed.");
}
