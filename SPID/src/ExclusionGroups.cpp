#include "ExclusionGroups.h"
#include "FormData.h"

void Exclusion::Manager::LookupExclusions(RE::TESDataHandler* const dataHandler, INI::ExclusionsVec& exclusions)
{
	groups.clear();
	linkedGroups.clear();

	for (auto& [name, filterIDs, path] : exclusions) {
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
	for (auto& [name, forms] : groups) {
		if (forms.empty()) {
			groups.erase(name);
		}
	}

	for (auto& [name, forms] : groups) {
		for (auto& form : forms) {
			linkedGroups[form].insert(name);
		}
	}
}

std::unordered_set<RE::TESForm*> Exclusion::Manager::MutuallyExclusiveFormsForForm(RE::TESForm* form) const
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

const Exclusion::Groups& Exclusion::Manager::GetGroups() const
{
	return groups;
}
