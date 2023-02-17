#pragma once

// Manage PC Level Mult NPC distribution
namespace PCLevelMult
{
	struct Input
	{
		Input(const RE::TESNPC* a_base, bool a_onlyPlayerLevelEntries, bool a_noPlayerLevelDistribution);
		Input(const RE::Actor* a_character, const RE::TESNPC* a_base, bool a_onlyPlayerLevelEntries, bool a_noPlayerLevelDistribution);

		std::uint64_t playerID;
		RE::FormID    npcFormID;
		std::uint16_t npcLevel;
		std::uint16_t npcLevelCap;
		bool          onlyPlayerLevelEntries;
		bool          noPlayerLevelDistribution;
	};

	class Manager : public RE::BSTEventSink<RE::MenuOpenCloseEvent>
	{
	public:
		static Manager* GetSingleton()
		{
			static Manager singleton;
			return &singleton;
		}

		void Register();

		[[nodiscard]] bool FindRejectedEntry(const Input& a_input, RE::FormID a_distributedFormID, std::uint32_t a_formDataIndex) const;
		bool               InsertRejectedEntry(const Input& a_input, RE::FormID a_distributedFormID, std::uint32_t a_formDataIndex);
		void               DumpRejectedEntries();

		[[nodiscard]] bool FindDistributedEntry(const Input& a_input);
		void               InsertDistributedEntry(const Input& a_input, RE::FormType a_formType, const Set<RE::FormID>& a_formIDSet);
		void               ForEachDistributedEntry(const Input& a_input, std::function<void(RE::FormType, const Set<RE::FormID>&, bool)> a_fn) const;
		void               DumpDistributedEntries();

		void DeleteNPC(RE::FormID a_characterID);

		bool HasHitLevelCap(const Input& a_input);

		std::uint64_t GetCurrentPlayerID();
		void          GetPlayerIDFromSave(const std::string& a_saveName);
		void          SetNewGameStarted();

	protected:
		static std::uint64_t get_game_playerID();
		void                 remap_player_ids(std::uint64_t a_oldID, std::uint64_t a_newID);

		RE::BSEventNotifyControl ProcessEvent(const RE::MenuOpenCloseEvent* a_event, RE::BSTEventSource<RE::MenuOpenCloseEvent>*) override;

	private:
		Manager() = default;
		Manager(const Manager&) = delete;
		Manager(Manager&&) = delete;

		~Manager() override = default;

		Manager& operator=(const Manager&) = delete;
		Manager& operator=(Manager&&) = delete;

		enum class LEVEL_CAP_STATE
		{
			kNotHit = 0,
			kHit
		};

		struct Data
		{
			struct Entries
			{
				Map<RE::FormID, Set<std::uint32_t>> rejectedEntries{};     // Distributed formID, FormData vector index
				Map<RE::FormType, Set<RE::FormID>>  distributedEntries{};  // formtype, distributed formID
			};

			LEVEL_CAP_STATE             levelCapState{};
			Map<std::uint16_t, Entries> entries{};  // Actor Level, Entries
		};

		using Lock = std::shared_mutex;
		using Locker = std::scoped_lock<Lock>;

		Map<std::uint64_t,          // PlayerID
			Map<RE::FormID, Data>>  // NPC formID, Data
					 _cache{};
		mutable Lock _lock;

		std::uint64_t currentPlayerID{ 0 };
		std::uint64_t oldPlayerID{ 0 };

		bool newGameStarted{ false };
	};
}
