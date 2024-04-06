#include "LinkedDistribution.h"
#include "FormData.h"
#include "LookupConfigs.h"

namespace LinkedDistribution
{

	using namespace Forms;

#pragma region Parsing
	namespace INI
	{
		enum Sections : std::uint8_t
		{
			kForm = 0,
			kLinkedForms,
			kIdxOrCount,
			kChance,

			// Minimum required sections
			kRequired = kLinkedForms
		};

		bool TryParse(const std::string& originalKey, const std::string& value, const Path& path)
		{
			std::string key = originalKey;

			Scope scope = kLocal;

			if (key.starts_with("Global"sv)) {
				scope = kGlobal;
				key.erase(0, 6);
			}

			if (!key.starts_with("Linked"sv)) {
				return false;
			}

			std::string rawType = key.substr(6);
			auto        type = RECORD::GetType(rawType);

			if (type == RECORD::kTotal) {
				logger::warn("IGNORED: Unsupported Linked Form type: {}"sv, rawType);
				return true;
			}

			const auto sections = string::split(value, "|");
			const auto size = sections.size();

			if (size <= kRequired) {
				logger::warn("IGNORED: LinkedItem must have a form and at least one Form Filter: {} = {}"sv, key, value);
				return true;
			}

			auto split_IDs = distribution::split_entry(sections[kLinkedForms]);

			if (split_IDs.empty()) {
				logger::warn("IGNORED: LinkedItem must have at least one Form Filter : {} = {}"sv, key, value);
				return true;
			}

			INI::RawLinkedForm item{};
			item.formOrEditorID = distribution::get_record(sections[kForm]);
			item.scope = scope;
			item.path = path;

			for (auto& IDs : split_IDs) {
				item.formIDs.MATCH.push_back(distribution::get_record(IDs));
			}

			if (type == RECORD::kPackage) {  // reuse item count for package stack index
				item.idxOrCount = 0;
			}

			if (kIdxOrCount < size) {
				if (type == RECORD::kPackage) {
					if (const auto& str = sections[kIdxOrCount]; distribution::is_valid_entry(str)) {
						item.idxOrCount = string::to_num<Index>(str);
					}
				} else {
					if (const auto& str = sections[kIdxOrCount]; distribution::is_valid_entry(str)) {
						if (auto countPair = string::split(str, "-"); countPair.size() > 1) {
							auto minCount = string::to_num<Count>(countPair[0]);
							auto maxCount = string::to_num<Count>(countPair[1]);

							item.idxOrCount = RandomCount(minCount, maxCount);
						} else {
							auto count = string::to_num<Count>(str);

							item.idxOrCount = RandomCount(count, count);  // create the exact match range.
						}
					}
				}
			}

			if (kChance < size) {
				if (const auto& str = sections[kChance]; distribution::is_valid_entry(str)) {
					item.chance = string::to_num<PercentChance>(str);
				}
			}

			linkedForms[type].push_back(item);

			return true;
		}
	}
#pragma endregion

#pragma region Lookup

	void Manager::LookupLinkedForms(RE::TESDataHandler* dataHandler, INI::LinkedFormsConfig& rawLinkedForms)
	{
		ForEachLinkedForms([&]<typename Form>(LinkedForms<Form>& forms) {
			forms.LookupForms(dataHandler, rawLinkedForms[forms.GetType()]);
		});

		auto& genericForms = rawLinkedForms[RECORD::kForm];
		for (auto& rawForm : genericForms) {
			if (auto form = detail::LookupLinkedForm(dataHandler, rawForm); form) {
				auto& [formID, scope, parentFormIDs, idxOrCount, chance, path] = rawForm;
				FormVec parentForms{};
				if (!Forms::detail::formID_to_form(dataHandler, parentFormIDs.MATCH, parentForms, path, false, false)) {
					continue;
				}
				// Add to appropriate list. (Note that type inferring doesn't recognize SleepOutfit, Skin or DeathItems)
				if (const auto keyword = form->As<RE::BGSKeyword>(); keyword) {
					keywords.Link(keyword, scope, parentForms, idxOrCount, chance, path);
				} else if (const auto spell = form->As<RE::SpellItem>(); spell) {
					spells.Link(spell, scope, parentForms, idxOrCount, chance, path);
				} else if (const auto levSpell = form->As<RE::TESLevSpell>(); levSpell) {
					levSpells.Link(levSpell, scope, parentForms, idxOrCount, chance, path);
				} else if (const auto perk = form->As<RE::BGSPerk>(); perk) {
					perks.Link(perk, scope, parentForms, idxOrCount, chance, path);
				} else if (const auto shout = form->As<RE::TESShout>(); shout) {
					shouts.Link(shout, scope, parentForms, idxOrCount, chance, path);
				} else if (const auto item = form->As<RE::TESBoundObject>(); item) {
					items.Link(item, scope, parentForms, idxOrCount, chance, path);
				} else if (const auto outfit = form->As<RE::BGSOutfit>(); outfit) {
					outfits.Link(outfit, scope, parentForms, idxOrCount, chance, path);
				} else if (const auto faction = form->As<RE::TESFaction>(); faction) {
					factions.Link(faction, scope, parentForms, idxOrCount, chance, path);
				} else {
					auto type = form->GetFormType();
					if (type == RE::FormType::Package || type == RE::FormType::FormList) {
						// With generic Form entries we default to RandomCount, so we need to properly convert it to Index if it turned out to be a package.
						Index packageIndex = 1;
						if (std::holds_alternative<RandomCount>(idxOrCount)) {
							auto& count = std::get<RandomCount>(idxOrCount);
							if (!count.IsExact()) {
								logger::warn("\t[{}] Inferred Form is a Package, but specifies a random count instead of index. Min value ({}) of the range will be used as an index.", path, count.min);
							}
							packageIndex = count.min;
						} else {
							packageIndex = std::get<Index>(idxOrCount);
						}
						packages.Link(form, scope, parentForms, packageIndex, chance, path);
					} else {
						logger::warn("\t[{}] Unsupported Form type: {}", path, type);
					}
				}
			}
		}

		// Remove empty linked forms
		ForEachLinkedForms([&]<typename Form>(LinkedForms<Form>& forms) {
			std::erase_if(forms.forms, [](const auto& pair) { return pair.second.empty(); });
		});

		// Clear INI once lookup is done
		rawLinkedForms.clear();

		// Clear logger's buffer to free some memory :)
		buffered_logger::clear();
	}

