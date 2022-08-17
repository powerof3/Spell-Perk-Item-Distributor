#pragma once
#include "LookupForms.h"

namespace Filter
{
	namespace detail
	{
		namespace name
		{
			inline bool contains(const std::string& a_name, const StringVec& a_strings)
			{
				return std::ranges::any_of(a_strings, [&](const auto& str) {
					return string::icontains(a_name, str);
				});
			}

			inline bool matches(const std::string& a_name, const StringVec& a_strings)
			{
				return std::ranges::any_of(a_strings, [&](const auto& str) {
					return string::iequals(a_name, str);
				});
			}
		}

		namespace form
		{
			inline bool get_type(RE::TESNPC& a_actorbase, RE::TESForm* a_filter)
			{
				switch (a_filter->GetFormType()) {
				case RE::FormType::CombatStyle:
					return a_actorbase.GetCombatStyle() == a_filter;
				case RE::FormType::Class:
					return a_actorbase.npcClass == a_filter;
				case RE::FormType::Faction:
					{
						const auto faction = static_cast<RE::TESFaction*>(a_filter);
						return faction && a_actorbase.IsInFaction(faction);
					}
				case RE::FormType::Race:
					return a_actorbase.GetRace() == a_filter;
				case RE::FormType::Outfit:
					return a_actorbase.defaultOutfit == a_filter;
				case RE::FormType::NPC:
					return &a_actorbase == a_filter;
				case RE::FormType::VoiceType:
					return a_actorbase.voiceType == a_filter;
				default:
					return false;
				}
			}

			inline bool matches(RE::TESNPC& a_actorbase, const FormVec& a_forms)
			{
				return std::ranges::any_of(a_forms, [&a_actorbase](const auto& a_formFile) {
					if (std::holds_alternative<RE::TESForm*>(a_formFile)) {
						auto form = std::get<RE::TESForm*>(a_formFile);
						return form && get_type(a_actorbase, form);
					}
					if (std::holds_alternative<const RE::TESFile*>(a_formFile)) {
						auto file = std::get<const RE::TESFile*>(a_formFile);
						return file && file->IsFormInMod(a_actorbase.GetFormID());
					}
					return false;
				});
			}

			inline bool matches_ALL(RE::TESNPC& a_actorbase, const FormVec& a_forms)
			{
				return std::ranges::all_of(a_forms, [&a_actorbase](const auto& a_formFile) {
					if (std::holds_alternative<RE::TESForm*>(a_formFile)) {
						auto form = std::get<RE::TESForm*>(a_formFile);
						return form && get_type(a_actorbase, form);
					}
					if (std::holds_alternative<const RE::TESFile*>(a_formFile)) {
						auto file = std::get<const RE::TESFile*>(a_formFile);
						return file && file->IsFormInMod(a_actorbase.GetFormID());
					}
					return false;
				});
			}
		}

		namespace keyword
		{
			inline bool contains(RE::TESNPC& a_actorbase, const StringVec& a_strings)
			{
				return std::ranges::any_of(a_strings, [&a_actorbase](const auto& str) {
					return a_actorbase.ContainsKeyword(str);
				});
			}

			inline bool matches(RE::TESNPC& a_actorbase, const StringVec& a_strings, bool a_matchesAll = false)
			{
				const auto has_keyword = [&](const auto& str) {
					return a_actorbase.HasApplicableKeywordString(str);
				};

				if (a_matchesAll) {
					return std::ranges::all_of(a_strings, has_keyword);
				}
				return std::ranges::any_of(a_strings, has_keyword);
			}
		}
	}

	inline bool strings(RE::TESNPC& a_actorbase, const FormData& a_formData)
	{
		auto& [strings_ALL, strings_NOT, strings_MATCH, strings_ANY] = std::get<DATA::TYPE::kStrings>(a_formData);

		if (!strings_ALL.empty() && !detail::keyword::matches(a_actorbase, strings_ALL, true)) {
			return false;
		}

		const std::string name = a_actorbase.GetName();
		const std::string editorID = Cache::EditorID::GetEditorID(a_actorbase.GetFormID());

		if (!strings_NOT.empty()) {
			bool result = false;
			if (!name.empty() && detail::name::matches(name, strings_NOT)) {
				result = true;
			}
			if (!result && !editorID.empty() && detail::name::matches(editorID, strings_NOT)) {
				result = true;
			}
			if (!result && detail::keyword::matches(a_actorbase, strings_NOT)) {
				result = true;
			}
			if (result) {
				return false;
			}
		}

		if (!strings_MATCH.empty()) {
			bool result = false;
			if (!name.empty() && detail::name::matches(name, strings_MATCH)) {
				result = true;
			}
			if (!result && !editorID.empty() && detail::name::matches(editorID, strings_MATCH)) {
				result = true;
			}
			if (!result && detail::keyword::matches(a_actorbase, strings_MATCH)) {
				result = true;
			}
			if (!result) {
				return false;
			}
		}

		if (!strings_ANY.empty()) {
			bool result = false;
			if (!name.empty() && detail::name::contains(name, strings_ANY)) {
				result = true;
			}
			if (!result && !editorID.empty() && detail::name::contains(editorID, strings_ANY)) {
				result = true;
			}
			if (!result && detail::keyword::contains(a_actorbase, strings_ANY)) {
				result = true;
			}
			if (!result) {
				return false;
			}
		}

		return true;
	}

