#pragma once

struct DeviceInput;

// Bridge sampled hardware input to the semantic controller. The controller
// remains independent of TFT, Audio, Preferences and device side effects.
void serviceUiInput(const DeviceInput& input, unsigned long now);
// Execute queued commands on the Arduino loop using their owning modules.
void serviceUiCommands(unsigned long now);
