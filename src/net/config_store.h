#pragma once

#include <string>
#include "core/app_state.h"
#include "core/media_store.h"

namespace net {

class ConfigStore {
public:
    bool begin();
    bool loadConfig(core::AppState& state);
    bool saveConfig(const core::AppState& state);

    bool loadAllMedia(core::MediaStore& media);
    bool saveMediaFile(const std::string& id, const uint8_t* data, size_t len);
    bool deleteMediaFile(const std::string& id);

private:
    bool mounted_ = false;
};

} // namespace net
