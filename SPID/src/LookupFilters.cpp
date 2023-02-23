#include "LookupFilters.h"
#include "LookupNPC.h"

// ------------------- Data --------------------
namespace Filter
{
	Data::Data(const AndExpression filters) :
		filters(filters)
	{
		hasLeveledFilters = HasLevelFiltersImpl();
	}

	bool Data::HasLevelFilters() const
	{
		return hasLeveledFilters;
	}

	bool Data::HasLevelFiltersImpl() const
	{
		return filters.contains<LevelRange>([](const auto& value) -> bool {
			return value.min != LevelRange::MinLevel || value.max != LevelRange::MaxLevel;
		});
	}

	Result Data::PassedFilters(const NPCData& a_npcData) const
	{
		if (const auto npc = a_npcData.GetNPC(); HasLevelFilters() && npc->HasPCLevelMult()) {
			return Result::kFail;
		}
		return filters.evaluate(a_npcData);
	}
}
