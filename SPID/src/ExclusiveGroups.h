#pragma once
#include <span>

#include "LookupConfigs.h"

namespace ExclusiveGroups
{
	namespace INI
	{
		struct RawExclusiveGroup
		{
			std::string name{};

			/// Raw filters in RawExclusiveGroup only use NOT and MATCH, there is no meaning for ALL, so it's ignored.
			RawFormFilters formFilters{};
			Path           path{};
		};

		using ExclusiveGroupsVec = std::vector<RawExclusiveGroup>;

		/// <summary>
		/// A list of RawExclusiveGroups that will be processed along with configs.
		/// </summary>
		inline ExclusiveGroupsVec exclusiveGroups{};

		bool TryParse(const std::string& a_key, const std::string& a_value, const Path& a_path);
	}

	using Group = std::string;
	using FormExclusionsMap = std::unordered_map<RE::TESForm*, std::vector<RE::TESForm*>>;
	using GroupFormsMap = std::unordered_map<Group, std::unordered_set<RE::TESForm*>>;

	class Manager : public ISingleton<Manager>
	{
	public:
		/// <summary>
		/// Does a forms lookup similar to what Filters do.
		///
		/// As a result this method configures Manager with discovered valid exclusive groups.
		/// </summary>
		/// <param name="dataHandler">A DataHandler that will perform the actual lookup.</param>
		/// <param name="rawExclusiveGroups">A raw exclusive group entries that should be processed.</param>
		void LookupExclusiveGroups(RE::TESDataHandler* const dataHandler, INI::ExclusiveGroupsVec& rawExclusiveGroups = INI::exclusiveGroups);

		void LogExclusiveGroupsLookup();

		/// <summary>
		/// Gets all forms that are in the same exclusive group as the given form (excluding the form itself).
		/// Note that a form can appear in multiple exclusive groups, so forms from all matching groups are returned.
		/// </summary>
		/// <param name="form">A form for which mutually exclusive forms will be returned.</param>
		/// <returns>A view of all forms mutually exclusive with the given form, valid until the next LookupExclusiveGroups call.</returns>
		std::span<RE::TESForm* const> MutuallyExclusiveFormsForForm(RE::TESForm* form) const;

		/// <summary>
		/// Retrieves all exclusive groups.
		/// </summary>
		/// <returns>A reference to discovered exclusive groups</returns>
		const GroupFormsMap& GetGroups() const;

	private:
		/// Precomputes the mutually exclusive forms lookup table from the resolved groups.
		void BuildMutuallyExclusiveForms();

		/// <summary>
		/// A map of each form to all other forms that share an exclusive group with it.
		/// </summary>
		FormExclusionsMap mutuallyExclusiveForms{};

		/// <summary>
		///  A map of exclusive groups names and the forms that are part of each exclusive group.
		/// </summary>
		GroupFormsMap groups{};
	};
}
