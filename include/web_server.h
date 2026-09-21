#pragma once

void startWebServer();
// Services browser-triggered restarts without blocking audio or HTTP servicing.
// It is called once from the Arduino loop.
void serviceWebNetworkRequests(unsigned long now);
