#include "LookupForms.h"
#include "ExclusiveGroups.h"
#include "FormData.h"
#include "KeywordDependencies.h"
#include "LinkedDistribution.h"


bool LookupDistributables(RE::TESDataHandler* const dataHandler)
{
	using namespace Forms;

	ForEachDistributable([&]<typename Form>(Distributables<Form>& a_distributable) {
		const auto& recordName = RECORD::GetTypeName(a_distributable.GetType());

		a_distributable.LookupForms(dataHandler, recordName, INI::configs[a_distributable.GetType()]);
	});

	auto& genericForms = INI::configs[RECORD::kForm];

	for (auto& rawForm : genericForms) {
		// Add to appropriate list. (Note that type inferring doesn't recognize SleepOutfit or DeathItems)
		LookupGenericForm<RE::TESForm>(dataHandler, rawForm, [&](auto form, auto& idxOrCount, auto& filters, std::string& path) {
			if (const auto keyword = form->As<RE::BGSKeyword>(); keyword) {
				keywords.GetForms().emplace_back(keywords.GetSize(), keyword, idxOrCount, filters, path);
			} else if (const auto spell = form->As<RE::SpellItem>(); spell) {
				spells.GetForms().emplace_back(spells.GetSize(), spell, idxOrCount, filters, path);
			} else if (const auto perk = form->As<RE::BGSPerk>(); perk) {
				perks.GetForms().emplace_back(perks.GetSize(), perk, idxOrCount, filters, path);
			} else if (const auto shout = form->As<RE::TESShout>(); shout) {
				shouts.GetForms().emplace_back(shouts.GetSize(), shout, idxOrCount, filters, path);
			} else if (const auto item = form->As<RE::TESBoundObject>(); item) {
				items.GetForms().emplace_back(items.GetSize(), item, idxOrCount, filters, path);
			} else if (const auto outfit = form->As<RE::BGSOutfit>(); outfit) {
				outfits.GetForms().emplace_back(outfits.GetSize(), outfit, idxOrCount, filters, path);
			} else if (const auto faction = form->As<RE::TESFaction>(); faction) {
				factions.GetForms().emplace_back(factions.GetSize(), faction, idxOrCount, filters, path);
			} else if (const auto skin = form->As<RE::TESObjectARMO>(); skin) {
				skins.GetForms().emplace_back(skins.GetSize(), skin, idxOrCount, filters, path);
			} else {
				auto type = form->GetFormType();
				if (type == RE::FormType::Package || type == RE::FormType::FormList) {
					// With generic Form entries we default to RandomCount, so we need to properly convert it to Index if it turned out to be a package.
					Index packageIndex = 1;
					if (std::holds_alternative<RandomCount>(idxOrCount)) {
						auto& count = std::get<RandomCount>(idxOrCount);
						if (!count.IsExact()) {
							logger::warn("Inferred Form is a Package, but specifies a random count instead of index. Min value ({}) of the range will be used as an index.", count.min);
						}
						packageIndex = count.min;
					} else {
						packageIndex = std::get<Index>(idxOrCount);
					}
					packages.GetForms().emplace_back(packages.GetSize(), form, packageIndex, filters, path);
				}
			}
		});		
	}

	Dependencies::ResolveKeywords();

	bool valid = false;

	ForEachDistributable([&]<typename Form>(Distributables<Form>& a_distributable) {
		a_distributable.FinishLookupForms();
		if (a_distributable) {
			valid = true;
		}
	});

	return valid;
}

void LogDistributablesLookup()
{
	using namespace Forms;

	logger::info("{:*^50}", "PROCESSING");

	ForEachDistributable([]<typename Form>(Distributables<Form>& a_distributable) {
		const auto& recordName = RECORD::GetTypeName(a_distributable.GetType());

		const auto all = INI::configs[a_distributable.GetType()].size();
		const auto added = a_distributable.GetSize();

		// Only log entries that are actually present in INIs.
		if (all > 0) {
			logger::info("Registered {}/{} {}s:", added, all, recordName);
		}
	});

	// Clear INI map once lookup is done
	INI::configs.clear();

	// Clear logger's buffer to free some memory :)
	buffered_logger::clear();
}

// Lookup forms in exclusvie groups too.
// P.S. Lookup process probably should build some sort of cache and reuse already discovered forms
//      instead of quering data handler for the same raw FormOrEditorID.
void LookupExclusiveGroups(RE::TESDataHandler* const dataHandler)
{
	ExclusiveGroups::Manager::GetSingleton()->LookupExclusiveGroups(dataHandler, INI::exclusiveGroups);
}

void LogExclusiveGroupsLookup()
{
	if (const auto manager = ExclusiveGroups::Manager::GetSingleton(); manager) {
		const auto& groups = manager->GetGroups();

		if (!groups.empty()) {
			logger::info("{:*^50}", "EXCLUSIVE GROUPS");

			for (const auto& [group, forms] : groups) {
				logger::info("Adding '{}' exclusive group", group);
				for (const auto& form : forms) {
					logger::info("  {}", describe(form));
				}
			}
		}
	}
}

void LookupLinkedForms(RE::TESDataHandler* const dataHandler)
{
	LinkedDistribution::Manager::GetSingleton()->LookupLinkedForms(dataHandler);
}

void LogLinkedFormsLookup()
{
	LinkedDistribution::Manager::GetSingleton()->LogLinkedFormsLookup();
}

bool Lookup::LookupForms()
{
	if (const auto dataHandler = RE::TESDataHandler::GetSingleton(); dataHandler) {
		logger::info("{:*^50}", "LOOKUP");

		Timer timer;

		timer.start();
		const bool success = LookupDistributables(dataHandler);
		timer.end();

		if (success) {
			LogDistributablesLookup();
			logger::info("Lookup took {}μs / {}ms", timer.duration_μs(), timer.duration_ms());
		}

		LookupLinkedForms(dataHandler);
		LogLinkedFormsLookup();

		LookupExclusiveGroups(dataHandler);
		LogExclusiveGroupsLookup();

		return success;
	}

	return false;
}
