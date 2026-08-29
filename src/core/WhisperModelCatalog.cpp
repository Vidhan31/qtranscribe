#include "WhisperModelCatalog.h"

#include <algorithm>
#include <ranges>

using namespace Qt::StringLiterals;

namespace {
const auto kHfBaseUrl = u"https://huggingface.co/ggerganov/whisper.cpp/resolve/main/"_s;
} // namespace

WhisperModelCatalog::WhisperModelCatalog() {
    initPresets();
}

const QList<WhisperModelItem>& WhisperModelCatalog::presets() const {
    return m_presets;
}

std::optional<WhisperModelItem> WhisperModelCatalog::model(const QString& modelId) const {
    const auto it = std::ranges::find_if(m_presets, [&](const auto& m) { return m.id == modelId; });
    return it != m_presets.end() ? std::optional(*it) : std::nullopt;
}

int WhisperModelCatalog::modelCount() const {
    return static_cast<int>(m_presets.size());
}

int WhisperModelCatalog::findModelIndex(const QString& modelId) const {
    const auto it = std::ranges::find_if(m_presets, [&](const auto& m) { return m.id == modelId; });
    return it != m_presets.end() ? static_cast<int>(std::distance(m_presets.begin(), it)) : -1;
}

void WhisperModelCatalog::initPresets() {
    m_presets = {
        WhisperModelItem {.id = u"tiny.en"_s,
                          .name = tr("Tiny (English)"),
                          .fileName = u"ggml-tiny.en.bin"_s,
                          .downloadUrl = kHfBaseUrl + u"ggml-tiny.en.bin"_s,
                          .sizeBytes = 77704715,
                          .sizeFormatted = u"~74 MiB"_s,
                          .memoryFormatted = u"~273 MB RAM/VRAM"_s,
                          .description =
                              tr("Fastest English dictation with lowest resource usage and minimal latency.")},
        WhisperModelItem {.id = u"tiny"_s,
                          .name = tr("Tiny (Multilingual)"),
                          .fileName = u"ggml-tiny.bin"_s,
                          .downloadUrl = kHfBaseUrl + u"ggml-tiny.bin"_s,
                          .sizeBytes = 77691713,
                          .sizeFormatted = u"~74 MiB"_s,
                          .memoryFormatted = u"~273 MB RAM/VRAM"_s,
                          .description =
                              tr("Ultra-fast multilingual dictation across 99+ languages with minimal memory usage.")},
        WhisperModelItem {.id = u"base.en"_s,
                          .name = tr("Base (English)"),
                          .fileName = u"ggml-base.en.bin"_s,
                          .downloadUrl = kHfBaseUrl + u"ggml-base.en.bin"_s,
                          .sizeBytes = 147964211,
                          .sizeFormatted = u"~141 MiB"_s,
                          .memoryFormatted = u"~388 MB RAM/VRAM"_s,
                          .description =
                              tr("Fast English transcription with improved accuracy over Tiny for general speech.")},
        WhisperModelItem {.id = u"base"_s,
                          .name = tr("Base (Multilingual)"),
                          .fileName = u"ggml-base.bin"_s,
                          .downloadUrl = kHfBaseUrl + u"ggml-base.bin"_s,
                          .sizeBytes = 147951465,
                          .sizeFormatted = u"~141 MiB"_s,
                          .memoryFormatted = u"~388 MB RAM/VRAM"_s,
                          .description =
                              tr("Fast multilingual transcription with solid baseline recognition accuracy.")},
        WhisperModelItem {.id = u"small.en"_s,
                          .name = tr("Small (English)"),
                          .fileName = u"ggml-small.en.bin"_s,
                          .downloadUrl = kHfBaseUrl + u"ggml-small.en.bin"_s,
                          .sizeBytes = 487614201,
                          .sizeFormatted = u"~465 MiB"_s,
                          .memoryFormatted = u"~852 MB RAM/VRAM"_s,
                          .description =
                              tr("High accuracy English transcription; recommended sweet spot for desktop dictation.")},
        WhisperModelItem {
            .id = u"small"_s,
            .name = tr("Small (Multilingual)"),
            .fileName = u"ggml-small.bin"_s,
            .downloadUrl = kHfBaseUrl + u"ggml-small.bin"_s,
            .sizeBytes = 487601967,
            .sizeFormatted = u"~465 MiB"_s,
            .memoryFormatted = u"~852 MB RAM/VRAM"_s,
            .description = tr("High accuracy multilingual model; excellent balance of speed and recognition quality.")},
        WhisperModelItem {
            .id = u"medium.en"_s,
            .name = tr("Medium (English)"),
            .fileName = u"ggml-medium.en.bin"_s,
            .downloadUrl = kHfBaseUrl + u"ggml-medium.en.bin"_s,
            .sizeBytes = 1533774781,
            .sizeFormatted = u"~1.4 GiB"_s,
            .memoryFormatted = u"~2.1 GB RAM/VRAM"_s,
            .description =
                tr("Near-professional English accuracy for complex vocabulary, technical terms, and accents.")},
        WhisperModelItem {
            .id = u"medium"_s,
            .name = tr("Medium (Multilingual)"),
            .fileName = u"ggml-medium.bin"_s,
            .downloadUrl = kHfBaseUrl + u"ggml-medium.bin"_s,
            .sizeBytes = 1533763059,
            .sizeFormatted = u"~1.4 GiB"_s,
            .memoryFormatted = u"~2.1 GB RAM/VRAM"_s,
            .description =
                tr("Professional-grade multilingual transcription across diverse accents and audio conditions.")},
        WhisperModelItem {.id = u"large-v3-turbo"_s,
                          .name = tr("Large v3 Turbo (Multilingual)"),
                          .fileName = u"ggml-large-v3-turbo.bin"_s,
                          .downloadUrl = kHfBaseUrl + u"ggml-large-v3-turbo.bin"_s,
                          .sizeBytes = 1624555275,
                          .sizeFormatted = u"~1.5 GiB"_s,
                          .memoryFormatted = u"~2.3 GB RAM/VRAM"_s,
                          .description = tr("State-of-the-art multilingual accuracy with optimized 4-layer fast "
                                            "decoder (up to 8x faster than Large v3).")},
        WhisperModelItem {.id = u"large-v3"_s,
                          .name = tr("Large v3 (Multilingual)"),
                          .fileName = u"ggml-large-v3.bin"_s,
                          .downloadUrl = kHfBaseUrl + u"ggml-large-v3.bin"_s,
                          .sizeBytes = 3095033483,
                          .sizeFormatted = u"~2.9 GiB"_s,
                          .memoryFormatted = u"~3.9 GB RAM/VRAM"_s,
                          .description = tr("Maximum accuracy flagship Whisper model for challenging audio, background "
                                            "noise, and rare dialects.")}};
}
