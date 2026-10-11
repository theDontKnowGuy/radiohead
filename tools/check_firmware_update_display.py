"""Exercise production updater-display sampling without TFT/network hardware."""
from pathlib import Path
import subprocess
import tempfile

root = Path(__file__).resolve().parents[1]
source = (root / "src/display.cpp").read_text()
state = source[source.index("bool firmwareUpdateOverlayActive ="):
               source.index("String urlEncode(")]
service = source[source.index("void serviceFirmwareUpdateDisplay("):
                 source.index("void initializeDisplay(")]
stub = r"""
#include <cassert>
#include <cstdint>
class FirmwareUpdater {
public:
    enum class Status { Idle, Checking, UpToDate, UpdateFound, Downloading, Installed, Failed };
    struct Snapshot {
        Status status = Status::Idle;
        uint8_t downloadPercent = 0;
        bool installationAttempted = false;
    };
    Snapshot value;
    uint32_t revision = 0;
    unsigned snapshots = 0;
    uint32_t statusRevision() const { return revision; }
    Snapshot snapshot() { ++snapshots; return value; }
    void publish(Status status, uint8_t percent = 0, bool attempted = false) {
        value = {status, percent, attempted}; ++revision;
    }
} firmwareUpdater;
bool forceRedraw = false, isDimmed = true;
unsigned long lastInteraction = 0;
int brightness = 20;
void setBrightness(int value) { brightness = value; }
"""
checks = r"""
int main() {
    using Status = FirmwareUpdater::Status;
    // A failed download may finish before the loop samples Downloading.
    firmwareUpdater.publish(Status::Failed, 0, true);
    serviceFirmwareUpdateDisplay(250);
    assert(releaseUpdateOverlayActive() && forceRedraw);
    assert(!isDimmed && brightness == 255 && lastInteraction == 250);
    // An unrelated revision must not restart the failure notice timer.
    firmwareUpdater.publish(Status::Failed, 0, true);
    serviceFirmwareUpdateDisplay(500);
    assert(releaseUpdateFailedAt == 250);
    serviceFirmwareUpdateDisplay(8249);
    assert(releaseUpdateOverlayActive());
    serviceFirmwareUpdateDisplay(8250);
    assert(!releaseUpdateOverlayActive());
    // Routine manifest failures must not interrupt normal listening.
    firmwareUpdater.publish(Status::Checking);
    serviceFirmwareUpdateDisplay(8500);
    firmwareUpdater.publish(Status::Failed);
    serviceFirmwareUpdateDisplay(8750);
    assert(!releaseUpdateOverlayActive());
    firmwareUpdater.publish(Status::Downloading, 20, true);
    serviceFirmwareUpdateDisplay(9000);
    assert(releaseUpdateOverlayActive() && releaseUpdatePercent == 20);
    firmwareUpdater.publish(Status::Downloading, 45, true);
    const auto samples = firmwareUpdater.snapshots;
    serviceFirmwareUpdateDisplay(9100);
    assert(releaseUpdatePercent == 20 && firmwareUpdater.snapshots == samples);
    serviceFirmwareUpdateDisplay(9250);
    assert(releaseUpdatePercent == 45);
    serviceFirmwareUpdateDisplay(9500);
    assert(firmwareUpdater.snapshots == samples + 1);
    firmwareUpdater.publish(Status::Installed, 100, true);
    serviceFirmwareUpdateDisplay(9750);
    assert(releaseUpdateDisplayStatus == Status::Installed && releaseUpdatePercent == 100);
    // Browser uploads also remain awake when remote status is inactive.
    firmwareUpdater.publish(Status::UpToDate);
    firmwareUpdateOverlayActive = true;
    isDimmed = true;
    serviceFirmwareUpdateDisplay(10000);
    assert(!isDimmed && brightness == 255 && lastInteraction == 10000);
}
"""
with tempfile.TemporaryDirectory(prefix="radiohead-update-display-") as directory:
    path = Path(directory)
    (path / "check.cpp").write_text(stub + state + service + checks)
    subprocess.run(["clang++", "-std=c++17", "-Wall", "-Wextra", "-Werror",
                    str(path / "check.cpp"), "-o", str(path / "check")], check=True)
    subprocess.run([str(path / "check")], check=True)
print("Firmware update display transitions passed")
