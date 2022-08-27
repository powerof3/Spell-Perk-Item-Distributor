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

	bool FormType::GetWhitelisted(const RE::FormType a_type)
	{
		return whitelist.find(a_type) != whitelist.end();
	}
}
