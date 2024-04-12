#include "LinkedDistribution.h"
#include "FormData.h"
#include "LookupConfigs.h"
#include "Parser.h"

namespace LinkedDistribution
{

	using namespace Forms;

#pragma region Parsing
	namespace INI
	{
		LinkedFormsConfig linkedConfigs{};

		namespace concepts
		{
			template <typename Data>
			concept linked_typed_data = Distribution::INI::concepts::typed_data<Data> && requires(Data data)
			{
				{
					data.scope
				} -> std::same_as<Scope&>;
				{
					data.scope = std::declval<Scope>()
				};
				{
					data.distributionType
				} -> std::same_as<DistributionType&>;
				{
					data.distributionType = std::declval<DistributionType>()
				};
			};
		}

		using namespace Distribution::INI;

		struct LinkedKeyComponentParser
		{
			template <concepts::linked_typed_data Data>
			bool operator()(const std::string& originalKey, Data& data) const
			{
				std::string key = originalKey;

				if (key.starts_with("Global"sv)) {
					data.scope = kGlobal;
					key.erase(0, 6);
				}

				if (!key.starts_with("Linked"sv)) {
					return false;
				}

				key.erase(0, 6);

				if (key.starts_with("Death"sv)) {
					data.distributionType = kDeath;
					key.erase(0, 5);
				}

				std::string rawType = key;
				auto        type = RECORD::GetType(rawType);

				if (type == RECORD::kTotal) {
					throw Exception::UnsupportedFormTypeException(rawType);
				}
				data.type = type;

				return true;
			}
		};

		bool TryParse(const std::string& key, const std::string& value, const Path& path)
		{
			try {
				if (auto optData = Parse<RawLinkedForm,
						LinkedKeyComponentParser,
						DistributableFormComponentParser,
						FormFiltersComponentParser<kRequired>,
						IndexOrCountComponentParser,
						ChanceComponentParser>(key, value);
					optData) {
					auto& data = *optData;
					data.path = path;

					linkedConfigs[data.type].push_back(data);
					return true;
				}
			} catch (const Exception::MissingComponentParserException& e) {
				logger::warn("SKIPPED: Linked Form must have a form and at least one Parent Form: {} = {}"sv, key, value);
			} catch (const std::exception& e) {
				logger::warn("SKIPPED {}", e.what());
			}
			return false;
		}
	}
#pragma endregion

#pragma region Lookup

	void Manager::LookupLinkedForms(RE::TESDataHandler* const dataHandler)
	{
		ForEachLinkedForms([&]<class Form>(LinkedForms<Form>& forms) {
			// If it's spells distributable we want to manually lookup forms to pick LevSpells that are added into the list.
			if constexpr (!std::is_same_v<Form, RE::SpellItem>) {
				forms.LookupForms(dataHandler, INI::linkedConfigs[forms.GetType()]);
			}
		});

		// Sort out Spells and Leveled Spells into two separate lists.
		auto& rawSpells = INI::linkedConfigs[RECORD::kSpell];

		for (auto& rawSpell : rawSpells) {
			if (auto form = detail::LookupLinkedForm(dataHandler, rawSpell); form) {
				auto& [formID, type, scope, distributionType, parentFormIDs, idxOrCount, chance, path] = rawSpell;
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

		auto& genericForms = INI::linkedConfigs[RECORD::kForm];
		for (auto& rawForm : genericForms) {
			if (auto form = detail::LookupLinkedForm(dataHandler, rawForm); form) {
				auto& [formID, type, scope, distributionType, parentFormIDs, idxOrCount, chance, path] = rawForm;
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
			std::erase_if(forms.forms[kRegular], [](const auto& pair) { return pair.second.empty(); });
			std::erase_if(forms.forms[kDeath], [](const auto& pair) { return pair.second.empty(); });
		});

		
		// Clear INI once lookup is done
		INI::linkedConfigs.clear();

		// Clear logger's buffer to free some memory :)
		buffered_logger::clear();
	}

	void Manager::LogLinkedFormsLookup(DistributionType type)
	{
		ForEachLinkedForms([&]<typename Form>(LinkedForms<Form>& linkedConfigs) {
			if (linkedConfigs.forms[type].empty()) {
				return;
			}
			std::unordered_map<RE::TESForm*, std::vector<DistributedForm>> map{};

			// Iterate through the original map
			for (const auto& [path, formsMap] : linkedConfigs.forms[type]) {
				for (const auto& [key, values] : formsMap) {
					for (const auto& value : values) {
						map[value.form].emplace_back(key, path);
					}
				}
			}

			const auto& recordName = RECORD::GetTypeName(linkedConfigs.GetType());
			logger::info("Linked {}s: ", recordName);

			for (const auto& [form, linkedConfigs] : map) {
				logger::info("\t{}", describe(form));

				const auto lastItemIndex = linkedConfigs.size() - 1;
				for (int i = 0; i < lastItemIndex; ++i) {
					const auto& linkedItem = linkedConfigs[i];
					logger::info("\t├─── {}", describe(linkedItem));
				}
				const auto& lastLinkedItem = linkedConfigs[lastItemIndex];
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
		return spells.IsEmpty(type) &&
		       perks.IsEmpty(type) &&
		       items.IsEmpty(type) &&
		       shouts.IsEmpty(type) &&
		       levSpells.IsEmpty(type) &&
		       packages.IsEmpty(type) &&
		       outfits.IsEmpty(type) &&
		       keywords.IsEmpty(type) &&
		       factions.IsEmpty(type) &&
		       sleepOutfits.IsEmpty(type) &&
		       skins.IsEmpty(type);
	}
#pragma endregion
}
