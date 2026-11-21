#include "core/media_store.h"

#include <algorithm>
#include <cstring>

namespace core {

static inline uint16_t readU16LE(const uint8_t* p) {
    return static_cast<uint16_t>(p[0]) | (static_cast<uint16_t>(p[1]) << 8);
}

static inline void writeU16LE(std::vector<uint8_t>& out, uint16_t v) {
    out.push_back(static_cast<uint8_t>(v & 0xFF));
    out.push_back(static_cast<uint8_t>((v >> 8) & 0xFF));
}

bool MediaStore::putMpx(const std::string& id, const std::string& name,
                        const uint8_t* data, size_t len) {
    if (!data || len < 10) return false;
    if (std::memcmp(data, "MPX1", 4) != 0) return false;

    uint8_t w = data[4];
    uint8_t h = data[5];
    uint16_t frameCount = readU16LE(data + 6);
    uint16_t defaultDelay = readU16LE(data + 8);
    if (w != kMatrixWidth || h != kMatrixHeight || frameCount == 0 || frameCount > kMaxMediaFrames) {
        return false;
    }

    size_t frameBytes = 2 + kMatrixPixels * 3;
    if (len < 10 + static_cast<size_t>(frameCount) * frameBytes) {
        return false;
    }

    MediaAsset asset;
    asset.id = id;
    asset.name = name.empty() ? id : name;
    asset.animated = (frameCount > 1);
    asset.frames.reserve(frameCount);

    const uint8_t* ptr = data + 10;
    for (uint16_t i = 0; i < frameCount; ++i) {
        MediaFrame f;
        uint16_t d = readU16LE(ptr);
        f.delayMs = (d < 20) ? (defaultDelay >= 20 ? defaultDelay : 100) : d;
        ptr += 2;
        f.rgb888.assign(ptr, ptr + kMatrixPixels * 3);
        ptr += kMatrixPixels * 3;
        asset.frames.push_back(std::move(f));
    }

    for (auto& existing : assets_) {
        if (existing.id == id) {
            existing = std::move(asset);
            return true;
        }
    }
    assets_.push_back(std::move(asset));
    return true;
}

std::vector<uint8_t> MediaStore::encodeMpx(const MediaAsset& asset) {
    std::vector<uint8_t> out;
    if (asset.frames.empty()) return out;
    uint16_t count = static_cast<uint16_t>(std::min<size_t>(asset.frames.size(), kMaxMediaFrames));
    out.reserve(10 + count * (2 + kMatrixPixels * 3));

    out.push_back('M'); out.push_back('P'); out.push_back('X'); out.push_back('1');
    out.push_back(kMatrixWidth);
    out.push_back(kMatrixHeight);
    writeU16LE(out, count);
    writeU16LE(out, asset.frames[0].delayMs);

    for (uint16_t i = 0; i < count; ++i) {
        writeU16LE(out, asset.frames[i].delayMs);
        const auto& rgb = asset.frames[i].rgb888;
        if (rgb.size() >= kMatrixPixels * 3) {
            out.insert(out.end(), rgb.begin(), rgb.begin() + kMatrixPixels * 3);
        } else {
            out.insert(out.end(), kMatrixPixels * 3, 0);
        }
    }
    return out;
}

bool MediaStore::remove(const std::string& id) {
    auto it = std::remove_if(assets_.begin(), assets_.end(),
                             [&](const MediaAsset& a) { return a.id == id; });
    if (it == assets_.end()) return false;
    assets_.erase(it, assets_.end());
    return true;
}

const MediaAsset* MediaStore::get(const std::string& id) const {
    for (const auto& a : assets_) {
        if (a.id == id) return &a;
    }
    return nullptr;
}

std::vector<const MediaAsset*> MediaStore::list() const {
    std::vector<const MediaAsset*> out;
    out.reserve(assets_.size());
    for (const auto& a : assets_) out.push_back(&a);
    return out;
}

} // namespace core
