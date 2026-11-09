#!/usr/bin/env python3
"""Exercise production HTTP/CDN cancellation over real stalled socket pairs.

The TLS record API is a deterministic shim; production TLS policy, socketstream,
HTTP parsing and CDN range/retry code run unchanged under ASan/UBSan. This tests
transport cancellation, not cryptographic interoperability or the physical radio.
"""
from pathlib import Path
import subprocess
import tempfile
from ensure_spotify_patches import ROOT, PATCH, ensure_spotify_patches

CANDIDATE = ROOT / '.pio/spotify-candidate'
CSPOT = CANDIDATE / 'lib/cspot/cspot'
BELL = CSPOT / 'bell'


def run(*args):
    subprocess.run([str(arg) for arg in args], check=True)


def write(out, name, value):
    path = out / name
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(value)


with tempfile.TemporaryDirectory(prefix='radiohead-stream-') as temporary:
    out = Path(temporary)
    repo = out / 'candidate/lib/cspot'
    run('git', 'clone', '--quiet', '--shared', '--no-checkout', CANDIDATE / 'lib/cspot', repo)
    run('git', '-C', repo, 'checkout', '--quiet', '--detach', '37b650a526625773a5a0e0a90b57527f914e7641')
    bell_repo = repo / 'cspot/bell'
    run('git', 'clone', '--quiet', '--shared', '--no-checkout', BELL, bell_repo)
    run('git', '-C', bell_repo, 'checkout', '--quiet', '--detach', 'ead27050f63aba369631ec99ad68322685aff5a6')
    for name in ('cspot-radiohead', 'wifi-recovery-cspot', 'wifi-recovery-session',
                 's2-activation-cspot', 's2-suspend-cspot', 's1-stream-open-cspot'):
        run('git', '-C', repo, 'apply', PATCH.parent / (name + '.patch'))
    for name in ('bell-idf55', 'wifi-recovery-bell'):
        run('git', '-C', bell_repo, 'apply', PATCH.parent / (name + '.patch'))
    ensure_spotify_patches(out / 'candidate')
    watched = [bell_repo / 'main/io/TLSSocket.cpp', repo / 'cspot/src/TrackPlayer.cpp']
    before = [path.read_bytes() for path in watched]
    ensure_spotify_patches(out / 'candidate')
    assert before == [path.read_bytes() for path in watched]
    for path in watched:
        original = path.read_text()
        path.write_text(original.replace('cancelled', 'conflicting_local_edit'))
        try:
            ensure_spotify_patches(out / 'candidate')
        except RuntimeError:
            assert 'conflicting_local_edit' in path.read_text()
        else:
            raise AssertionError('Conflicting patch was accepted')
        path.write_text(original)

    # Compile the freshly reproduced patched production sources, not the
    # developer's mutable .pio candidate.
    prod = repo / 'cspot'
    bell = bell_repo / 'main/io'
    for name in ('BellSocket.h', 'TLSSocket.h', 'SocketStream.h', 'HTTPClient.h',
                 'TCPSocket.h', 'URLParser.h', 'picohttpparser.h'):
        write(out, name, (bell / 'include' / name).read_text())
    for name in ('TLSSocket.cpp', 'SocketStream.cpp', 'HTTPClient.cpp', 'URLParser.cpp', 'picohttpparser.c'):
        write(out, name, (bell / name).read_text())
    for name in ('CDNAudioFile.h',):
        write(out, name, (prod / 'include' / name).read_text())
    write(out, 'CDNAudioFile.cpp', (prod / 'src/CDNAudioFile.cpp').read_text())
    for name in ('BellLogger.h', 'Logger.h'):
        write(out, name, '#pragma once\n#define BELL_LOG(...) ((void)0)\n#define CSPOT_LOG(...) ((void)0)\n')
    for name in ('AccessKeyFetcher.h', 'Packet.h', 'WrappedSemaphore.h'):
        write(out, name, '#pragma once\n')
    for name in ('json.hpp', 'json_fwd.hpp'):
        write(out, 'nlohmann/' + name, '#pragma once\n')
    write(out, 'Crypto.h', '''#pragma once
#include <cstdint>
#include <vector>
class Crypto { public:
void aesCTRXcrypt(const std::vector<uint8_t>&, std::vector<uint8_t>&, uint8_t*, size_t) {}
};
''')
    write(out, 'Utils.h', '''#pragma once
#include <cstdint>
#include <vector>
namespace cspot { inline std::vector<uint8_t> bigNumAdd(std::vector<uint8_t> value, int) { return value; } }
''')
    write(out, 'esp_random.h', '#pragma once\n#include <cstring>\ninline void esp_fill_random(void* out, size_t n) { memset(out, 0, n); }\n')
    write(out, 'esp_log.h', '#pragma once\n#define ESP_LOGI(...) ((void)0)\n#define ESP_LOGW(...) ((void)0)\n')
    write(out, 'freertos/FreeRTOS.h', '''#pragma once
#include <chrono>
#include <thread>
using TickType_t = uint32_t;
#define pdMS_TO_TICKS(ms) (ms)
inline TickType_t xTaskGetTickCount() {
return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
}
inline void vTaskDelay(TickType_t ms) { std::this_thread::sleep_for(std::chrono::milliseconds(ms)); }
''')
    write(out, 'freertos/task.h', '#pragma once\n#include "FreeRTOS.h"\n')
    write(out, 'X509Bundle.h', '#pragma once\nnamespace bell { struct X509Bundle { static bool shouldVerify() { return false; } static void attach(void*) {} }; }\n')
    write(out, 'mbedtls/ssl.h', r'''#pragma once
#include <atomic>
#include <cstdint>
#include <cstddef>
#define MBEDTLS_ERR_SSL_WANT_READ -0x6900
#define MBEDTLS_ERR_SSL_WANT_WRITE -0x6880
#define MBEDTLS_SSL_IS_CLIENT 0
#define MBEDTLS_SSL_TRANSPORT_STREAM 0
#define MBEDTLS_SSL_PRESET_DEFAULT 0
#define MBEDTLS_SSL_VERIFY_REQUIRED 1
#define MBEDTLS_SSL_VERIFY_NONE 0
#define MBEDTLS_SSL_VERSION_TLS1_2 0
struct mbedtls_ssl_context {
void* bio = nullptr;
int (*send)(void*, const unsigned char*, size_t) = nullptr;
int (*recv)(void*, unsigned char*, size_t) = nullptr;
};
struct mbedtls_ssl_config {};
inline std::atomic<bool> handshakeStall{false}, forceError{false}, zeroWrite{false};
inline int psa_crypto_init() { return 0; }
inline void mbedtls_ssl_init(mbedtls_ssl_context*) {}
inline void mbedtls_ssl_free(mbedtls_ssl_context*) {}
inline void mbedtls_ssl_config_init(mbedtls_ssl_config*) {}
inline void mbedtls_ssl_config_free(mbedtls_ssl_config*) {}
inline int mbedtls_ssl_config_defaults(mbedtls_ssl_config*, int, int, int) { return 0; }
inline void mbedtls_ssl_conf_authmode(mbedtls_ssl_config*, int) {}
inline void mbedtls_ssl_conf_max_tls_version(mbedtls_ssl_config*, int) {}
inline void mbedtls_ssl_conf_min_tls_version(mbedtls_ssl_config*, int) {}
inline void mbedtls_ssl_conf_rng(mbedtls_ssl_config*, int(*)(void*,unsigned char*,size_t), void*) {}
inline int mbedtls_ssl_setup(mbedtls_ssl_context*, mbedtls_ssl_config*) { return 0; }
inline int mbedtls_ssl_set_hostname(mbedtls_ssl_context*, const char*) { return 0; }
inline void mbedtls_ssl_set_bio(mbedtls_ssl_context* ssl, void* bio,
int(*send)(void*,const unsigned char*,size_t), int(*recv)(void*,unsigned char*,size_t), void*) {
ssl->bio=bio; ssl->send=send; ssl->recv=recv;
}
inline int mbedtls_ssl_handshake(mbedtls_ssl_context*) { return handshakeStall ? MBEDTLS_ERR_SSL_WANT_READ : 0; }
inline int mbedtls_ssl_read(mbedtls_ssl_context* ssl, unsigned char* buf, size_t size) {
return forceError ? -42 : ssl->recv(ssl->bio,buf,size);
}
inline int mbedtls_ssl_write(mbedtls_ssl_context* ssl, const unsigned char* buf, size_t size) {
return zeroWrite ? 0 : ssl->send(ssl->bio,buf,size);
}
inline size_t mbedtls_ssl_get_bytes_avail(mbedtls_ssl_context*) { return 0; }
''')
    write(out, 'mbedtls/net_sockets.h', r'''#pragma once
#include "ssl.h"
#include <deque>
#include <mutex>
#include <cassert>
#include <cerrno>
#include <fcntl.h>
#include <sys/socket.h>
#include <unistd.h>
#define MBEDTLS_NET_PROTO_TCP 0
struct mbedtls_net_context { int fd = -1; };
inline std::mutex endpointsMutex;
inline std::deque<int> endpoints;
inline std::atomic<int> openConnections{0};
inline void mbedtls_net_init(mbedtls_net_context* ctx) { ctx->fd = -1; }
inline int mbedtls_net_connect(mbedtls_net_context* ctx, const char*, const char*, int) {
std::scoped_lock lock(endpointsMutex); assert(!endpoints.empty());
ctx->fd=endpoints.front(); endpoints.pop_front(); ++openConnections; return 0;
}
inline void mbedtls_net_free(mbedtls_net_context* ctx) {
if(ctx->fd>=0) { ::close(ctx->fd); ctx->fd=-1; --openConnections; }
}
inline int mbedtls_net_set_nonblock(mbedtls_net_context* ctx) { return fcntl(ctx->fd,F_SETFL,O_NONBLOCK); }
inline int mbedtls_net_recv(void* bio, unsigned char* buf, size_t n) {
const int result=::recv(static_cast<mbedtls_net_context*>(bio)->fd,buf,n,0);
return result<0 && (errno==EAGAIN || errno==EWOULDBLOCK) ? MBEDTLS_ERR_SSL_WANT_READ : result;
}
inline int mbedtls_net_send(void* bio, const unsigned char* buf, size_t n) {
const int result=::send(static_cast<mbedtls_net_context*>(bio)->fd,buf,n,0);
return result<0 && (errno==EAGAIN || errno==EWOULDBLOCK) ? MBEDTLS_ERR_SSL_WANT_WRITE : result;
}
''')
    write(out, 'fixture.cpp', r'''
#include <algorithm>
#include <atomic>
#include <cassert>
#include <chrono>
#include <condition_variable>
#include <deque>
#include <functional>
#include <iostream>
#include <memory>
#include <mutex>
#include <regex>
#include <stdexcept>
#include <string>
#include <thread>
#include <utility>
#include <vector>
#include "HTTPClient.h"
#include "TLSSocket.h"
#include "mbedtls/net_sockets.h"
#define private public
#include "CDNAudioFile.h"
#undef private
using Clock=std::chrono::steady_clock;
using namespace std::chrono_literals;
int connection() {
int pair[2]; assert(socketpair(AF_UNIX,SOCK_STREAM,0,pair)==0);
std::scoped_lock lock(endpointsMutex); endpoints.push_back(pair[0]); return pair[1];
}
void sendAll(int fd,const std::string& value) {
size_t at=0; while(at<value.size()) { auto n=::send(fd,value.data()+at,value.size()-at,0); assert(n>0); at+=n; }
}
std::string body(size_t n) { std::string result(n,'\0'); for(size_t i=0;i<n;++i) result[i]=i%251; return result; }
void requestHeaders(int fd) {
std::string request; char ch;
while(request.find("\r\n\r\n")==std::string::npos) { assert(::recv(fd,&ch,1,0)==1); request+=ch; }
}
void response(int fd,size_t bytes) {
sendAll(fd,"HTTP/1.1 206 Partial Content\r\nContent-Length: 14336\r\nContent-Range: bytes 20160-34495/500000\r\n\r\n");
sendAll(fd,body(bytes));
}
void initFile(cspot::CDNAudioFile& file) { file.totalFileSize=500000; file.position=20000; }
int main() {
// Actual nonblocking socket with no peer traffic: finite5s deadline, no unsigned error count.
{
const int peer=connection(); bell::TLSSocket socket; socket.open("fixture.invalid",443);
uint8_t byte; const auto started=Clock::now(); bool threw=false;
try { (void)socket.read(&byte,1); } catch(const std::runtime_error&) { threw=true; }
assert(threw && Clock::now()-started>=4800ms && Clock::now()-started<6500ms);
forceError=true; threw=false;
try { (void)socket.read(&byte,1); } catch(const std::runtime_error&) { threw=true; }
assert(threw); forceError=false;
zeroWrite=true; threw=false;
try { (void)socket.write(&byte,1); } catch(const std::runtime_error&) { threw=true; }
assert(threw); zeroWrite=false; ::close(peer);
socket.close(); threw=false;
try { (void)socket.read(&byte,1); } catch(const std::runtime_error&) { threw=true; }
assert(threw);
}
assert(openConnections==0);
// Socket send buffer is full: WANT_WRITE also yields to cancellation.
{
const int peer=connection(); std::atomic<bool> cancel{false}; bell::TLSSocket socket;
socket.setCancellationHandler([&]{return cancel.load();}); socket.open("fixture.invalid",443);
std::string pending(65536,'x');
while(::send(socket.getFd(),pending.data(),pending.size(),0)>0) {}
std::thread trigger([&]{std::this_thread::sleep_for(30ms);cancel=true;});
const auto started=Clock::now(); bool threw=false; uint8_t byte=0;
try { (void)socket.write(&byte,1); } catch(const std::runtime_error&) { threw=true; }
trigger.join(); assert(threw && Clock::now()-started<400ms); ::close(peer);
}
assert(openConnections==0);
// Handshake cancellation uses the same bounded WANT_READ policy.
{
const int peer=connection(); std::atomic<bool> cancel{false}; bell::TLSSocket socket;
socket.setCancellationHandler([&]{return cancel.load();}); handshakeStall=true;
std::thread trigger([&]{std::this_thread::sleep_for(30ms);cancel=true;});
const auto started=Clock::now(); bool threw=false;
try { socket.open("fixture.invalid",443); } catch(const std::runtime_error&) { threw=true; }
trigger.join(); assert(threw && Clock::now()-started<400ms); handshakeStall=false; ::close(peer);
}
assert(openConnections==0);
// Production CDN range body read: peer sends a partial body then stalls. Reset
// generation cancels its captured context, preserving position and closing TLS.
{
const int peer=connection(); std::atomic<bool> sent{false}; std::atomic<uint32_t> generation{1};
std::thread server([&]{requestHeaders(peer);response(peer,128);sent=true;
char ch; assert(::recv(peer,&ch,1,0)==0); ::close(peer);});
cspot::CDNAudioFile file("https://fixture.invalid/audio",{}); initFile(file);
const uint32_t saved=generation.load(); file.setCancellationHandler([&]{return generation!=saved;});
uint8_t destination[128]={}; size_t received=999;
std::thread decoder([&]{received=file.readBytes(destination,sizeof(destination));});
while(!sent) std::this_thread::sleep_for(1ms);
const auto started=Clock::now(); ++generation;
decoder.join(); server.join();
assert(received==0 && file.getPosition()==20000 && Clock::now()-started<500ms);
assert(!file.httpConnection && openConnections==0);
// Clearing a reset flag cannot resurrect an old stream: its generation stays stale.
assert(file.readBytes(destination,sizeof(destination))==0);
}
// A new track/connection works after cancellation, with correct range offset.
{
const int peer=connection(); std::thread server([&]{requestHeaders(peer);response(peer,14336);::close(peer);});
cspot::CDNAudioFile file("https://fixture.invalid/audio",{}); initFile(file);
uint8_t destination[128]={}; assert(file.readBytes(destination,sizeof(destination))==128);
assert(file.getPosition()==20128);
for(size_t i=0;i<sizeof(destination);++i) assert(destination[i]==(i+7)%251);
server.join();
}
assert(openConnections==0);
// Truncated range is discarded/retried at the same position, never decrypted as complete.
{
const int first=connection(), second=connection();
std::thread bad([&]{requestHeaders(first);response(first,40);::close(first);});
std::thread good([&]{requestHeaders(second);response(second,14336);::close(second);});
cspot::CDNAudioFile file("https://fixture.invalid/audio",{}); initFile(file);
uint8_t destination[128]={}; assert(file.readBytes(destination,sizeof(destination))==128);
for(size_t i=0;i<sizeof(destination);++i) assert(destination[i]==(i+7)%251);
bad.join(); good.join();
}
assert(openConnections==0 && endpoints.empty());
// Explicit close/reopen discards pending output, unread input and stream flags.
{
const int oldPeer=connection(); bell::SocketStream stream;
stream.open("fixture.invalid",443,true); stream << "must-not-be-sent";
sendAll(oldPeer,"old-input"); char ch; stream.get(ch); assert(ch=='o');
stream.setstate(std::ios::badbit|std::ios::eofbit); stream.close();
assert(::recv(oldPeer,&ch,1,0)==0); ::close(oldPeer);
const int peer=connection(); stream.open("fixture.invalid",443,true); assert(stream.good());
sendAll(peer,"new"); char incoming[3]; stream.read(incoming,3);
assert(std::string(incoming,3)=="new"); stream << "fresh"; stream.flush();
char outgoing[5]; assert(::recv(peer,outgoing,5,MSG_WAITALL)==5);
assert(std::string(outgoing,5)=="fresh"); stream.close(); ::close(peer);
}
assert(openConnections==0 && endpoints.empty());
}
''')
    run('clang', '-fsanitize=address,undefined', '-c', out / 'picohttpparser.c', '-o', out / 'pico.o')
    run('clang++', '-std=c++17', '-pthread', '-DBELL_DISABLE_FMT', '-DESP_PLATFORM',
        '-Wno-deprecated-declarations',
        '-fsanitize=address,undefined', '-I' + str(out),
        out / 'fixture.cpp', out / 'TLSSocket.cpp', out / 'SocketStream.cpp',
        out / 'HTTPClient.cpp', out / 'URLParser.cpp', out / 'CDNAudioFile.cpp',
        out / 'pico.o', '-o', out / 'check')
    run(out / 'check')
    # Run the production decoder lifecycle against controlled queue/codec seams.
    # The blocking file seam consults the exact cancellation predicate installed
    # by runTask; transport cancellation itself was exercised above.
    player = (prod / 'src/TrackPlayer.cpp').read_text()
    methods = player.split('void TrackPlayer::suspendForLocal()', 1)[1].split(
        'void TrackPlayer::seekMs(', 1)[0]
    loop = player.split('void TrackPlayer::runTask()', 1)[1].split(
        'size_t TrackPlayer::_vorbisRead(', 1)[0]
    callback = player.split('size_t TrackPlayer::_vorbisRead(', 1)[1].split(
        'size_t TrackPlayer::_vorbisClose()', 1)[0]
    write(out, 'decoder.cpp', r'''
#include <atomic>
#include <cassert>
#include <chrono>
#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>
using namespace std::chrono_literals;
using Clock=std::chrono::steady_clock;
using TickType_t=uint32_t;
uint32_t xTaskGetTickCount() {
return std::chrono::duration_cast<std::chrono::milliseconds>(Clock::now().time_since_epoch()).count();
}
void sleepMs(int ms) { std::this_thread::sleep_for(std::chrono::milliseconds(ms)); }
#define ESP_PLATFORM 1
#define pdMS_TO_TICKS(ms) (ms)
#define BELL_SLEEP_MS(ms) sleepMs(ms)
#define ESP_LOGI(...) ((void)0)
#define ESP_LOGW(...) ((void)0)
#define CSPOT_LOG(...) ((void)0)
std::atomic<int> streams{0}, opens{0}, reads{0}, clears{0}, eofEvents{0};
std::atomic<bool> blockedOpen{false}, throwRead{false};
std::atomic<int> openResult{0};
std::function<bool()> oldPredicate;
std::mutex predicateMutex;
struct Semaphore {
std::atomic<int> waits{0};
void twait(int ms) { ++waits; sleepMs(ms); }
};
struct File {
std::function<bool()> cancelled;
File() { ++streams; }
~File() { --streams; }
void setCancellationHandler(std::function<bool()> value) {
cancelled=std::move(value);
std::scoped_lock lock(predicateMutex); if(!oldPredicate) oldPredicate=cancelled;
}
void openStream() {
++opens;
if(blockedOpen) { while(!cancelled()) sleepMs(1); throw std::runtime_error("cancelled open"); }
}
size_t readBytes(uint8_t*,size_t) {
++reads;
if(throwRead) throw std::runtime_error("injected read failure");
while(!cancelled()) sleepMs(1);
return 0;
}
};
struct QueuedTrack {
enum class State { PENDING, READY };
State state=State::READY;
std::shared_ptr<Semaphore> loadedSemaphore=std::make_shared<Semaphore>();
std::string identifier="fixture";
uint32_t requestedPosition=0;
bool loading=false;
std::shared_ptr<File> getAudioFile() { return std::make_shared<File>(); }
};
struct Queue {
std::shared_ptr<QueuedTrack> track=std::make_shared<QueuedTrack>();
std::shared_ptr<Semaphore> playableSemaphore=std::make_shared<Semaphore>();
bool hasTracks() { return true; }
bool isFinished() { return false; }
std::shared_ptr<QueuedTrack> consumeTrack(std::shared_ptr<QueuedTrack>,int& offset) { offset=0; return track; }
};
class TrackPlayer {
public:
std::atomic<bool> isRunning{true}, pendingReset{false}, currentSongPlaying{false};
std::atomic<bool> localSuspended{false}, localSuspendAck{false}, startPaused{false}, inFuture{false};
std::atomic<uint32_t> resetGeneration{0};
std::atomic<size_t> pendingSeekPositionMs{0};
std::mutex dataOutMutex, runningMutex, playbackMutex;
std::shared_ptr<Queue> trackQueue=std::make_shared<Queue>();
std::shared_ptr<File> currentTrackStream;
std::function<void()> eofCallback=[] { ++eofEvents; };
std::function<void(std::shared_ptr<QueuedTrack>,bool)> trackLoaded=[](auto,bool) {};
std::function<size_t(uint8_t*,size_t,std::string)> dataCallback=nullptr;
std::vector<uint8_t> pcmBuffer=std::vector<uint8_t>(1024);
int vorbisFile=0, vorbisCallbacks=0, currentSection=0;
void runTask();
void resetState(bool paused=false);
void suspendForLocal(); void resumeForSpotify(); bool localSuspendAcknowledged() const;
size_t _vorbisRead(void*,size_t,size_t);
};
int ov_open_callbacks(TrackPlayer*,int*,void*,int,int) { return openResult; }
void ov_clear(int*) { ++clears; }
#define VORBIS_SEEK(file,position) ((void)0)
#define VORBIS_READ(file,buffer,length,section) static_cast<long>(this->_vorbisRead(buffer,1,length))
void TrackPlayer::suspendForLocal()''' + methods + r'''
void TrackPlayer::runTask()''' + loop + r'''
size_t TrackPlayer::_vorbisRead(''' + callback + r'''
template<class Condition> void await(Condition condition) {
const auto deadline=Clock::now()+2s;
while(!condition()) { assert(Clock::now()<deadline); sleepMs(1); }
}
void stop(TrackPlayer& player,std::thread& thread) {
player.isRunning=false; player.resetState(); thread.join(); assert(streams==0);
std::scoped_lock lock(predicateMutex); oldPredicate={};
}
int main() {
// Metadata can be missing indefinitely; suspension still acknowledges below1s.
{
TrackPlayer player; player.trackQueue->track->state=QueuedTrack::State::PENDING;
std::thread thread([&]{player.runTask();});
await([&]{return player.trackQueue->track->loadedSemaphore->waits>0;});
const auto started=Clock::now(); player.suspendForLocal();
await([&]{return player.localSuspendAcknowledged();});
assert(Clock::now()-started<400ms && streams==0 && opens==0); stop(player,thread);
}
// A change-song reset cancels the old read even after pendingReset is cleared
// for the replacement track; then local handoff frees its stream before ACK.
{
TrackPlayer player; std::thread thread([&]{player.runTask();});
await([&]{return reads>0;}); player.resetState();
await([&]{return opens>=2;});
{ std::scoped_lock lock(predicateMutex); assert(oldPredicate && oldPredicate()); }
const auto started=Clock::now(); player.suspendForLocal();
await([&]{return player.localSuspendAcknowledged();});
assert(Clock::now()-started<400ms && streams==0 && eofEvents==0); stop(player,thread);
}
// Cancellation during stream setup skips the retry sleep and frees context.
{
blockedOpen=true; const int before=opens;
TrackPlayer player; std::thread thread([&]{player.runTask();}); await([&]{return opens>before;});
const auto started=Clock::now(); player.suspendForLocal();
await([&]{return player.localSuspendAcknowledged();});
assert(Clock::now()-started<400ms && streams==0); stop(player,thread); blockedOpen=false;
}
// A failed Vorbis open never enters ov_read; an unexpected read exception is
// converted to EOF at the C callback boundary without terminating the worker.
{
openResult=-1; const int before=opens, readBefore=reads;
TrackPlayer player; std::thread thread([&]{player.runTask();}); await([&]{return opens>before;});
player.suspendForLocal(); await([&]{return player.localSuspendAcknowledged();});
assert(reads==readBefore && streams==0); stop(player,thread); openResult=0;
}
{
TrackPlayer player; player.currentTrackStream=std::make_shared<File>();
throwRead=true; player.currentSongPlaying=true; uint8_t bytes[8];
assert(player._vorbisRead(bytes,1,sizeof(bytes))==0 && !player.currentSongPlaying);
assert(player._vorbisRead(bytes,SIZE_MAX,2)==0); throwRead=false;
player.currentTrackStream=nullptr;
}
assert(streams==0);
}
''')
    run('clang++', '-std=c++17', '-pthread', '-fsanitize=address,undefined',
        out / 'decoder.cpp', '-o', out / 'decoder')
    run(out / 'decoder')
print('Spotify stream IO: finite stalled read, handshake cancellation, partial-range reset/close, '
      'fresh track, short-read retry, signed errors, stream reopen, metadata suspension, '
      'reset generations, codec unwind, patch order/idempotency/conflicts passed')
