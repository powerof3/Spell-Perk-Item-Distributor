#include "LookupForms.h"
#include "FormData.h"
#include "KeywordDependencies.h"

bool Lookup::LookupForms()
{
	using namespace Forms;

	if (const auto dataHandler = RE::TESDataHandler::GetSingleton()) {
		ForEachDistributable([dataHandler]<typename Form>(Distributables<Form>& a_distributable) {
			const auto& recordName = RECORD::add[a_distributable.GetType()];

			a_distributable.LookupForms(dataHandler, recordName, INI::configs[recordName]);
			if constexpr (std::is_same_v<RE::BGSKeyword, Form>) {
				Dependencies::ResolveKeywords();
			}
			a_distributable.FinishLookupForms();
		});
	}

	return spells || perks || items || shouts || levSpells || packages || outfits || keywords || deathItems || factions || sleepOutfits || skins;
}

void Lookup::LogFormLookup()
{
	using namespace Forms;

	logger::info("{:*^50}", "PROCESSING");

	ForEachDistributable([]<typename Form>(const Distributables<Form>& a_distributable) {
		const auto& recordName = RECORD::add[a_distributable.GetType()];

		const auto all = INI::configs[recordName].size();
		const auto added = a_distributable.GetSize();

		// Only log entries that are actually present in INIs.
		if (all > 0) {
			logger::info("Adding {}/{} {}s", added, all, recordName);
		}
	});

	// Clear INI map once lookup is done
	INI::configs.clear();

	// Clear logger's buffer to free some memory :)
	buffered_logger::clear();
}

bool Lookup::DoFormLookup()
{
	logger::info("{:*^50}", "LOOKUP");

	const auto startTime = std::chrono::steady_clock::now();
	const bool success = LookupForms();
	const auto endTime = std::chrono::steady_clock::now();

	if (success) {
		LogFormLookup();

		const auto duration = std::chrono::duration_cast<std::chrono::microseconds>(endTime - startTime).count();
		logger::info("Lookup took {}μs / {}ms", duration, duration / 1000.0f);
	}

	return success;
}
