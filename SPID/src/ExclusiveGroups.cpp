#include "ExclusiveGroups.h"
#include "FormData.h"
#include "LookupConfigs.h"
#include "Parser.h"

namespace ExclusiveGroups
{
	namespace concepts
	{
		template <typename Data>
		concept named_data = requires(Data data) {
			{
				data.name
			} -> std::same_as<std::string&>;
			{
				data.name = std::declval<std::string>()
			};
		};
	}

	using namespace concepts;
	using namespace Distribution::INI;

	struct ExclusiveGroupKeyComponentParser
	{
		template <typename Data>
		bool operator()(const std::string& key, Data& data) const
		{
			return key == "ExclusiveGroup";
		}
	};

	struct ExclusiveGroupNameComponentParser
	{
		template <named_data Data>
		void operator()(const std::string& entry, Data& data) const
		{
			data.name = entry;
		}
	};

	bool INI::TryParse(const std::string& key, const std::string& value, const Path& path)
	{
		try {
			if (auto optData = Parse<RawExclusiveGroup,
					ExclusiveGroupKeyComponentParser,
					ExclusiveGroupNameComponentParser,
					FormFiltersComponentParser<kRequired | kAllowExclusionModifier>>(key, value);
				optData) {
				auto& data = *optData;
				data.path = path;
				exclusiveGroups.emplace_back(data);
			} else {
				return false;
			}
		} catch (const Exception::MissingComponentParserException& e) {
			logger::warn("\t'{} = {}'"sv, key, value);
			logger::warn("\t\tSKIPPED: Exclusive Group must have a name and at least one Form"sv);
		} catch (const std::exception& e) {
			logger::warn("\t'{} = {}'"sv, key, value);
			logger::warn("\t\tSKIPPED: {}"sv, e.what());
		}
		return true;
	}

	void Manager::LookupExclusiveGroups(RE::TESDataHandler* const dataHandler, INI::ExclusiveGroupsVec& exclusiveGroups)
	{
		groups.clear();
		mutuallyExclusiveForms.clear();

		for (auto& [name, filterIDs, path] : exclusiveGroups) {
			auto&   forms = groups[name];
			FormVec match{};
			FormVec formsNot{};
			if (Forms::detail::formID_to_form(dataHandler, filterIDs.MATCH, match, path, Forms::LookupOptions::kNone) &&
				Forms::detail::formID_to_form(dataHandler, filterIDs.NOT, formsNot, path, Forms::LookupOptions::kNone)) {
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

		BuildMutuallyExclusiveForms();
	}

	void Manager::LogExclusiveGroupsLookup()
	{
		if (groups.empty()) {
			return;
		}

		LOG_HEADER("EXCLUSIVE GROUPS");

		for (const auto& [group, forms] : groups) {
			logger::info("Adding '{}' exclusive group", group);
			for (const auto& form : forms) {
				logger::info("  {}", describe(form));
			}
		}
	}

	std::span<RE::TESForm* const> Manager::MutuallyExclusiveFormsForForm(RE::TESForm* form) const
	{
		if (const auto it = mutuallyExclusiveForms.find(form); it != mutuallyExclusiveForms.end()) {
			return it->second;
		}
		return {};
	}

	void Manager::BuildMutuallyExclusiveForms()
	{
		for (const auto& [name, forms] : groups) {
			for (auto form : forms) {
				auto& excludedForms = mutuallyExclusiveForms[form];
				excludedForms.insert(excludedForms.end(), forms.begin(), forms.end());
			}
		}

		for (auto& [form, excludedForms] : mutuallyExclusiveForms) {
			std::erase(excludedForms, form);
			std::ranges::sort(excludedForms, std::less<>{});
			const auto duplicates = std::ranges::unique(excludedForms);
			excludedForms.erase(duplicates.begin(), duplicates.end());
			excludedForms.shrink_to_fit();
		}
	}

	const GroupFormsMap& ExclusiveGroups::Manager::GetGroups() const
	{
		return groups;
	}
}
