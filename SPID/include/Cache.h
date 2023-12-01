#pragma once

namespace Cache
{
	namespace FormType
	{
		inline constexpr std::array whitelist{
			RE::FormType::Faction,
			RE::FormType::Class,
			RE::FormType::CombatStyle,
			RE::FormType::Race,
			RE::FormType::Outfit,
			RE::FormType::NPC,
			RE::FormType::VoiceType,
			RE::FormType::FormList,
			RE::FormType::Spell,
			RE::FormType::Armor,
			RE::FormType::Location
		};

		bool GetWhitelisted(RE::FormType a_type);
	}
}