	void Manager::LogLinkedFormsLookup()
	{
		logger::info("{:*^50}", "LINKED ITEMS");

		ForEachLinkedForms([]<typename Form>(LinkedForms<Form>& linkedForms) {
			if (linkedForms.GetForms().empty()) {
				return;
			}

			std::unordered_map<RE::TESForm*, std::vector<DistributedForm>> map{};

			// Iterate through the original map
			for (const auto& pair : linkedForms.GetForms()) {
				const auto& path = pair.first;
				const auto& formsMap = pair.second;

				for (const auto& pair : formsMap) {
					const auto  key = pair.first;
					const auto& values = pair.second;
					for (const auto& value : values) {
						map[value.form].emplace_back(key, path);
					}
				}
			}

			const auto& recordName = RECORD::GetTypeName(linkedForms.GetType());
			logger::info("Linked {}s: ", recordName);

			for (const auto& [form, linkedForms] : map) {
				logger::info("\t{}", describe(form));

				const auto lastItemIndex = linkedForms.size() - 1;
				for (int i = 0; i < lastItemIndex; ++i) {
					const auto& linkedItem = linkedForms[i];
					logger::info("\t├─── {}", describe(linkedItem));
				}
				const auto& lastLinkedItem = linkedForms[lastItemIndex];
				logger::info("\t└─── {}", describe(lastLinkedItem));
			}
		});
	}
#pragma endregion

#pragma region Distribution
	void Manager::ForEachLinkedDistributionSet(const DistributedForms& targetForms, Scope scope, std::function<void(DistributionSet&)> performDistribution)
	{
		for (const auto& form : targetForms) {
			auto& linkedSpells = LinkedFormsForForm(form, scope, spells);
			auto& linkedPerks = LinkedFormsForForm(form, scope, perks);
			auto& linkedItems = LinkedFormsForForm(form, scope, items);
			auto& linkedShouts = LinkedFormsForForm(form, scope, shouts);
			auto& linkedLevSpells = LinkedFormsForForm(form, scope, levSpells);
			auto& linkedPackages = LinkedFormsForForm(form, scope, packages);
			auto& linkedOutfits = LinkedFormsForForm(form, scope, outfits);
			auto& linkedKeywords = LinkedFormsForForm(form, scope, keywords);
			auto& linkedFactions = LinkedFormsForForm(form, scope, factions);
			auto& linkedSleepOutfits = LinkedFormsForForm(form, scope, sleepOutfits);
			auto& linkedSkins = LinkedFormsForForm(form, scope, skins);

			DistributionSet linkedEntries{
				linkedSpells,
				linkedPerks,
				linkedItems,
				linkedShouts,
				linkedLevSpells,
				linkedPackages,
				linkedOutfits,
				linkedKeywords,
				DistributionSet::empty<RE::TESBoundObject>(),  // deathItems are distributed only on death :) as such, linked items are also distributed only on death.
				linkedFactions,
				linkedSleepOutfits,
				linkedSkins
			};

			if (linkedEntries.IsEmpty()) {
				continue;
			}

			performDistribution(linkedEntries);
		}
	}

	void Manager::ForEachLinkedDistributionSet(const DistributedForms& targetForms, std::function<void(DistributionSet&)> performDistribution)
	{
		ForEachLinkedDistributionSet(targetForms, Scope::kLocal, performDistribution);
		ForEachLinkedDistributionSet(targetForms, Scope::kGlobal, performDistribution);
	}

	void Manager::ForEachLinkedDeathDistributionSet(const DistributedForms& targetForms, Scope scope, std::function<void(DistributionSet&)> performDistribution)
	{
		for (const auto& form : targetForms) {
			auto& linkedDeathItems = LinkedFormsForForm(form, scope, deathItems);

			DistributionSet linkedEntries{
				DistributionSet::empty<RE::SpellItem>(),
				DistributionSet::empty<RE::BGSPerk>(),
				DistributionSet::empty<RE::TESBoundObject>(),
				DistributionSet::empty<RE::TESShout>(),
				DistributionSet::empty<RE::TESLevSpell>(),
				DistributionSet::empty<RE::TESForm>(),
				DistributionSet::empty<RE::BGSOutfit>(),
				DistributionSet::empty<RE::BGSKeyword>(),
				linkedDeathItems,
				DistributionSet::empty<RE::TESFaction>(),
				DistributionSet::empty<RE::BGSOutfit>(),
				DistributionSet::empty<RE::TESObjectARMO>()
			};

			if (linkedEntries.IsEmpty()) {
				continue;
			}

			performDistribution(linkedEntries);
		}
	}

	void Manager::ForEachLinkedDeathDistributionSet(const DistributedForms& targetForms, std::function<void(DistributionSet&)> performDistribution)
	{
		ForEachLinkedDeathDistributionSet(targetForms, Scope::kLocal, performDistribution);
		ForEachLinkedDeathDistributionSet(targetForms, Scope::kGlobal, performDistribution);
	}
#pragma endregion
}
