"""Exercise the production speaker gate with simulated time and playback sources."""
from pathlib import Path
import subprocess
import tempfile

root = Path(__file__).resolve().parents[1]
source = (root / "src/media.cpp").read_text()


def function(signature):
    start = source.index(signature)
    end = source.index("{", start) + 1
    depth = 1
    while depth:
        depth += (source[end] == "{") - (source[end] == "}")
        end += 1
    return source[start:end]


fixture = r"""
#include <cassert>
#include <cstdint>
using gpio_num_t = int;
constexpr int SPEAKER_MUTE = 2;
uint32_t clockMs = 0;
uint32_t millis() { return clockMs; }
int outputLevel = -1;
void gpio_set_level(int pin, int level) { assert(pin == 2); outputLevel = level; }
enum class PlaybackState { Stopped, Connecting, Playing, Failed };
enum class MediaStatus { Stopped, Connecting, Playing, Paused, Failed };
enum class OutputOwner { Local, Spotify, Failed };
OutputOwner outputOwner = OutputOwner::Local;
MediaStatus spotifyStatus = MediaStatus::Stopped;
PlaybackState playbackState = PlaybackState::Stopped;
bool podcastMode = false, podcastPaused = false, radioMuted = false;
int mainVal = 10;
struct { bool running = false; bool isRunning() { return running; } } audio;
"""
state_start = source.index("constexpr uint32_t kSpeakerBootMuteMs")
state_end = source.index("\n", source.index("bool speakerBootHoldComplete", state_start))
fixture += source[state_start:state_end] + "\n"
fixture += function("void mediaSilenceSpeakers()") + "\n"
fixture += function("void mediaUpdateSpeakerMute(bool bootPlaying)") + "\n"
fixture += r"""
int main() {
    constexpr int MUTED = 1, AUDIBLE = 0;
    auto expect = [](int level, bool boot = false) {
        mediaUpdateSpeakerMute(boot); assert(outputLevel == level);
    };
    playbackState = PlaybackState::Playing; audio.running = true;
    expect(MUTED);
    clockMs = 3999; expect(MUTED); expect(MUTED, true);
    clockMs = 4000; expect(AUDIBLE);
    radioMuted = true; expect(MUTED); expect(MUTED, true);
    radioMuted = false; expect(AUDIBLE);
    mainVal = 0; expect(MUTED);
    expect(AUDIBLE, true); // firmware boot audio has its own fixed volume
    mainVal = 10;
    podcastMode = true; podcastPaused = true; expect(MUTED);
    podcastPaused = false; expect(AUDIBLE);
    podcastPaused = true; podcastMode = false; expect(AUDIBLE); // switching to radio
    audio.running = false; expect(MUTED);
    audio.running = true;
    for (auto state : {PlaybackState::Stopped, PlaybackState::Connecting, PlaybackState::Failed}) {
        playbackState = state; expect(MUTED);
    }
    playbackState = PlaybackState::Playing;
    clockMs = 0; expect(AUDIBLE); // no second boot hold on millis rollover
    speakerBootHoldComplete = false; speakerBootStartedAt = UINT32_MAX - 1000;
    clockMs = 2998; expect(MUTED);
    clockMs = 2999; expect(AUDIBLE); // hold itself crosses rollover
#if defined(RADIO_SPOTIFY_EXPERIMENT)
    outputOwner = OutputOwner::Spotify;
    for (auto state : {MediaStatus::Stopped, MediaStatus::Connecting, MediaStatus::Paused, MediaStatus::Failed}) {
        spotifyStatus = state; expect(MUTED);
    }
    spotifyStatus = MediaStatus::Playing; audio.running = false; expect(AUDIBLE);
    radioMuted = true; expect(MUTED); radioMuted = false;
    mainVal = 0; expect(MUTED); mainVal = 10;
    outputOwner = OutputOwner::Failed; expect(MUTED);
#endif
    mediaSilenceSpeakers(); assert(outputLevel == MUTED);
}
"""
fixture = "#include <initializer_list>\n" + fixture
with tempfile.TemporaryDirectory(prefix="radiohead-speaker-") as folder:
    cpp = Path(folder) / "check.cpp"
    binary = Path(folder) / "check"
    cpp.write_text(fixture)
    for flags in ([], ["-DRADIO_SPOTIFY_EXPERIMENT"]):
        subprocess.run(["c++", "-std=c++17", "-Wall", "-Wextra", "-Werror",
                        *flags, str(cpp), "-o", str(binary)], check=True)
        subprocess.run([str(binary)], check=True)
print("Speaker mute: boot boundary/rollover, volume, radio, podcast and Spotify checks passed.")
