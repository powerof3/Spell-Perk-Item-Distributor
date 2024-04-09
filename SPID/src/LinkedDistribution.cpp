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

			DistributionType distributionType = kRegular;

			if (key.starts_with("Global"sv)) {
				scope = kGlobal;
				key.erase(0, 6);
			}

			if (!key.starts_with("Linked"sv)) {
				return false;
			}

			key.erase(0, 6);

			if (key.starts_with("Death"sv)) {
				distributionType = kDeath;
				key.erase(0, 5);
			}

			std::string rawType = key;
			auto        type = RECORD::GetType(rawType);

			if (type == RECORD::kTotal) {
				logger::warn("IGNORED: Unsupported Linked Form type ({}): {} = {}"sv, rawType, originalKey, value);
				return true;
			}

			const auto sections = string::split(value, "|");
			const auto size = sections.size();

			if (size <= kRequired) {
				logger::warn("IGNORED: Linked Form must have a form and at least one Form Filter: {} = {}"sv, originalKey, value);
				return true;
			}

			auto split_IDs = distribution::split_entry(sections[kLinkedForms]);

			if (split_IDs.empty()) {
				logger::warn("IGNORED: Linked Form must have at least one parent Form to link to: {} = {}"sv, originalKey, value);
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

			linkedForms[distributionType][type].push_back(item);

			return true;
		}
	}
#pragma endregion

