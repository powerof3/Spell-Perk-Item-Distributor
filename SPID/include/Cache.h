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
		inline constexpr frozen::set<RE::FormType, 9> whitelist = {
			{ RE::FormType::Faction },
			{ RE::FormType::Class },
			{ RE::FormType::CombatStyle },
			{ RE::FormType::Race },
			{ RE::FormType::Outfit },
			{ RE::FormType::NPC },
			{ RE::FormType::VoiceType },
			{ RE::FormType::FormList },
			{ RE::FormType::Spell }
		};
		
		bool GetWhitelisted(RE::FormType a_type);
	}
}
