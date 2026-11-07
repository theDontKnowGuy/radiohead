#!/usr/bin/env python3
"""Exercise patched token refresh and queue recovery with injected HTTP failures."""
from pathlib import Path
import subprocess
import tempfile

from ensure_spotify_patches import PATCH, ROOT, ensure_token_refresh_patch

CANDIDATE = ROOT / '.pio/spotify-candidate'
CSPOT = CANDIDATE / 'lib/cspot/cspot'


def run(*args):
    subprocess.run([str(arg) for arg in args], check=True, capture_output=True)


with tempfile.TemporaryDirectory(prefix='radiohead-token-') as temporary:
    out = Path(temporary)
    # Reproduce preparation from the pinned cspot revision and every ordered patch.
    repository = out / 'candidate/lib/cspot'
    repository.mkdir(parents=True)
    run('git', 'clone', '--shared', '--no-checkout', CANDIDATE / 'lib/cspot', repository)
    run('git', '-C', repository, 'checkout', '--detach', '37b650a526625773a5a0e0a90b57527f914e7641')
    for name in ('cspot-radiohead', 'wifi-recovery-cspot', 'wifi-recovery-session',
                 's2-activation-cspot', 's2-suspend-cspot', 's1-stream-open-cspot'):
        run('git', '-C', repository, 'apply', PATCH.parent / (name + '.patch'))
    ensure_token_refresh_patch(out / 'candidate')
    before = (repository / 'cspot/src/TrackQueue.cpp').read_bytes()
    ensure_token_refresh_patch(out / 'candidate')
    assert before == (repository / 'cspot/src/TrackQueue.cpp').read_bytes()
    # A conflicting local edit must fail without overwriting it.
    queue_path = repository / 'cspot/src/TrackQueue.cpp'
    queue_path.write_text(queue_path.read_text().replace('retrying in 5s', 'local edit'))
    try:
        ensure_token_refresh_patch(out / 'candidate')
    except RuntimeError:
        assert 'local edit' in queue_path.read_text()
    else:
        raise AssertionError('Conflicting candidate accepted')

    # Compile production fetcher source/header and the production queue function.
    (out / 'AccessKeyFetcher.cpp').write_text((CSPOT / 'src/AccessKeyFetcher.cpp').read_text())
    (out / 'AccessKeyFetcher.h').write_text((CSPOT / 'include/AccessKeyFetcher.h').read_text())
    for name in ('BellLogger.h', 'MercurySession.h', 'Utils.h'):
        (out / name).write_text('#pragma once\n')
    (out / 'Logger.h').write_text('#pragma once\n#define CSPOT_LOG(...) ((void)0)\n')
    (out / 'TimeProvider.h').write_text('''#pragma once
struct TestClock {
  long long now = 100000;
  long long getSyncedTimestamp() { return now; }
};
''')
    (out / 'CSpotContext.h').write_text('''#pragma once
#include <memory>
#include <string>
#include "TimeProvider.h"
namespace cspot {
struct Context {
  struct { std::string clientId = "fixture-id", clientSecret = "fixture-secret"; } config;
  std::shared_ptr<TestClock> timeProvider = std::make_shared<TestClock>();
};
}
''')
    (out / 'HTTPClient.h').write_text('''#pragma once
#include <functional>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>
inline int requests = 0, failures = 0;
inline bool allocationFailure = false;
inline std::function<void()> onRequest;
inline std::string responseBody = R"({"access_token":"fixture-token","expires_in":3600})";
namespace bell {
struct HTTPClient {
  struct Response { std::string body() { return responseBody; } };
  static std::shared_ptr<Response> post(const std::string&,
      const std::vector<std::pair<std::string, std::string>>&,
      const std::vector<unsigned char>&) {
    ++requests;
    if (onRequest) onRequest();
    if (allocationFailure) throw std::bad_alloc();
    if (failures > 0) { --failures; throw std::runtime_error("connection failed"); }
    return std::make_shared<Response>();
  }
};
}
''')
    queue = (CSPOT / 'src/TrackQueue.cpp').read_text().split(
        'void TrackQueue::runTask() {', 1)[1].split('void TrackQueue::stopTask()', 1)[0]
    fixture = r'''
#include <cassert>
#include <deque>
#include <exception>
#include <functional>
#include <memory>
#include <mutex>
#include "AccessKeyFetcher.h"
#include "CSpotContext.h"
#include "HTTPClient.h"
#include "Logger.h"
int sleeps = 0;
std::function<void()> onSleep;
void testSleep(int ms) { assert(ms == 100); ++sleeps; if (onSleep) onSleep(); }
#define BELL_SLEEP_MS(ms) testSleep(ms)
namespace cspot {
struct QueuedTrack {};
struct TestSemaphore { void twait(int ms) { assert(ms == 100); } };
struct TrackQueue {
  bool isRunning = false;
  std::mutex runningMutex, tracksMutex;
  std::deque<std::shared_ptr<QueuedTrack>> preloadedTracks{std::make_shared<QueuedTrack>()};
  std::shared_ptr<TestSemaphore> processSemaphore = std::make_shared<TestSemaphore>();
  std::shared_ptr<AccessKeyFetcher> accessKeyFetcher;
  std::string accessKey;
  int currentTracksIndex = 0, processed = 0;
  void processTrack(std::shared_ptr<QueuedTrack>) {
    assert(accessKey == "fixture-token"); ++processed; isRunning = false;
  }
  void runTask();
};
}
using namespace cspot;
void TrackQueue::runTask() {
''' + queue + r'''
int main() {
  auto ctx = std::make_shared<Context>();
  auto fetcher = std::make_shared<AccessKeyFetcher>(ctx);
  failures = 1;
  try { fetcher->getAccessKey(); assert(false); } catch (const std::runtime_error&) {}
  assert(fetcher->getAccessKey() == "fixture-token" && requests == 2);
  assert(fetcher->getAccessKey() == "fixture-token" && requests == 2);
  ctx->timeProvider->now += 1800001;
  allocationFailure = true;
  try { fetcher->getAccessKey(); assert(false); } catch (const std::bad_alloc&) {}
  allocationFailure = false;
  assert(fetcher->getAccessKey() == "fixture-token" && requests == 4);

  auto noToken = std::make_shared<AccessKeyFetcher>(ctx);
  responseBody = "{}";
  assert(noToken->getAccessKey().empty());
  responseBody = R"({"access_token":"fixture-token","expires_in":3600})";
  assert(noToken->getAccessKey() == "fixture-token");

  auto nested = std::make_shared<AccessKeyFetcher>(ctx);
  int before = requests;
  onRequest = [&] { nested->updateAccessKey(); };
  assert(nested->getAccessKey() == "fixture-token" && requests == before + 1);
  onRequest = {};

  TrackQueue queue;
  queue.accessKeyFetcher = std::make_shared<AccessKeyFetcher>(ctx);
  failures = 2; sleeps = 0; before = requests;
  queue.runTask();
  assert(queue.processed == 1 && requests == before + 3 && sleeps == 100);

  TrackQueue stopping;
  stopping.accessKeyFetcher = std::make_shared<AccessKeyFetcher>(ctx);
  failures = 1; sleeps = 0;
  onSleep = [&] { stopping.isRunning = false; };
  stopping.runTask();
  assert(stopping.processed == 0 && sleeps == 1);
}
'''
    (out / 'fixture.cpp').write_text(fixture)
    cjson = CSPOT / 'bell/external/cJSON'
    run('clang', '-fsanitize=address,undefined', '-c', cjson / 'cJSON.c', '-o', out / 'cJSON.o')
    run('clang++', '-std=c++17', '-Wall', '-Wextra', '-Werror', '-DBELL_ONLY_CJSON',
        '-fsanitize=address,undefined', '-I' + str(out), '-I' + str(cjson),
        out / 'fixture.cpp', out / 'AccessKeyFetcher.cpp', out / 'cJSON.o', '-o', out / 'check')
    run(out / 'check')
print('Spotify token refresh: failure/retry, cached token, expiry, pending reset, '
      'retry delay, stop response, fresh patch order, idempotency and conflicts passed')
