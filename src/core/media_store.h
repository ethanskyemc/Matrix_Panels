#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include "config_defaults.h"

namespace core {

// Compact binary 16x16 animation/image format (.mpx):
// Header (10 bytes):
//   [0..3] Magic "MPX1"
//   [4]    Width  (16)
//   [5]    Height (16)
//   [6..7] Frame count (uint16_t LE)
//   [8..9] Default frame delay ms (uint16_t LE)
// Per frame (2 + 768 = 770 bytes):
//   [0..1] Frame delay ms (uint16_t LE)
//   [2..769] RGB888 pixel data (16 * 16 * 3 = 768 bytes)

struct MediaFrame {
    uint16_t delayMs = 100;
    std::vector<uint8_t> rgb888; // 768 bytes
};

struct MediaAsset {
    std::string id;
    std::string name;
    bool animated = false;
    std::vector<MediaFrame> frames;
};

class MediaStore {
public:
    MediaStore() = default;

    // Decode .mpx binary payload and store/replace asset under `id`
    bool putMpx(const std::string& id, const std::string& name,
                const uint8_t* data, size_t len);

    // Encode asset to .mpx binary payload
    static std::vector<uint8_t> encodeMpx(const MediaAsset& asset);

    bool remove(const std::string& id);
    const MediaAsset* get(const std::string& id) const;
    std::vector<const MediaAsset*> list() const;
    size_t count() const { return assets_.size(); }

private:
    std::vector<MediaAsset> assets_;
};

} // namespace core
