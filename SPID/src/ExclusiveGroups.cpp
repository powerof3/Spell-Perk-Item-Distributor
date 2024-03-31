#include "ExclusiveGroups.h"
#include "FormData.h"

namespace ExclusiveGroups
{
	bool INI::TryParse(const std::string& a_key, const std::string& a_value, const Path& a_path)
	{
		if (a_key != "ExclusiveGroup") {
			return false;
		}

		const auto sections = string::split(a_value, "|");
		const auto size = sections.size();

		if (size < 2) {
			logger::warn("IGNORED: ExclusiveGroup must have a name and at least one Form Filter: {} = {}"sv, a_key, a_value);
			return true;
		}

		auto split_IDs = distribution::split_entry(sections[1]);

		if (split_IDs.empty()) {
			logger::warn("ExclusiveGroup must have at least one Form Filter : {} = {}"sv, a_key, a_value);
			return true;
		}

		RawExclusiveGroup group{};
		group.name = sections[0];
		group.path = a_path;

		for (auto& IDs : split_IDs) {
			if (IDs.at(0) == '-') {
				IDs.erase(0, 1);
				group.formIDs.NOT.push_back(distribution::get_record(IDs));
			} else {
				group.formIDs.MATCH.push_back(distribution::get_record(IDs));
			}
		}

		exclusiveGroups.emplace_back(group);

		return true;
	}

	void Manager::LookupExclusiveGroups(RE::TESDataHandler* const dataHandler, INI::ExclusiveGroupsVec& exclusiveGroups)
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

	void Manager::LogExclusiveGroupsLookup()
	{
		if (groups.empty()) {
			return;
		}

		logger::info("{:*^50}", "EXCLUSIVE GROUPS");

		for (const auto& [group, forms] : groups) {
			logger::info("Adding '{}' exclusive group", group);
			for (const auto& form : forms) {
				logger::info("  {}", describe(form));
			}
		}
	}

	std::unordered_set<RE::TESForm*> Manager::MutuallyExclusiveFormsForForm(RE::TESForm* form) const
	{
		std::unordered_set<RE::TESForm*> forms{};
		if (auto it = linkedGroups.find(form); it != linkedGroups.end()) {
			std::ranges::for_each(it->second, [&](const Group& name) {
				const auto& group = groups.at(name);
				forms.insert(group.begin(), group.end());
			});
		}

		// Remove self from the list.
		forms.erase(form);

		return forms;
	}

	const GroupFormsMap& ExclusiveGroups::Manager::GetGroups() const
	{
		return groups;
	}
}
