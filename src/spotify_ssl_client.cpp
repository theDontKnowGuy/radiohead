#ifdef RADIO_SPOTIFY_EXPERIMENT
// The direct IDF/Arduino component omits this implementation while compiling
// NetworkClientSecure.cpp. Include it in the application translation unit so
// its symbols are available before static-library link ordering takes effect.
#include <ssl_client.cpp>
#endif
