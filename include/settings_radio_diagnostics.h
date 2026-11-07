#pragma once
#include "radio_diagnostics_store.h"

namespace RadioLog {
constexpr size_t PageSize = 8;
bool loadDetailedEnabled();
bool saveDetailedEnabled(bool enabled);
// Only at startup, before playback. Existing archive stays intact on failure.
bool archive(const Store& store, uint32_t endingReset, bool crash);
bool readArchive(bool crash, size_t offset, Header& header, uint32_t& phase,
                 uint32_t& phaseMs, uint32_t& endingReset, Record* page,
                 Network* network = nullptr, Breadcrumbs* breadcrumbs = nullptr);
}
