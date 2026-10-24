#pragma once

#include <cstddef>
#include <cstdint>
#include <expat.h>

// RSS 2.0, streamed in the media worker. No document tree or downloaded body.
// Reject DTDs/entities, excessive nesting, oversized URLs and malformed XML.
struct PodcastRssEpisode {
    char id[49] = {};
    char title[161] = {};
    char published[81] = {};
    char audioUrl[513] = {};
    uint32_t durationSeconds = 0;
};

class PodcastRssParser {
public:
    PodcastRssParser(PodcastRssEpisode* episodes, size_t capacity);
    ~PodcastRssParser();
    PodcastRssParser(const PodcastRssParser&) = delete;
    PodcastRssParser& operator=(const PodcastRssParser&) = delete;
    bool feed(const char* data, size_t size);
    bool finish();
    size_t count() const { return count_; }
    bool complete() const { return complete_; }

private:
    static void start(void*, const XML_Char*, const XML_Char**);
    static void end(void*, const XML_Char*);
    static void text(void*, const XML_Char*, int);
    static void doctype(void*, const XML_Char*, const XML_Char*, const XML_Char*, int);
    void fail();
    XML_Parser parser_ = nullptr;
    PodcastRssEpisode* episodes_;
    size_t capacity_, count_ = 0, bytes_ = 0;
    unsigned depth_ = 0;
    bool failed_ = false, complete_ = false, rss_ = false, channel_ = false;
    enum class Field { None, Title, Guid, Date, Duration } field_ = Field::None;
    PodcastRssEpisode current_;
    uint64_t guidHash_ = 14695981039346656037ULL;
    size_t guidBytes_ = 0;
    char duration_[16] = {};
};
