#include "Cache.h"

namespace Cache
{
	std::string EditorID::GetEditorID(RE::FormID a_formID)
	{
		static auto function = reinterpret_cast<_GetFormEditorID>(GetProcAddress(tweaks, "GetFormEditorID"));
		if (function) {
			return function(a_formID);
		}
		return std::string();
	}

	std::string FormType::GetWhitelistFormString(const RE::FormType a_type)
	{
		auto it = whitelistMap.find(a_type);
		return it != whitelistMap.end() ? std::string(it->second) : std::string();
	}
}
