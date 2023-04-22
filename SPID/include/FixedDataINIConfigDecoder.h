#pragma once

#include "Configs.h"
#include "ConfigsCoding.h"

namespace Configs
{
    struct FixedDataINIConfigDecoder : DataINIConfigDecoder
    {
        void decode(Config& config);
    };
}
