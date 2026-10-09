#pragma once

void startWebServer();
// Reopens HTTP and starts mDNS on a late join/reconnect, or reopens HTTP when
// startup falls back to the setup AP.
void serviceWebConnectivity();
// Services browser-triggered restarts without blocking audio or HTTP servicing.
// It is called once from the Arduino loop.
void serviceWebNetworkRequests(unsigned long now);

// True while an OTA image is being written.  Power/reset paths use this to
// defer destructive transitions until the update has either completed or
// failed.
bool webUpdateInProgress();
