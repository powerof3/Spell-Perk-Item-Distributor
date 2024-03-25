#include "ExclusiveGroups.h"
#include "FormData.h"

void ExclusiveGroups::Manager::LookupExclusiveGroups(RE::TESDataHandler* const dataHandler, INI::ExclusiveGroupsVec& exclusiveGroups)
{
	groups.clear();
	linkedGroups.clear();

	for (auto& [name, filterIDs, path] : exclusiveGroups) {
		auto&   forms = groups[name];
		FormVec match{};
		FormVec formsNot{};

		if (Forms::detail::formID_to_form(dataHandler, filterIDs.MATCH, match, path, false, false) &&
			Forms::detail::formID_to_form(dataHandler, filterIDs.NOT, formsNot, path, false, false)) {
			for (const auto& form : match) {
				if (std::holds_alternative<RE::TESForm*>(form)) {
					forms.insert(std::get<RE::TESForm*>(form));
				}
			}

			for (auto& form : formsNot) {
				if (std::holds_alternative<RE::TESForm*>(form)) {
					forms.erase(std::get<RE::TESForm*>(form));
				}
			}
		}
	}

	// Remove empty groups
	std::erase_if(groups, [](const auto& pair) { return pair.second.empty(); });

	for (auto& [name, forms] : groups) {
		for (auto& form : forms) {
			linkedGroups[form].insert(name);
		}
	}
}

std::unordered_set<RE::TESForm*> ExclusiveGroups::Manager::MutuallyExclusiveFormsForForm(RE::TESForm* form) const
{
	std::unordered_set<RE::TESForm*> forms{};
	if (auto it = linkedGroups.find(form); it != linkedGroups.end()) {
		std::ranges::for_each(it->second, [&](const GroupName& name) {
			const auto& group = groups.at(name);
			forms.insert(group.begin(), group.end());
		});
	}

	// Remove self from the list.
	forms.erase(form);

	return forms;
}

const ExclusiveGroups::Groups& ExclusiveGroups::Manager::GetGroups() const
{
	return groups;
}
