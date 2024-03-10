#include "LookupForms.h"
#include "ExclusionGroups.h"
#include "FormData.h"
#include "KeywordDependencies.h"

bool Lookup::LookupForms()
{
	using namespace Forms;

	bool valid = false;

	if (const auto dataHandler = RE::TESDataHandler::GetSingleton()) {
		ForEachDistributable([&]<typename Form>(Distributables<Form>& a_distributable) {
			const auto& recordName = RECORD::add[a_distributable.GetType()];

			a_distributable.LookupForms(dataHandler, recordName, INI::configs[recordName]);
			if constexpr (std::is_same_v<RE::BGSKeyword, Form>) {
				Dependencies::ResolveKeywords();
			}
			a_distributable.FinishLookupForms();

			if (a_distributable) {
				valid = true;
			}
		});

		// Lookup exclusion forms too.
		// P.S. Lookup process probably should build some sort of cache and reuse already discovered forms
		//      instead of quering data handler for the same raw FormOrEditorID.
		Exclusion::Manager::GetSingleton()->LookupExclusions(dataHandler, INI::exclusions);
	}

	return valid;
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

	Timer timer;

	timer.start();
	const bool success = LookupForms();
	timer.end();

	if (success) {
		LogFormLookup();
		logger::info("Lookup took {}μs / {}ms", timer.duration_μs(), timer.duration_ms());
	}

	return success;
}
