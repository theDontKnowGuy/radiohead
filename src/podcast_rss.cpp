#include "podcast_rss.h"

#include <algorithm>
#include <atomic>
#include <cstdlib>
#include <cstring>
#include <cstdio>

namespace {
// Expat can retain incomplete XML tokens. Bound its aggregate allocations as
// well as the whole response; a huge attribute must not exhaust radio memory.
std::atomic<size_t> allocated{0};
constexpr size_t kParserMemory = 32768;
union Allocation { size_t size; max_align_t alignment; };
void* xmlAllocate(size_t size) {
    size_t current = allocated.load();
    do { if (size > kParserMemory || current > kParserMemory - size) return nullptr; }
    while (!allocated.compare_exchange_weak(current, current + size));
    auto* block = static_cast<Allocation*>(std::malloc(sizeof(Allocation) + size));
    if (!block) { allocated.fetch_sub(size); return nullptr; }
    block->size = size;
    return block + 1;
}
void xmlFree(void* pointer) {
    if (!pointer) return;
    auto* block = static_cast<Allocation*>(pointer) - 1;
    allocated.fetch_sub(block->size);
    std::free(block);
}
void* xmlResize(void* pointer, size_t size) {
    if (!pointer) return xmlAllocate(size);
    if (!size) { xmlFree(pointer); return nullptr; }
    void* next = xmlAllocate(size);
    if (!next) return nullptr;
    std::memcpy(next, pointer, std::min(size, (static_cast<Allocation*>(pointer) - 1)->size));
    xmlFree(pointer);
    return next;
}
void append(char* destination, size_t capacity, const char* data, size_t size) {
    const size_t used = std::strlen(destination);
    const size_t copied = std::min(size, capacity - used - 1);
    std::memcpy(destination + used, data, copied);
    destination[used + copied] = 0;
}
void trimUtf8(char* text) {
    size_t size = std::strlen(text);
    // Remove a cut UTF-8 sequence rather than leaving an invalid final glyph.
    if (size) {
        size_t start = size - 1;
        while (start && (static_cast<unsigned char>(text[start]) & 0xc0) == 0x80) --start;
        const unsigned char c = text[start];
        const size_t expected = c < 0x80 ? 1 : c < 0xe0 ? 2 : c < 0xf0 ? 3 : 4;
        if (size - start < expected) text[size = start] = 0;
    }
    while (size && static_cast<unsigned char>(text[size - 1]) <= 32) text[--size] = 0;
    size_t start = 0;
    while (text[start] && static_cast<unsigned char>(text[start]) <= 32) ++start;
    if (start) std::memmove(text, text + start, size - start + 1);
}
bool audioUrlValid(const char* url) {
    const size_t start = !std::strncmp(url, "https://", 8) ? 8 : !std::strncmp(url, "http://", 7) ? 7 : 0;
    if (!start || !url[start] || url[start] == '/') return false;
    for (const char* p = url; *p; ++p) if (static_cast<unsigned char>(*p) <= 32 || *p == '@' || *p == '\\') return false;
    return true;
}
uint32_t durationSeconds(const char* value) {
    // iTunes duration: integer seconds, MM:SS or HH:MM:SS.
    uint32_t total = 0, part = 0;
    unsigned groups = 0, digits = 0;
    for (const char* p = value;; ++p) {
        if (*p >= '0' && *p <= '9') {
            if (++digits > 7) return 0;
            part = part * 10 + (*p - '0');
        } else if (*p == ':' || *p == 0) {
            if (!digits || (groups && part >= 60) || part > 604800 || total > (604800 - part) / 60) return 0;
            total = total * 60 + part;
            if (!*p) return total;
            if (++groups > 2) return 0;
            part = 0; digits = 0;
        } else return 0;
    }
}
}

