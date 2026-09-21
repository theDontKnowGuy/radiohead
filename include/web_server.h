#pragma once

void startWebServer();
// Services browser-triggered Wi-Fi transitions without blocking audio or HTTP
// servicing.  It is called once from the Arduino loop.
void serviceWebNetworkRequests(unsigned long now);
