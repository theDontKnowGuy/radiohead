#include "settings.h"
#include "Preferences.h"
#include <cassert>
#include <iostream>

void expectPair(const String& expectedId, const String& expectedSecret) {
    String id, secret;
    assert(loadSpotifyAppCredentials(id, secret));
    assert(id == expectedId && secret == expectedSecret);
    assert(spotifyAppCredentialsConfigured());
}

int main() {
    using namespace fakeNvs;
    reset();
    String id = "old-output", secret = "old-output";
    assert(!loadSpotifyAppCredentials(id, secret));
    assert(id.empty() && secret.empty());
    assert(!spotifyAppCredentialsConfigured());
    for (const String& bad : {String(""), String("with space"), String("line\n"),
                             String("\x7f"), String("\xc3\xa9"), String(129, 'a'), String("a\0b", 3)}) {
        assert(!saveSpotifyAppCredentials(bad, "fixture-secret"));
        assert(!saveSpotifyAppCredentials("fixture-id", bad));
    }
    assert(writes == 0);
    strings["client_id"] = "legacy-fixture-id";
    assert(!spotifyAppCredentialsConfigured());
    strings["client_secret"] = "legacy-fixture-secret";
    expectPair("legacy-fixture-id", "legacy-fixture-secret");
    strings["client_id"] = std::string(129, 'a');
    assert(!spotifyAppCredentialsConfigured());
    strings["client_id"] = "legacy-fixture-id";
    failWrite = true;
    assert(!saveSpotifyAppCredentials("new-fixture-id", "new-fixture-secret"));
    expectPair("legacy-fixture-id", "legacy-fixture-secret");
    failWrite = false;
    assert(saveSpotifyAppCredentials("new-fixture-id", "new-fixture-secret"));
    assert(writes == 1); // Whole pair is a single durable write.
    expectPair("new-fixture-id", "new-fixture-secret");
    assert(strings.at("client_id") == "legacy-fixture-id");
    failWrite = true;
    assert(!saveSpotifyAppCredentials("other-fixture-id", "other-fixture-secret"));
    expectPair("new-fixture-id", "new-fixture-secret");
    failWrite = false;
    assert(saveSpotifyAppCredentials(String(128, 'i'), String(128, 's')));
    expectPair(String(128, 'i'), String(128, 's'));
    const auto valid = blobs.at("app_creds");
    blobs["app_creds"].resize(3);
    assert(!spotifyAppCredentialsConfigured()); // No revival of legacy keys.
    blobs["app_creds"] = valid;
    blobs["app_creds"][0] = 'X';
    assert(!spotifyAppCredentialsConfigured());
    blobs["app_creds"] = valid;
    blobs["app_creds"][4 + 128] = 'X'; // Missing NUL, read must stay bounded.
    assert(!spotifyAppCredentialsConfigured());
    blobs["app_creds"] = valid;
    failRead = true;
    assert(!spotifyAppCredentialsConfigured());
    failRead = false;
    failOpen = true;
    assert(!saveSpotifyAppCredentials("fixture-id", "fixture-secret"));
    assert(!spotifyAppCredentialsConfigured());
    std::cout << "Spotify credential storage checks passed.\n";
}
