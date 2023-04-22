#pragma once
#include <string>

#include "Configs.h"
#include "ConfigsCoding.h"

namespace Configs
{
	struct FreeformDataINIConfigDecoder : DataINIConfigDecoder
	{
		void decode(Config& config) override
		{

		}
	};
}
