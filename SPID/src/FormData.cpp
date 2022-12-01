#include "FormData.h"

std::size_t Forms::GetTotalEntries()
{
	return keywords.GetSize() +
	       spells.GetSize() +
	       perks.GetSize() +
	       items.GetSize() +
	       shouts.GetSize() +
	       levSpells.GetSize() +
	       packages.GetSize() +
	       outfits.GetSize() +
	       deathItems.GetSize() +
	       factions.GetSize() +
	       sleepOutfits.GetSize() +
	       skins.GetSize();
}

std::size_t Forms::GetTotalLeveledEntries()
{
	return keywords.GetLeveledSize() +
	       spells.GetLeveledSize() +
	       perks.GetLeveledSize() +
	       items.GetLeveledSize() +
	       shouts.GetLeveledSize() +
	       levSpells.GetLeveledSize() +
	       packages.GetLeveledSize() +
	       outfits.GetLeveledSize() +
	       deathItems.GetLeveledSize() +
	       factions.GetLeveledSize() +
	       sleepOutfits.GetLeveledSize() +
	       skins.GetLeveledSize();
}
