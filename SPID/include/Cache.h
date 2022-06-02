#pragma once

//implements https://github.com/Ryan-rsm-McKenzie/CCExtender/blob/master/src/EditorIDCache.h
namespace Cache
{
	class EditorID
	{
	public:
		static EditorID* GetSingleton();

		void FillMap();

		std::string GetEditorID(RE::FormID a_formID);
		RE::FormID GetFormID(const std::string& a_editorID);

	private:
		using Lock = std::mutex;
		using Locker = std::scoped_lock<Lock>;

		EditorID() = default;
		EditorID(const EditorID&) = delete;
		EditorID(EditorID&&) = delete;
		~EditorID() = default;

		EditorID& operator=(const EditorID&) = delete;
		EditorID& operator=(EditorID&&) = delete;

		mutable Lock _lock;
		robin_hood::unordered_flat_map<RE::FormID, std::string> _formIDToEditorIDMap;
		robin_hood::unordered_flat_map<std::string, RE::FormID> _editorIDToFormIDMap;
	};

	namespace FormType
	{
		inline constexpr frozen::map<RE::FormType, std::string_view, 7> whitelistMap = {
			{ RE::FormType::Faction, "Faction"sv },
			{ RE::FormType::Class, "Class"sv },
			{ RE::FormType::CombatStyle, "CombatStyle"sv },
			{ RE::FormType::Race, "Race"sv },
			{ RE::FormType::Outfit, "Outfit"sv },
			{ RE::FormType::NPC, "NPC"sv },
			{ RE::FormType::VoiceType, "VoiceType"sv }
		};
		
		std::string GetWhitelistFormString(RE::FormType a_type);
	}
}
