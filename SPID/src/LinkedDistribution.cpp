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

		bool TryParse(const std::string& a_key, const std::string& a_value, const std::string& a_path)
		{
			if (!a_key.starts_with("Linked"sv)) {
				return false;
			}
			
			std::string rawType = a_key.substr(6);
			auto type = RECORD::GetType(rawType);
			if (type == RECORD::kTotal) {
				logger::warn("IGNORED: Invalid Linked Form type: {}"sv, rawType);
				return true;
			}

			const auto sections = string::split(a_value, "|");
			const auto size = sections.size();

			if (size <= kRequired) {
				logger::warn("IGNORED: LinkedItem must have a form and at least one Form Filter: {} = {}"sv, a_key, a_value);
				return true;
			}

			auto split_IDs = distribution::split_entry(sections[kLinkedForms]);

			if (split_IDs.empty()) {
				logger::warn("IGNORED: LinkedItem must have at least one Form Filter : {} = {}"sv, a_key, a_value);
				return true;
			}

			INI::RawLinkedForm item{};
			item.formOrEditorID = distribution::get_record(sections[kForm]);
			item.path = a_path;

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
					item.chance = string::to_num<Chance>(str);
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
				auto& [formID, parentFormIDs, idxOrCount, chance, path] = rawForm;
				FormVec parentForms{};
				if (!Forms::detail::formID_to_form(dataHandler, parentFormIDs.MATCH, parentForms, path, false, false)) {
					continue;
				}
				// Add to appropriate list. (Note that type inferring doesn't recognize SleepingOutfit or DeathItems)
				if (const auto keyword = form->As<RE::BGSKeyword>(); keyword) {
					keywords.Link(keyword, parentForms, idxOrCount, chance, path);
				} else if (const auto spell = form->As<RE::SpellItem>(); spell) {
					spells.Link(spell, parentForms, idxOrCount, chance, path);
				} else if (const auto perk = form->As<RE::BGSPerk>(); perk) {
					perks.Link(perk, parentForms, idxOrCount, chance, path);
				} else if (const auto shout = form->As<RE::TESShout>(); shout) {
					shouts.Link(shout, parentForms, idxOrCount, chance, path);
				} else if (const auto item = form->As<RE::TESBoundObject>(); item) {
					items.Link(item, parentForms, idxOrCount, chance, path);
				} else if (const auto outfit = form->As<RE::BGSOutfit>(); outfit) {
					outfits.Link(outfit, parentForms, idxOrCount, chance, path);
				} else if (const auto faction = form->As<RE::TESFaction>(); faction) {
					factions.Link(faction, parentForms, idxOrCount, chance, path);
				} else if (const auto skin = form->As<RE::TESObjectARMO>(); skin) {
					skins.Link(skin, parentForms, idxOrCount, chance, path);
				} else if (const auto package = form->As<RE::TESForm>(); package) {
					auto type = package->GetFormType();
					if (type == RE::FormType::Package || type == RE::FormType::FormList) {
						// During type inferring we'll default to RandomCount, so we need to properly convert it to Index if it's a package.
						Index packageIndex = 1;
						if (std::holds_alternative<RandomCount>(idxOrCount)) {
							auto& count = std::get<RandomCount>(idxOrCount);
							if (!count.IsExact()) {
								logger::warn("Inferred Form is a package, but specifies a random count instead of index. Min value ({}) of the range will be used as an index.", count.min);
							}
							packageIndex = count.min;
						} else {
							packageIndex = std::get<Index>(idxOrCount);
						}
						packages.Link(package, parentForms, packageIndex, chance, path);
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

			std::unordered_map<RE::TESForm*, std::vector<RE::TESForm*>> map{};

			// Iterate through the original map
			for (const auto& pair : linkedForms.GetForms()) {
				const auto           key = pair.first;
				const DataVec<Form>& values = pair.second;

				for (const auto& value : values) {
					map[value.form].push_back(key);
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

	void Manager::ForEachLinkedDistributionSet(const std::set<RE::TESForm*>& targetForms, std::function<void(DistributionSet&)> performDistribution)
	{
		for (const auto form : targetForms) {
			auto& linkedSpells = LinkedFormsForForm(form, spells);
			auto& linkedPerks = LinkedFormsForForm(form, perks);
			auto& linkedItems = LinkedFormsForForm(form, items);
			auto& linkedShouts = LinkedFormsForForm(form, shouts);
			auto& linkedLevSpells = LinkedFormsForForm(form, levSpells);
			auto& linkedPackages = LinkedFormsForForm(form, packages);
			auto& linkedOutfits = LinkedFormsForForm(form, outfits);
			auto& linkedKeywords = LinkedFormsForForm(form, keywords);
			auto& linkedFactions = LinkedFormsForForm(form, factions);
			auto& linkedSkins = LinkedFormsForForm(form, skins);

			DistributionSet linkedEntries{
				linkedSpells,
				linkedPerks,
				linkedItems,
				linkedShouts,
				linkedLevSpells,
				linkedPackages,
				linkedOutfits,
				linkedKeywords,
				DistributionSet::empty<RE::TESBoundObject>(),  // deathItems can't be linked at the moment (only makes sense on death)
				linkedFactions,
				DistributionSet::empty<RE::BGSOutfit>(),  // sleeping outfits are not supported for now due to lack of support in config's syntax.
				linkedSkins
			};

			if (linkedEntries.IsEmpty()) {
				continue;
			}

			performDistribution(linkedEntries);
		}
	}

#pragma endregion
}
