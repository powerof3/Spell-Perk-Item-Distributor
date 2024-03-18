#include "LinkedDistribution.h"
#include "FormData.h"

#pragma region Parsing
bool           LinkedDistribution::INI::Parser::TryParse(const std::string& a_key, const std::string& a_value, const std::string& a_path)
{
	if (a_key != "LinkedItem") {
		return false;
	}

	const auto sections = string::split(a_value, "|");
	const auto size = sections.size();

	if (size < 2) {
		logger::warn("IGNORED: LinkedItem must have a form and at least one filter name: {} = {}"sv, a_key, a_value);
		return false;
	}

	auto split_IDs = distribution::split_entry(sections[1]);

	if (split_IDs.empty()) {
		logger::warn("ExclusiveGroup must have at least one Form Filter : {} = {}"sv, a_key, a_value);
		return false;
	}

	LinkedDistribution::INI::RawLinkedItem item{};
	item.rawForm = sections[0];
	item.path = a_path;

	for (auto& IDs : split_IDs) {
		item.rawFormFilters.MATCH.push_back(distribution::get_record(IDs));
	}
}
#pragma endregion

#pragma region Lookup
void           LinkedDistribution::Manager::LookupLinkedItems(RE::TESDataHandler* const dataHandler, INI::LinkedItemsVec& rawLinkedItems)
{
	using namespace Forms;

	// TODO: Figure out templates here.

	for (auto& [rawForm, filterIDs, count, chance, path] : rawLinkedItems) {
		try {
			if (const auto form = Forms::detail::get_form(dataHandler, rawForm, path); form) {
				/*auto& forms = linkedForms[form];
				FormVec match{};

				if (Forms::detail::formID_to_form(dataHandler, filterIDs.MATCH, match, path, false, false)) {
					for (const auto& form : match) {
						if (std::holds_alternative<RE::TESForm*>(form)) {
							forms.insert(std::get<RE::TESForm*>(form));
						}
					}
				}*/
			}
		} catch (const Lookup::UnknownFormIDException& e) {
			buffered_logger::error("\t[{}] [0x{:X}] ({}) FAIL - formID doesn't exist", e.path, e.formID, e.modName.value_or(""));
		} catch (const Lookup::InvalidKeywordException& e) {
			buffered_logger::error("\t[{}] [0x{:X}] ({}) FAIL - keyword does not have a valid editorID", e.path, e.formID, e.modName.value_or(""));
		} catch (const Lookup::KeywordNotFoundException& e) {
			if (e.isDynamic) {
				buffered_logger::critical("\t[{}] {} FAIL - couldn't create keyword", e.path, e.editorID);
			} else {
				buffered_logger::critical("\t[{}] {} FAIL - couldn't get existing keyword", e.path, e.editorID);
			}
		} catch (const Lookup::UnknownEditorIDException& e) {
			buffered_logger::error("\t[{}] ({}) FAIL - editorID doesn't exist", e.path, e.editorID);
		} catch (const Lookup::MalformedEditorIDException& e) {
			buffered_logger::error("\t[{}] FAIL - editorID can't be empty", e.path);
		} catch (const Lookup::InvalidFormTypeException& e) {
			// Whitelisting is disabled, so this should not occur
		} catch (const Lookup::UnknownPluginException& e) {
			// Likewise, we don't expect plugin names in linked forms.
		}
	}

	// Remove empty linked forms
	//std::erase_if(linkedForms, [](const auto& pair) { return pair.second.empty(); });
}
#pragma endregion
