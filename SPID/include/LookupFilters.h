#pragma once

namespace NPC
{
	struct Data;
}

namespace Filter
{
	enum class Result
	{
		kFail = 0,
		kFailRNG,
		kPass
	};

	struct Data
	{
		// Note that chance passed to this constructor is expected to be in percent. It will be converted to a decimal chance by the constructor.
		Data(StringFilters a_strings, FormFilters a_formFilters, LevelFilters a_level, Traits a_traits, PercentChance a_chance);

		StringFilters strings{};
		FormFilters   forms{};
		LevelFilters  levels{};
		Traits        traits{};
		DecimalChance chance{ 1 };

		bool hasLeveledFilters;

		[[nodiscard]] bool   HasLevelFilters() const;
		[[nodiscard]] Result PassedFilters(const NPC::Data& a_npcData) const;

	private:
		[[nodiscard]] bool HasLevelFiltersImpl() const;

		[[nodiscard]] Result passed_string_filters(const NPC::Data& a_npcData) const;
		[[nodiscard]] Result passed_form_filters(const NPC::Data& a_npcData) const;
		[[nodiscard]] Result passed_level_filters(const NPC::Data& a_npcData) const;
		[[nodiscard]] Result passed_trait_filters(const NPC::Data& a_npcData) const;
	};
}

using FilterData = Filter::Data;
