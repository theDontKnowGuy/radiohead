#include "settings.h"

#include <cstring>
#include <Preferences.h>

namespace {
// One NVS blob prevents an interrupted replacement from mixing two app pairs.
// Legacy USB-provisioned client_id/client_secret remain readable until a web save.
constexpr const char* CREDENTIAL_KEY = "app_creds";
struct SpotifyAppCredentials {
    char version[4];
    char clientId[SPOTIFY_APP_CREDENTIAL_MAX_BYTES + 1];
    char clientSecret[SPOTIFY_APP_CREDENTIAL_MAX_BYTES + 1];
};

bool validStoredCredential(const char* value) {
    const size_t length = strnlen(value, SPOTIFY_APP_CREDENTIAL_MAX_BYTES + 1);
    return length <= SPOTIFY_APP_CREDENTIAL_MAX_BYTES &&
        isValidSpotifyAppCredential(String(value));
}
} // namespace

bool isValidSpotifyAppCredential(const String& value) {
    if (value.isEmpty() || value.length() > SPOTIFY_APP_CREDENTIAL_MAX_BYTES) return false;
    for (size_t i = 0; i < value.length(); ++i) {
        const uint8_t character = static_cast<uint8_t>(value[i]);
        if (character < 0x21 || character > 0x7e) return false;
    }
    return true;
}

bool loadSpotifyAppCredentials(String& clientId, String& clientSecret) {
    clientId = "";
    clientSecret = "";
    Preferences credentials;
    if (!credentials.begin("spotify", true)) return false;
    SpotifyAppCredentials stored = {};
    bool loaded = false;
    if (credentials.isKey(CREDENTIAL_KEY)) {
        // A malformed new record must not silently revive superseded credentials.
        loaded = credentials.getBytesLength(CREDENTIAL_KEY) == sizeof(stored) &&
            credentials.getBytes(CREDENTIAL_KEY, &stored, sizeof(stored)) == sizeof(stored) &&
            memcmp(stored.version, "RHS1", sizeof(stored.version)) == 0 &&
            validStoredCredential(stored.clientId) && validStoredCredential(stored.clientSecret);
    } else {
        // Read legacy strings into bounded buffers rather than allocating from
        // an unchecked persisted size. Preferences includes the trailing NUL.
        loaded = credentials.getString("client_id", stored.clientId, sizeof(stored.clientId)) > 1 &&
            credentials.getString("client_secret", stored.clientSecret, sizeof(stored.clientSecret)) > 1 &&
            validStoredCredential(stored.clientId) && validStoredCredential(stored.clientSecret);
    }
    credentials.end();
    if (!loaded) return false;
    clientId = stored.clientId;
    clientSecret = stored.clientSecret;
    return clientId.length() == strlen(stored.clientId) &&
        clientSecret.length() == strlen(stored.clientSecret);
}

bool spotifyAppCredentialsConfigured() {
    String clientId, clientSecret;
    return loadSpotifyAppCredentials(clientId, clientSecret);
}

bool saveSpotifyAppCredentials(const String& clientId, const String& clientSecret) {
    if (!isValidSpotifyAppCredential(clientId) || !isValidSpotifyAppCredential(clientSecret)) return false;
    SpotifyAppCredentials stored = {};
    memcpy(stored.version, "RHS1", sizeof(stored.version));
    memcpy(stored.clientId, clientId.c_str(), clientId.length());
    memcpy(stored.clientSecret, clientSecret.c_str(), clientSecret.length());
    Preferences credentials;
    if (!credentials.begin("spotify", false)) return false;
    const bool saved = credentials.putBytes(CREDENTIAL_KEY, &stored, sizeof(stored)) == sizeof(stored);
    credentials.end();
    return saved;
}
