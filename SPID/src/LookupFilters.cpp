#include "LookupFilters.h"
#include "LookupNPC.h"

// ------------------- Data --------------------
namespace filters
{
	Data::Data(NPCExpression* filters) :
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
		return filters->contains<SPID::LevelFilter>([](const auto& filter) -> bool {
			return filter->value.first != SPID::LevelFilter::MinLevel || filter->value.second != SPID::LevelFilter::MaxLevel;
		});
	}

	Result Data::PassedFilters(const NPCData& a_npcData) const
	{
		if (const auto npc = a_npcData.GetNPC(); HasLevelFilters() && npc->HasPCLevelMult()) {
			return Result::kFail;
		}
		return filters->evaluate(a_npcData);
	}
}
