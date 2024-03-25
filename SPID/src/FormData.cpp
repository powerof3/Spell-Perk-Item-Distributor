#include "FormData.h"

std::size_t Forms::GetTotalEntries()
{
	std::size_t size = 0;

	ForEachDistributable([&]<typename Form>(Distributables<Form>& a_distributable) {
		size += a_distributable.GetSize();
	});

	return size;
}

std::size_t Forms::GetTotalLeveledEntries()
{
	std::size_t size = 0;

	ForEachDistributable([&]<typename Form>(Distributables<Form>& a_distributable) {
		size += a_distributable.GetLeveledSize();
	});

	return size;
}

bool Forms::DistributionSet::IsEmpty() const
{
	return spells.empty() && perks.empty() && items.empty() && shouts.empty() && levSpells.empty() && packages.empty() && outfits.empty() && keywords.empty() && deathItems.empty() && factions.empty() && sleepOutfits.empty() && skins.empty();
}
