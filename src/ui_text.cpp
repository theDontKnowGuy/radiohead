#include "ui_text.h"

#include <algorithm>

namespace {

constexpr size_t kMaxClusters = 160;

enum class TextDirection : uint8_t {
    Neutral,
    LeftToRight,
    RightToLeft,
};

struct Cluster {
    size_t begin;
    size_t end;
    TextDirection direction;
};

struct Run {
    size_t begin;
    size_t end;
    TextDirection direction;
};

bool isContinuation(uint8_t byte) {
    return (byte & 0xC0U) == 0x80U;
}

uint32_t readUtf8(const String& value, size_t offset, size_t& length) {
    const size_t valueLength = value.length();
    const uint8_t first = static_cast<uint8_t>(value[offset]);
    length = 1;
    if (first < 0x80U) return first;

    size_t expected = 0;
    uint32_t codePoint = 0;
    if ((first & 0xE0U) == 0xC0U) {
        expected = 2;
        codePoint = first & 0x1FU;
    } else if ((first & 0xF0U) == 0xE0U) {
        expected = 3;
        codePoint = first & 0x0FU;
    } else if ((first & 0xF8U) == 0xF0U) {
        expected = 4;
        codePoint = first & 0x07U;
    } else {
        return 0xFFFDU;
    }
    if (offset + expected > valueLength) return 0xFFFDU;
    for (size_t index = 1; index < expected; ++index) {
        const uint8_t byte = static_cast<uint8_t>(value[offset + index]);
        if (!isContinuation(byte)) return 0xFFFDU;
        codePoint = (codePoint << 6U) | (byte & 0x3FU);
    }
    const uint32_t minimum = expected == 2 ? 0x80U : expected == 3 ? 0x800U : 0x10000U;
    if (codePoint < minimum || codePoint > 0x10FFFFU ||
        (codePoint >= 0xD800U && codePoint <= 0xDFFFU)) return 0xFFFDU;
    length = expected;
    return codePoint;
}

bool isCombining(uint32_t codePoint) {
    return (codePoint >= 0x0300U && codePoint <= 0x036FU) ||
        (codePoint >= 0x0591U && codePoint <= 0x05C7U);
}

TextDirection directionFor(uint32_t codePoint) {
    if (codePoint >= 0x05D0U && codePoint <= 0x05EAU) return TextDirection::RightToLeft;
    if ((codePoint >= 'A' && codePoint <= 'Z') || (codePoint >= 'a' && codePoint <= 'z') ||
        (codePoint >= '0' && codePoint <= '9') || codePoint >= 0x00C0U) return TextDirection::LeftToRight;
    return TextDirection::Neutral;
}

void appendCluster(String& destination, const String& source, const Cluster& cluster) {
    for (size_t index = cluster.begin; index < cluster.end; ++index) {
        destination += source[index];
    }
}

}  // namespace

UiTextLayout uiTextLayout(const String& logicalText) {
    UiTextLayout layout;
    if (logicalText.isEmpty()) return layout;
    layout.visual.reserve(std::min(logicalText.length() + 3, kMaxClusters * 4 + 3));

    Cluster clusters[kMaxClusters];
    size_t count = 0;
    size_t offset = 0;
    while (offset < logicalText.length() && count < kMaxClusters) {
        const size_t begin = offset;
        size_t bytes = 0;
        const uint32_t first = readUtf8(logicalText, offset, bytes);
        offset += bytes;
        const TextDirection direction = directionFor(first);
        while (offset < logicalText.length()) {
            size_t combiningBytes = 0;
            if (!isCombining(readUtf8(logicalText, offset, combiningBytes))) break;
            offset += combiningBytes;
        }
        clusters[count++] = {begin, offset, direction};
    }

    TextDirection baseDirection = TextDirection::LeftToRight;
    for (size_t index = 0; index < count; ++index) {
        if (clusters[index].direction != TextDirection::Neutral) {
            baseDirection = clusters[index].direction;
            break;
        }
    }
    layout.rightToLeft = baseDirection == TextDirection::RightToLeft;

    TextDirection resolved[kMaxClusters];
    for (size_t index = 0; index < count; ++index) {
        resolved[index] = clusters[index].direction;
        if (resolved[index] != TextDirection::Neutral) continue;
        TextDirection before = TextDirection::Neutral;
        TextDirection after = TextDirection::Neutral;
        for (size_t cursor = index; cursor > 0; --cursor) {
            if (clusters[cursor - 1].direction != TextDirection::Neutral) {
                before = clusters[cursor - 1].direction;
                break;
            }
        }
        for (size_t cursor = index + 1; cursor < count; ++cursor) {
            if (clusters[cursor].direction != TextDirection::Neutral) {
                after = clusters[cursor].direction;
                break;
            }
        }
        resolved[index] = before != TextDirection::Neutral && before == after ? before : baseDirection;
    }

    Run runs[kMaxClusters];
    size_t runCount = 0;
    for (size_t runBegin = 0; runBegin < count;) {
        size_t runEnd = runBegin + 1;
        while (runEnd < count && resolved[runEnd] == resolved[runBegin]) ++runEnd;
        runs[runCount++] = {runBegin, runEnd, resolved[runBegin]};
        runBegin = runEnd;
    }

    for (size_t visualRun = 0; visualRun < runCount; ++visualRun) {
        const size_t runIndex = layout.rightToLeft ? runCount - visualRun - 1 : visualRun;
        const Run& run = runs[runIndex];
        if (run.direction == TextDirection::RightToLeft) {
            for (size_t index = run.end; index > run.begin; --index) {
                appendCluster(layout.visual, logicalText, clusters[index - 1]);
            }
        } else {
            for (size_t index = run.begin; index < run.end; ++index) {
                appendCluster(layout.visual, logicalText, clusters[index]);
            }
        }
    }
    if (offset < logicalText.length()) layout.visual += "...";
    return layout;
}