PodcastRssParser::PodcastRssParser(PodcastRssEpisode* episodes, size_t capacity)
    : episodes_(episodes), capacity_(capacity) {
    const XML_Memory_Handling_Suite memory{xmlAllocate, xmlResize, xmlFree};
    parser_ = XML_ParserCreate_MM(nullptr, &memory, "|");
    if (!parser_ || !episodes || !capacity) { failed_ = true; return; }
    XML_SetUserData(parser_, this);
    XML_SetElementHandler(parser_, start, end);
    XML_SetCharacterDataHandler(parser_, text);
    XML_SetStartDoctypeDeclHandler(parser_, doctype);
    XML_SetParamEntityParsing(parser_, XML_PARAM_ENTITY_PARSING_NEVER);
}
PodcastRssParser::~PodcastRssParser() { if (parser_) XML_ParserFree(parser_); }
void PodcastRssParser::fail() { failed_ = true; XML_StopParser(parser_, XML_FALSE); }
void PodcastRssParser::doctype(void* context, const XML_Char*, const XML_Char*, const XML_Char*, int) {
    static_cast<PodcastRssParser*>(context)->fail();
}
bool PodcastRssParser::feed(const char* data, size_t size) {
    if (failed_) return false;
    if (complete_) return true;
    if (size > 1024 * 1024 - bytes_) { fail(); return false; }
    bytes_ += size;
    if (XML_Parse(parser_, data, static_cast<int>(size), XML_FALSE) == XML_STATUS_ERROR && !complete_) failed_ = true;
    return !failed_;
}
bool PodcastRssParser::finish() {
    if (!failed_ && !complete_ && XML_Parse(parser_, "", 0, XML_TRUE) == XML_STATUS_ERROR) failed_ = true;
    return !failed_ && rss_ && channel_ && count_ > 0;
}
void PodcastRssParser::start(void* context, const XML_Char* name, const XML_Char** attrs) {
    auto& self = *static_cast<PodcastRssParser*>(context);
    if (++self.depth_ > 24) { self.fail(); return; }
    if (self.depth_ == 1) {
        if (std::strcmp(name, "rss")) { self.fail(); return; }
        self.rss_ = true;
    } else if (self.depth_ == 2) {
        if (std::strcmp(name, "channel")) { self.fail(); return; }
        self.channel_ = true;
    } else if (self.depth_ == 3 && !std::strcmp(name, "item")) {
        self.current_ = {}; self.guidHash_ = 14695981039346656037ULL;
        self.guidBytes_ = 0; self.duration_[0] = 0;
    } else if (self.depth_ == 4) {
        self.field_ = Field::None;
        if (!std::strcmp(name, "title")) self.field_ = Field::Title;
        else if (!std::strcmp(name, "guid")) self.field_ = Field::Guid;
        else if (!std::strcmp(name, "pubDate")) self.field_ = Field::Date;
        else if (!std::strcmp(name, "http://www.itunes.com/dtds/podcast-1.0.dtd|duration")) self.field_ = Field::Duration;
        else if (!std::strcmp(name, "enclosure")) {
            const char* url = nullptr;
            const char* type = nullptr;
            for (size_t i = 0; attrs[i]; i += 2) {
                if (!std::strcmp(attrs[i], "url")) url = attrs[i + 1];
                if (!std::strcmp(attrs[i], "type")) type = attrs[i + 1];
            }
            if (url && std::strlen(url) < sizeof(self.current_.audioUrl) && audioUrlValid(url) &&
                (!type || !std::strncmp(type, "audio/", 6) || !std::strcmp(type, "application/octet-stream")))
                std::strcpy(self.current_.audioUrl, url);
        }
    }
}
void PodcastRssParser::text(void* context, const XML_Char* text, int size) {
    auto& self = *static_cast<PodcastRssParser*>(context);
    if (self.depth_ != 4) return;
    switch (self.field_) {
    case Field::Title: append(self.current_.title, sizeof(self.current_.title), text, size); break;
    case Field::Date: append(self.current_.published, sizeof(self.current_.published), text, size); break;
    case Field::Duration: append(self.duration_, sizeof(self.duration_), text, size); break;
    case Field::Guid:
        for (int i = 0; i < size; ++i) self.guidHash_ = (self.guidHash_ ^ static_cast<uint8_t>(text[i])) * 1099511628211ULL;
        self.guidBytes_ += size; break;
    case Field::None: break;
    }
}
void PodcastRssParser::end(void* context, const XML_Char* name) {
    auto& self = *static_cast<PodcastRssParser*>(context);
    if (self.depth_ == 3 && !std::strcmp(name, "item")) {
        trimUtf8(self.current_.title); trimUtf8(self.current_.published); trimUtf8(self.duration_);
        if (self.current_.title[0] && self.current_.audioUrl[0]) {
            if (!self.guidBytes_) for (const char* p = self.current_.audioUrl; *p; ++p)
                self.guidHash_ = (self.guidHash_ ^ static_cast<uint8_t>(*p)) * 1099511628211ULL;
            snprintf(self.current_.id, sizeof(self.current_.id), "rss-%016llx", static_cast<unsigned long long>(self.guidHash_));
            self.current_.durationSeconds = durationSeconds(self.duration_);
            bool duplicate = false;
            for (size_t i = 0; i < self.count_; ++i) if (!std::strcmp(self.episodes_[i].id, self.current_.id)) duplicate = true;
            if (!duplicate) self.episodes_[self.count_++] = self.current_;
            if (self.count_ == self.capacity_) {
                self.complete_ = true;
                XML_StopParser(self.parser_, XML_FALSE);
            }
        }
    }
    if (self.depth_ == 4) self.field_ = Field::None;
    --self.depth_;
}
