#include "LinkedDistribution.h"
#include "FormData.h"

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
			kCount,
			kChance,

			// Minimum required sections
			kRequired = kLinkedForms
		};

		bool TryParse(const std::string& a_key, const std::string& a_value, const std::string& a_path)
		{
			if (a_key != "LinkedItem") {
				return false;
			}

			const auto sections = string::split(a_value, "|");
			const auto size = sections.size();

			if (size <= kRequired) {
				logger::warn("IGNORED: LinkedItem must have a form and at least one Form Filter: {} = {}"sv, a_key, a_value);
				return false;
			}

			auto split_IDs = distribution::split_entry(sections[kLinkedForms]);

			if (split_IDs.empty()) {
				logger::warn("IGNORED: LinkedItem must have at least one Form Filter : {} = {}"sv, a_key, a_value);
				return false;
			}
			
			INI::RawLinkedItem item{};
			item.rawForm = distribution::get_record(sections[kForm]);
			item.path = a_path;

			for (auto& IDs : split_IDs) {
				item.rawFormFilters.MATCH.push_back(distribution::get_record(IDs));
			}

			if (kCount < size) {
				if (const auto& str = sections[kCount]; distribution::is_valid_entry(str)) {
					if (auto countPair = string::split(str, "-"); countPair.size() > 1) {
						auto minCount = string::to_num<Count>(countPair[0]);
						auto maxCount = string::to_num<Count>(countPair[1]);

						item.count = RandomCount(minCount, maxCount);
					} else {
						auto count = string::to_num<Count>(str);

						item.count = RandomCount(count, count);  // create the exact match range.
					}
				}
			}

			if (kChance < size) {
				if (const auto& str = sections[kChance]; distribution::is_valid_entry(str)) {
					item.chance = string::to_num<Chance>(str);
				}
			}

			linkedItems.push_back(item);

			return true;
		}
	}
#pragma endregion

#pragma region Lookup

	void Manager::LookupLinkedItems(RE::TESDataHandler* const dataHandler, INI::LinkedItemsVec& rawLinkedItems)
	{
		using namespace Forms::Lookup;

		for (auto& [formOrEditorID, linkedIDs, count, chance, path] : rawLinkedItems) {
			try {
				auto    form = detail::get_form(dataHandler, formOrEditorID, path);
				FormVec match{};
				if (!detail::formID_to_form(dataHandler, linkedIDs.MATCH, match, path, false, false)) {
					continue;
				}
				// Add to appropriate list.
				if (const auto keyword = form->As<RE::BGSKeyword>(); keyword) {
					keywords.Link(keyword, match, count, chance, path);
				} else if (const auto spell = form->As<RE::SpellItem>(); spell) {
					spells.Link(spell, match, count, chance, path);
				} else if (const auto perk = form->As<RE::BGSPerk>(); perk) {
					perks.Link(perk, match, count, chance, path);
				} else if (const auto shout = form->As<RE::TESShout>(); shout) {
					shouts.Link(shout, match, count, chance, path);
				} else if (const auto item = form->As<RE::TESBoundObject>(); item) {
					items.Link(item, match, count, chance, path);
				} else if (const auto outfit = form->As<RE::BGSOutfit>(); outfit) {
					outfits.Link(outfit, match, count, chance, path);
				} else if (const auto faction = form->As<RE::TESFaction>(); faction) {
					factions.Link(faction, match, count, chance, path);
				} else if (const auto skin = form->As<RE::TESObjectARMO>(); skin) {
					skins.Link(skin, match, count, chance, path);
				} else if (const auto package = form->As<RE::TESForm>(); package) {
					auto type = package->GetFormType();
					if (type == RE::FormType::Package || type == RE::FormType::FormList)
						packages.Link(package, match, count, chance, path);
				}
			} catch (const UnknownFormIDException& e) {
				buffered_logger::error("\t\t[{}] LinkedItem [0x{:X}] ({}) SKIP - formID doesn't exist", e.path, e.formID, e.modName.value_or(""));
			} catch (const UnknownPluginException& e) {
				buffered_logger::error("\t\t[{}] LinkedItem ({}) SKIP - mod cannot be found", e.path, e.modName);
			} catch (const InvalidKeywordException& e) {
				buffered_logger::error("\t\t[{}] LinkedItem [0x{:X}] ({}) SKIP - keyword does not have a valid editorID", e.path, e.formID, e.modName.value_or(""));
			} catch (const KeywordNotFoundException& e) {
				if (e.isDynamic) {
					buffered_logger::critical("\t\t[{}] LinkedItem {} FAIL - couldn't create keyword", e.path, e.editorID);
				} else {
					buffered_logger::critical("\t\t[{}] LinkedItem {} FAIL - couldn't get existing keyword", e.path, e.editorID);
				}
			} catch (const UnknownEditorIDException& e) {
				buffered_logger::error("\t\t[{}] LinkedItem ({}) SKIP - editorID doesn't exist", e.path, e.editorID);
			} catch (const MalformedEditorIDException& e) {
				buffered_logger::error("\t\t[{}] LinkedItem (\"\") SKIP - malformed editorID", e.path);
			} catch (const InvalidFormTypeException& e) {
				std::visit(overload{
							   [&](const FormModPair& formMod) {
								   auto& [formID, modName] = formMod;
								   buffered_logger::error("\t\t[{}] LinkedItem [0x{:X}] ({}) SKIP - unsupported form type ({})", e.path, *formID, modName.value_or(""), RE::FormTypeToString(e.formType));
							   },
							   [&](std::string editorID) {
								   buffered_logger::error("\t\t[{}] LinkedItem ({}) SKIP - unsupported form type ({})", e.path, editorID, RE::FormTypeToString(e.formType));
							   } },
					e.formOrEditorID);
			}
		}

		// Remove empty linked forms
		ForEachLinkedForms([&]<typename Form>(LinkedForms<Form>& forms) {
			std::erase_if(forms.forms, [](const auto& pair) { return pair.second.empty(); });
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
				DistributionSet::empty<RE::TESBoundObject>(), // deathItems can't be linked at the moment (only makes sense on death)
				linkedFactions,
				DistributionSet::empty<RE::BGSOutfit>(), // sleeping outfits are not supported for now due to lack of support in config's syntax.
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