	inline bool forms(RE::TESNPC& a_actorbase, const FormData& a_formData)
	{
		auto& [filterForms_ALL, filterForms_NOT, filterForms_MATCH] = std::get<DATA::TYPE::kFilterForms>(a_formData);

		if (!filterForms_ALL.empty() && !detail::form::matches_ALL(a_actorbase, filterForms_ALL)) {
			return false;
		}

		if (!filterForms_NOT.empty() && detail::form::matches(a_actorbase, filterForms_NOT)) {
			return false;
		}

		if (!filterForms_MATCH.empty() && !detail::form::matches(a_actorbase, filterForms_MATCH)) {
			return false;
		}

		return true;
	}

	inline bool secondary(const RE::TESNPC& a_actorbase, const FormData& a_formData)
	{
		const auto& [sex, isUnique, isSummonable] = std::get<DATA::TYPE::kTraits>(a_formData);
		if (sex && a_actorbase.GetSex() != *sex) {
			return false;
		}
		if (isUnique && a_actorbase.IsUnique() != *isUnique) {
			return false;
		}
		if (isSummonable && a_actorbase.IsSummonable() != *isSummonable) {
			return false;
		}

		const auto& [actorLevelPair, skillLevelPairs] = std::get<DATA::TYPE::kLevel>(a_formData);

		if (!a_actorbase.HasPCLevelMult()) {
			auto& [actorMin, actorMax] = actorLevelPair;
			const auto actorLevel = a_actorbase.actorData.level;

			if (actorMin < UINT16_MAX && actorMax < UINT16_MAX) {
				if (actorLevel < actorMin || actorLevel > actorMax) {
					return false;
				}
			} else if (actorMin < UINT16_MAX && actorLevel < actorMin) {
				return false;
			} else if (actorMax < UINT16_MAX && actorLevel > actorMax) {
				return false;
			}
		}

		for (auto& [skillType, skill] : skillLevelPairs) {
			auto& [skillMin, skillMax] = skill;

			if (skillType < 18) {
				auto const skillLevel = a_actorbase.playerSkills.values[skillType];

				if (skillMin < UINT8_MAX && skillMax < UINT8_MAX) {
					if (skillLevel < skillMin || skillLevel > skillMax) {
						return false;
					}
				} else if (skillMin < UINT8_MAX && skillLevel < skillMin) {
					return false;
				} else if (skillMax < UINT8_MAX && skillLevel > skillMax) {
					return false;
				}
			}
		}

		const auto chance = std::get<DATA::TYPE::kChance>(a_formData);
		if (!numeric::essentially_equal(chance, 100.0)) {
			if (auto rng = RNG::GetSingleton()->Generate<float>(0.0, 100.0); rng > chance) {
				return false;
			}
		}

		return true;
	}
}

namespace Distribute
{
	template <class Form>
	void for_each_form(RE::TESNPC& a_actorbase, Forms::FormMap<Form>& a_formDataMap, std::function<bool(const FormCount<Form>&)> a_fn)
	{
		for (auto& [form, data] : a_formDataMap.forms) {
			if (form != nullptr) {
				auto& [npcCount, formDataVec] = data;
				for (auto& formData : formDataVec) {
					if (!Filter::strings(a_actorbase, formData) || !Filter::forms(a_actorbase, formData) || !Filter::secondary(a_actorbase, formData)) {
						continue;
					}
					auto itemCount = std::get<DATA::TYPE::kItemCount>(formData);
					if (a_fn({ form, itemCount })) {
						++npcCount;
					}
				}
			}
		}
	}

	template <class Form>
	void list_npc_count(const std::string& a_type, Forms::FormMap<Form>& a_formDataMap, const size_t a_totalNPCCount)
	{
		logger::info("	{}", a_type);

		for (auto& [form, formData] : a_formDataMap.forms) {
			if (form != nullptr) {
				auto& [npcCount, data] = formData;
				std::string name;
				if constexpr (std::is_same_v<Form, RE::BGSKeyword>) {
					name = form->GetFormEditorID();
				} else {
					name = Cache::EditorID::GetEditorID(form->GetFormID());
				}
				logger::info("		{} [0x{:X}] added to {}/{} NPCs", name, form->GetFormID(), npcCount, a_totalNPCCount);
			}
		}
	}

	void Distribute(RE::TESNPC* a_actorbase);

	class DeathItemManager : public RE::BSTEventSink<RE::TESDeathEvent>
	{
	public:
		static DeathItemManager* GetSingleton()
		{
			static DeathItemManager singleton;
			return &singleton;
		}

		static void Register();

	protected:
		using EventResult = RE::BSEventNotifyControl;

	    struct detail  //AddObjectToContainer doesn't work with leveled items :s
		{
			static void add_item(RE::Actor* a_actor, RE::TESBoundObject* a_item, std::uint32_t a_itemCount, bool a_silent, std::uint32_t a_stackID, RE::BSScript::Internal::VirtualMachine* a_vm)
			{
				using func_t = decltype(&detail::add_item);
				REL::Relocation<func_t> func{ RELOCATION_ID(55945, 56489) };
				return func(a_actor, a_item, a_itemCount, a_silent, a_stackID, a_vm);
			}
		};

	    EventResult ProcessEvent(const RE::TESDeathEvent* a_event, RE::BSTEventSource<RE::TESDeathEvent>*) override;

	private:
		DeathItemManager() = default;
		DeathItemManager(const DeathItemManager&) = delete;
		DeathItemManager(DeathItemManager&&) = delete;

		~DeathItemManager() override = default;

		DeathItemManager& operator=(const DeathItemManager&) = delete;
		DeathItemManager& operator=(DeathItemManager&&) = delete;
	};

	namespace LeveledActor
	{
		void Install();
	}

	void ApplyToNPCs();
}
