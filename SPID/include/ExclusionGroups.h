#pragma once
#include "LookupConfigs.h"

namespace Exclusion
{
	using GroupName = std::string;
	using LinkedGroups = std::unordered_map<RE::TESForm*, std::unordered_set<GroupName>>;
	using Groups = std::unordered_map<GroupName, std::unordered_set<RE::TESForm*>>;

	class Manager : public ISingleton<Manager>
	{
	public:
		/// <summary>
		/// Does a forms lookup similar to what Filters do.
		///
		/// As a result this method configures Manager with discovered valid exclusion groups.
		/// </summary>
		/// <param name=""></param>
		/// <param name="exclusion">A Raw exclusion entries that should be processed/</param>
		void LookupExclusions(RE::TESDataHandler* const dataHandler, INI::ExclusionsVec& exclusion);

		/// <summary>
		/// Gets a set of all forms that are in the same exclusion group as the given form.
		/// Note that a form can appear in multiple exclusion groups, all of those groups are returned.
		/// </summary>
		/// <param name="form"></param>
		/// <returns>A union of all groups that contain a given form.</returns>
		std::unordered_set<RE::TESForm*> MutuallyExclusiveFormsForForm(RE::TESForm* form) const;

		/// <summary>
		/// Retrieves all exclusion groups.
		/// </summary>
		/// <returns>A reference to discovered exclusion groups</returns>
		const Groups& GetGroups() const;

	private:
		/// <summary>
		/// A map of exclusion group names related to each form in the exclusion groups.
		/// Provides a quick and easy way to get all indices that needs to be checked.
		/// </summary>
		LinkedGroups linkedGroups{};

		/// <summary>
		///  A map of exclusion groups names and the forms that are part of each exclusion group.
		/// </summary>
		Groups groups{};
	};
}
