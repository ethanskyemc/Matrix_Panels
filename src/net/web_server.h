#pragma once

#include <cstdint>
#include "core/app_state.h"
#include "core/display_engine.h"
#include "core/media_store.h"
#include "net/config_store.h"
#include "net/wifi_manager.h"

namespace net {

class MatrixWebServer {
public:
    void begin(core::AppState& state,
               core::DisplayEngine& engine,
               core::MediaStore& media,
               ConfigStore& store,
               WifiManager& wifi);

    void poll(uint32_t nowMs, const core::DisplayEngine& engine);

private:
    uint32_t lastPreviewBroadcastMs_ = 0;
};

} // namespace net
