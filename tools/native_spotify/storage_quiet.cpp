#include "settings.h"
#include "Preferences.h"
#include <cassert>
#include <iostream>

int main(int argc, char** argv) {
    assert(argc == 2);
    const bool present = std::string(argv[1]) == "present";
    fakeNvs::reset();
    if (present) {
        fakeNvs::strings["client_id"] = "fixture-id";
        fakeNvs::strings["client_secret"] = "fixture-secret";
    }
    assert(kStorageQuietTest);
    assert(storageReadsAllowed());
    // Even during boot this image must not save replacement credentials.
    assert(!saveSpotifyAppCredentials("replacement-id", "replacement-secret"));
    assert(fakeNvs::opens == 0 && fakeNvs::writes == 0);
    String id, secret;
    assert(loadSpotifyAppCredentials(id, secret) == present);
    const int bootOpens = fakeNvs::opens;
    assert(bootOpens == 1);
    finishStorageBootReads();
    assert(!storageReadsAllowed());

    // Remove the backing store and make every future open/read fail. Runtime
    // playback and web configured-state checks must use the boot snapshot.
    fakeNvs::strings.clear();
    fakeNvs::failOpen = fakeNvs::failRead = true;
    for (int i = 0; i < 10; ++i) {
        id = secret = "stale";
        assert(loadSpotifyAppCredentials(id, secret) == present);
        assert(spotifyAppCredentialsConfigured() == present);
        assert(id == (present ? "fixture-id" : ""));
        assert(secret == (present ? "fixture-secret" : ""));
        assert(!saveSpotifyAppCredentials("replacement-id", "replacement-secret"));
    }
    assert(fakeNvs::opens == bootOpens && fakeNvs::writes == 0);
    std::cout << "Storage-quiet credential snapshot checks passed.\n";
}
