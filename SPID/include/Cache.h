#pragma once

extern HMODULE tweaks;

namespace Cache
{
	using _GetFormEditorID = const char* (*)(std::uint32_t);

	namespace EditorID
	{
		std::string GetEditorID(RE::FormID a_formID);
	}

	namespace FormType
	{
		inline constexpr frozen::map<RE::FormType, std::string_view, 7> whitelistMap = {
			{ RE::FormType::Faction, "Faction"sv },
			{ RE::FormType::Class, "Class"sv },
			{ RE::FormType::CombatStyle, "CombatStyle"sv },
			{ RE::FormType::Race, "Race"sv },
			{ RE::FormType::Outfit, "Outfit"sv },
			{ RE::FormType::NPC, "NPC"sv },
			{ RE::FormType::VoiceType, "VoiceType"sv }
		};
		
		std::string GetWhitelistFormString(RE::FormType a_type);
	}
}
