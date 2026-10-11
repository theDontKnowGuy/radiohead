#pragma once

void startWebServer();
// Reopens HTTP and starts mDNS on a late join/reconnect, or reopens HTTP when
// startup falls back to the setup AP.
void serviceWebConnectivity();
// Services deferred browser maintenance once from the Arduino loop. Interactive
// touch calibration temporarily takes over the TFT after acknowledging HTTP.
void serviceWebNetworkRequests(unsigned long now);

// True while firmware maintenance or a reserved web calibration is active.
// Power/reset paths defer destructive transitions until maintenance finishes.
bool webUpdateInProgress();