#pragma region Lookup

	void Manager::LookupLinkedForms(RE::TESDataHandler* const dataHandler, DistributionType distributionType, INI::LinkedFormsConfig& rawLinkedForms)
	{
		ForEachLinkedForms([&]<class Form>(LinkedForms<Form>& forms) {
			// If it's spells distributable we want to manually lookup forms to pick LevSpells that are added into the list.
			if constexpr (!std::is_same_v<Form, RE::SpellItem>) {
				forms.LookupForms(dataHandler, distributionType, rawLinkedForms[distributionType][forms.GetType()]);
			}
		});

		// Sort out Spells and Leveled Spells into two separate lists.
		auto& rawSpells = rawLinkedForms[distributionType][RECORD::kSpell];

		for (auto& rawSpell : rawSpells) {
			if (auto form = detail::LookupLinkedForm(dataHandler, rawSpell); form) {
				auto& [formID, scope, parentFormIDs, idxOrCount, chance, path] = rawSpell;
				FormVec parentForms{};
				if (!Forms::detail::formID_to_form(dataHandler, parentFormIDs.MATCH, parentForms, path, LookupOptions::kNone)) {
					continue;
				}
				if (const auto spell = form->As<RE::SpellItem>(); spell) {
					spells.Link(spell, scope, distributionType, parentForms, idxOrCount, chance, path);
				} else if (const auto levSpell = form->As<RE::TESLevSpell>(); levSpell) {
					levSpells.Link(levSpell, scope, distributionType, parentForms, idxOrCount, chance, path);
				}
			}
		}

		auto& genericForms = rawLinkedForms[distributionType][RECORD::kForm];
		for (auto& rawForm : genericForms) {
			if (auto form = detail::LookupLinkedForm(dataHandler, rawForm); form) {
				auto& [formID, scope, parentFormIDs, idxOrCount, chance, path] = rawForm;
				FormVec parentForms{};
				if (!Forms::detail::formID_to_form(dataHandler, parentFormIDs.MATCH, parentForms, path, LookupOptions::kNone)) {
					continue;
				}
				// Add to appropriate list. (Note that type inferring doesn't recognize SleepOutfit, Skin or DeathItems)
				if (const auto keyword = form->As<RE::BGSKeyword>(); keyword) {
					keywords.Link(keyword, scope, distributionType, parentForms, idxOrCount, chance, path);
				} else if (const auto spell = form->As<RE::SpellItem>(); spell) {
					spells.Link(spell, scope, distributionType, parentForms, idxOrCount, chance, path);
				} else if (const auto levSpell = form->As<RE::TESLevSpell>(); levSpell) {
					levSpells.Link(levSpell, scope, distributionType, parentForms, idxOrCount, chance, path);
				} else if (const auto perk = form->As<RE::BGSPerk>(); perk) {
					perks.Link(perk, scope, distributionType, parentForms, idxOrCount, chance, path);
				} else if (const auto shout = form->As<RE::TESShout>(); shout) {
					shouts.Link(shout, scope, distributionType, parentForms, idxOrCount, chance, path);
				} else if (const auto item = form->As<RE::TESBoundObject>(); item) {
					items.Link(item, scope, distributionType, parentForms, idxOrCount, chance, path);
				} else if (const auto outfit = form->As<RE::BGSOutfit>(); outfit) {
					outfits.Link(outfit, scope, distributionType, parentForms, idxOrCount, chance, path);
				} else if (const auto faction = form->As<RE::TESFaction>(); faction) {
					factions.Link(faction, scope, distributionType, parentForms, idxOrCount, chance, path);
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
						packages.Link(form, scope, distributionType, parentForms, packageIndex, chance, path);
					} else {
						logger::warn("\t[{}] Unsupported Form type: {}", path, type);
					}
				}
			}
		}

		// Remove empty linked forms
		ForEachLinkedForms([&]<typename Form>(LinkedForms<Form>& forms) {
			std::erase_if(forms.forms[distributionType], [](const auto& pair) { return pair.second.empty(); });
		});
	}

	void Manager::LookupLinkedForms(RE::TESDataHandler* dataHandler, INI::LinkedFormsConfig& rawLinkedForms)
	{
		LookupLinkedForms(dataHandler, kRegular, rawLinkedForms);
		LookupLinkedForms(dataHandler, kDeath, rawLinkedForms);

		// Clear INI once lookup is done
		rawLinkedForms.clear();

		// Clear logger's buffer to free some memory :)
		buffered_logger::clear();
	}

	void Manager::LogLinkedFormsLookup(DistributionType type)
	{
		ForEachLinkedForms([&]<typename Form>(LinkedForms<Form>& linkedForms) {
			if (linkedForms.forms[type].empty()) {
				return;
			}
			std::unordered_map<RE::TESForm*, std::vector<DistributedForm>> map{};

			// Iterate through the original map
			for (const auto& pair : linkedForms.forms[type]) {
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

	void Manager::LogLinkedFormsLookup()
	{
		if (!IsEmpty(kRegular)) {
			logger::info("{:*^50}", "LINKED FORMS");

			LogLinkedFormsLookup(kRegular);
		}

		if (!IsEmpty(kDeath)) {
			logger::info("{:*^50}", "LINKED ON DEATH FORMS");

			LogLinkedFormsLookup(kDeath);
		}
	}
#pragma endregion

#pragma region Distribution
	void Manager::ForEachLinkedDistributionSet(DistributionType type, const DistributedForms& targetForms, Scope scope, std::function<void(DistributionSet&)> performDistribution)
	{
		for (const auto& form : targetForms) {
			DistributionSet linkedEntries{
				LinkedFormsForForm(type, form, scope, spells),
				LinkedFormsForForm(type, form, scope, perks),
				LinkedFormsForForm(type, form, scope, items),
				LinkedFormsForForm(type, form, scope, shouts),
				LinkedFormsForForm(type, form, scope, levSpells),
				LinkedFormsForForm(type, form, scope, packages),
				LinkedFormsForForm(type, form, scope, outfits),
				LinkedFormsForForm(type, form, scope, keywords),
				LinkedFormsForForm(type, form, scope, factions),
				LinkedFormsForForm(type, form, scope, sleepOutfits),
				LinkedFormsForForm(type, form, scope, skins)
			};

			if (linkedEntries.IsEmpty()) {
				continue;
			}

			performDistribution(linkedEntries);
		}
	}

	void Manager::ForEachLinkedDistributionSet(DistributionType type, const DistributedForms& targetForms, std::function<void(DistributionSet&)> performDistribution)
	{
		ForEachLinkedDistributionSet(type, targetForms, Scope::kLocal, performDistribution);
		ForEachLinkedDistributionSet(type, targetForms, Scope::kGlobal, performDistribution);
	}

	bool Manager::IsEmpty(DistributionType type) const
	{
		return false;
	}
#pragma endregion
}
