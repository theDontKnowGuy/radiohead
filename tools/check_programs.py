"""Compile and exercise production catalog persistence and bounded RSS parsing."""
from pathlib import Path
import subprocess
import sys
root = Path(__file__).resolve().parents[1]
out = root / '.pio/programs_native'
out.mkdir(parents=True, exist_ok=True)
common = ['clang++', '-std=c++17', '-Wall', '-Wextra', '-Werror', '-g', '-fsanitize=address,undefined', f'-I{root / "include"}']
subprocess.run(common + [str(root/'src/podcast_rss.cpp'), str(root/'tools/native_programs/rss.cpp'), '-lexpat', '-o', str(out/'rss')], check=True)
subprocess.run([str(out/'rss'), *sys.argv[1:]], check=True)

source = (root / 'src/app_state.cpp').read_text()
start = source.index('PodcastShow podcastShows[')
end = source.index('\n};', start) + 3
(out / 'defaults.inc').write_text(source[start:end])
subprocess.run(['clang++', '-std=c++17', '-Wall', '-Wextra', '-Werror', '-g', '-fsanitize=address,undefined', '-Wno-missing-field-initializers', f'-I{root / "tools/native_programs"}', f'-I{root / "include"}', f'-I{root / ".pio/libdeps/esp32s3/ArduinoJson/src"}', f'-iquote{out}', str(root / 'src/settings_programs.cpp'), str(root / 'tools/native_programs/catalog.cpp'), '-o', str(out / 'catalog')], check=True)
subprocess.run([str(out / 'catalog')], check=True)

web = (root / 'src/web_server.cpp').read_text()
parse = web[web.index('bool parseIntegerArg('):web.index('void sendBadRequest(')]
api = web[web.index('String programsJson()'):web.index('void handlePrograms()')]
(out / 'web.inc').write_text(parse + api)
subprocess.run(['clang++', '-std=c++17', '-Wall', '-Wextra', '-Werror', '-g', '-fsanitize=address,undefined', '-Wno-missing-field-initializers', f'-I{root / "tools/native_programs"}', f'-I{root / "include"}', f'-I{root / ".pio/libdeps/esp32s3/ArduinoJson/src"}', f'-iquote{out}', str(root / 'src/settings_programs.cpp'), str(root / 'tools/native_programs/web.cpp'), '-o', str(out / 'web')], check=True)
subprocess.run([str(out / 'web')], check=True)

media = (root / 'src/media.cpp').read_text()
state = media[media.index('PodcastLoadState podcastState'):media.index('int activePodcastShow')]
request = media[media.index('struct PodcastFetchRequest'):media.index('bool fetchRssEpisodes')]
start = media[media.index('void startPendingPodcastFetch()'):media.index('template <typename LineHandler>')]
consume = media[media.index('void mediaTick('):media.index('    if (podcastState == PodcastLoadState::Loading && !podcastFetchBusy) startPendingPodcastFetch();') + len('    if (podcastState == PodcastLoadState::Loading && !podcastFetchBusy) startPendingPodcastFetch();')] + '\n}\n'
api = media[media.index('bool requestPodcastEpisodes('):media.index('bool playPodcastEpisode(')]
(out / 'queue.inc').write_text(state + request + start + consume + api)
subprocess.run(['clang++', '-std=c++17', '-Wall', '-Wextra', '-Werror', '-Wno-unused-parameter', '-g', '-fsanitize=address,undefined', '-Wno-missing-field-initializers', f'-I{root / "tools/native_programs"}', f'-I{root / "include"}', f'-iquote{out}', str(root / 'tools/native_programs/queue.cpp'), '-o', str(out / 'queue-test')], check=True)
subprocess.run([str(out / 'queue-test')], check=True)
