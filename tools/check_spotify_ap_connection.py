#!/usr/bin/env python3
"""Fault-inject pinned production AP connection code under ASan/UBSan."""
from pathlib import Path
import subprocess
import tempfile
from ensure_spotify_patches import ROOT

CSPOT = ROOT / '.pio/spotify-candidate/lib/cspot'
PATCH = ROOT / 'experiments/spotify-native/patches/ap-connection-recovery-cspot.patch'
with tempfile.TemporaryDirectory(prefix='radiohead-ap-') as directory:
    out = Path(directory)
    subprocess.run(['git', 'clone', '--quiet', '--shared', '--no-checkout', str(CSPOT), str(out / 'repo')], check=True)
    subprocess.run(['git', '-C', str(out / 'repo'), 'checkout', '--quiet', '--detach', '37b650a526625773a5a0e0a90b57527f914e7641'], check=True)
    subprocess.run(['git', '-C', str(out / 'repo'), 'apply', str(PATCH)], check=True)
    subprocess.run(['git', '-C', str(out / 'repo'), 'apply', '--reverse', '--check', str(PATCH)], check=True)
    source = out / 'repo/cspot/src/PlainConnection.cpp'
    (out / 'PlainConnection.h').write_text((out / 'repo/cspot/include/PlainConnection.h').read_text())
    # Quoted includes search the source directory first. Copy only the target
    # translation unit and provide minimal unrelated serialization seams.
    (out / 'PlainConnection.cpp').write_text(source.read_text())
    (out / 'Logger.h').write_text('#pragma once\n#define CSPOT_LOG(...) ((void)0)\n')
    for header in ('BellLogger.h', 'Packet.h'):
        (out / header).write_text('#pragma once\n')
    (out / 'Utils.h').write_text('''#pragma once
#include <cstdint>
#include <cstring>
#include <vector>
namespace cspot {
template<class T> T extract(const std::vector<uint8_t>& data, size_t position) { T value; memcpy(&value, data.data()+position, sizeof(T)); return value; }
template<class T> std::vector<uint8_t> pack(T value) { std::vector<uint8_t> result(sizeof(T)); memcpy(result.data(), &value, sizeof(T)); return result; }
}
''')
    (out / 'fixture.cpp').write_text(r'''
#include <netdb.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/socket.h>
#include <unistd.h>
#include <cerrno>
#include <cassert>
#include <cstring>
#include <deque>
#include <stdexcept>
#include <string>
#include <vector>
#include <memory>
#include <functional>
int allocatedAddresses=0, sockets=0, nextFd=100, connectAttempts=0;
int resolverResult=0, socketFailures=0;
std::deque<int> connectResults, readResults, writeResults;
std::string written;
int resolver(const char*, const char*, const addrinfo*, addrinfo** result) {
    if (resolverResult) return resolverResult;
    *result=new addrinfo{}; (*result)->ai_family=AF_INET; ++allocatedAddresses;
    (*result)->ai_next=new addrinfo{}; (*result)->ai_next->ai_family=AF_INET; ++allocatedAddresses;
    return 0;
}
void release(addrinfo* result) { while(result) { auto next=result->ai_next; delete result; --allocatedAddresses; result=next; } }
int createSocket(int,int,int) { if(socketFailures) { --socketFailures; return -1; } ++sockets; return nextFd++; }
int connectSocket(int,const sockaddr*,socklen_t) { ++connectAttempts; int result=connectResults.empty()?0:connectResults.front(); if(!connectResults.empty()) connectResults.pop_front(); return result; }
int closeSocket(int) { assert(sockets>0); --sockets; return 0; }
int shutdownSocket(int,int) { return 0; }
int option(int,int,int,const void*,socklen_t) { return 0; }
ssize_t readSocket(int,void* buf,size_t size,int) {
    assert(!readResults.empty()); int result=readResults.front(); readResults.pop_front();
    if(result<0) { errno=-result; return -1; }
    assert(static_cast<size_t>(result)<=size); memset(buf,'a',result); return result;
}
ssize_t writeSocket(int,const void* buf,size_t size,int) {
    assert(!writeResults.empty()); int result=writeResults.front(); writeResults.pop_front();
    if(result<0) { errno=-result; return -1; }
    assert(static_cast<size_t>(result)<=size); written.append(static_cast<const char*>(buf),result); return result;
}
#define getaddrinfo resolver
#define freeaddrinfo release
#define socket createSocket
#define connect connectSocket
#define close closeSocket
#define shutdown shutdownSocket
#define setsockopt option
#define recv readSocket
#define send writeSocket
// connect/close are also member names: macros rename declarations consistently.
#include "PlainConnection.cpp"
#undef connect
#undef close
template<class F> void fails(F operation) { bool failed=false; try { operation(); } catch(const std::runtime_error&) { failed=true; } assert(failed); }
int main() {
    cspot::PlainConnection conn; conn.timeoutHandler=[] { return true; };
    resolverResult=EAI_FAIL;
    fails([&] { conn.connectSocket("fixture.invalid:4070"); });
    assert(allocatedAddresses==0 && sockets==0 && connectAttempts==0);
    resolverResult=0;
    for(int retry=0;retry<100;++retry) {
        connectResults={-1,-1}; fails([&] { conn.connectSocket("fixture.invalid:4070"); });
        assert(allocatedAddresses==0 && sockets==0);
    }
    connectResults={-1,0}; conn.connectSocket("fixture.invalid:4070");
    assert(allocatedAddresses==0 && sockets==1);
    // Reopening releases the previous successful socket.
    conn.connectSocket("fixture.invalid:4070"); assert(sockets==1);
    std::vector<uint8_t> data(4);
    readResults={-EINTR,2,-EINTR,2}; conn.readBlock(data.data(),data.size());
    assert(std::string(data.begin(),data.end())=="aaaa" && readResults.empty());
    data={'a','b','c','d'}; writeResults={-EINTR,2,-EINTR,2}; conn.writeBlock(data);
    assert(written=="abcd" && writeResults.empty());
    // EOF is terminal even when errno still contains an unrelated retry code.
    errno=EAGAIN; readResults={0}; fails([&] { conn.readBlock(data.data(),4); });
    errno=EAGAIN; writeResults={0}; fails([&] { conn.writeBlock(data); });
    readResults={-EAGAIN}; fails([&] { conn.readBlock(data.data(),4); });
    conn.closeSocket(); assert(sockets==0);
    socketFailures=2; fails([&] { conn.connectSocket("fixture.invalid:4070"); });
    assert(sockets==0 && allocatedAddresses==0);
}
''')
    subprocess.run(['clang++', '-std=c++17', '-g', '-O1', '-fsanitize=address,undefined', '-fno-omit-frame-pointer', '-I', str(out), str(out / 'fixture.cpp'), '-o', str(out / 'check')], check=True)
    subprocess.run([str(out / 'check')], check=True)
print('Spotify AP resolver/socket recovery checks passed (ASan/UBSan)')
