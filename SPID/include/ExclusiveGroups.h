#pragma once
#include "LookupConfigs.h"

namespace ExclusiveGroups
{
	namespace INI
	{
		struct RawExclusiveGroup
		{
			std::string name{};

			/// Raw filters in RawExclusiveGroup only use NOT and MATCH, there is no meaning for ALL, so it's ignored.
			Filters<FormOrEditorID> formIDs{};
			std::string             path{};
		};

		using ExclusiveGroupsVec = std::vector<RawExclusiveGroup>;

		/// <summary>
		/// A list of RawExclusiveGroups that will be processed along with configs.
		/// </summary>
		inline ExclusiveGroupsVec exclusiveGroups{};

		bool TryParse(const std::string& a_key, const std::string& a_value, const std::string& a_path);
	}

	using GroupName = std::string;
	using LinkedGroups = std::unordered_map<RE::TESForm*, std::unordered_set<GroupName>>;
	using Groups = std::unordered_map<GroupName, std::unordered_set<RE::TESForm*>>;

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
		/// Gets a set of all forms that are in the same exclusive group as the given form.
		/// Note that a form can appear in multiple exclusive groups, all of those groups are returned.
		/// </summary>
		/// <param name="form">A form for which mutually exclusive forms will be returned.</param>
		/// <returns>A union of all groups that contain a given form.</returns>
		std::unordered_set<RE::TESForm*> MutuallyExclusiveFormsForForm(RE::TESForm* form) const;

		/// <summary>
		/// Retrieves all exclusive groups.
		/// </summary>
		/// <returns>A reference to discovered exclusive groups</returns>
		const Groups& GetGroups() const;

	private:
		/// <summary>
		/// A map of exclusive group names related to each form in the exclusive groups.
		/// Provides a quick and easy way to get names of all groups that need to be checked.
		/// </summary>
		LinkedGroups linkedGroups{};

		/// <summary>
		///  A map of exclusive groups names and the forms that are part of each exclusive group.
		/// </summary>
		Groups groups{};
	};
}
