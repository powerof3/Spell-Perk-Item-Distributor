#include "DeathDistribution.h"
#include "Distribute.h"
#include "Hooking.h"
#include "LinkedDistribution.h"
#include "LookupNPC.h"
#include "OutfitManager.h"
#include "PCLevelMultManager.h"
#include "Parser.h"

namespace DeathDistribution
{
#pragma region Parsing

	namespace INI
	{
		struct DeathKeyComponentParser
		{
			template <Distribution::INI::concepts::typed_data Data>
			bool operator()(const std::string& originalKey, Data& data) const
			{
				std::string key = originalKey;

				if (key.starts_with("Final"sv)) {
					data.recordTraits = RECORD::TRAITS::Final;
					key.erase(0, 5);
				}

				if (!key.starts_with("Death"sv)) {
					return false;
				}

				key.erase(0, 5);

				auto type = RECORD::GetType(key);

				if (type == RECORD::kTotal) {
					throw Distribution::INI::Exception::UnsupportedFormTypeException(key);
				}
				data.type = type;

				return true;
			}
		};

		using DeathData = Distribution::INI::Data;
		using DataVec = Distribution::INI::DataVec;

		Map<RECORD::TYPE, DataVec> deathConfigs{};

		bool TryParse(const std::string& key, const std::string& value, const Path& path)
		{
			using namespace Distribution::INI;

			try {
				if (auto optData = Parse<DeathData,
						DeathKeyComponentParser,
						DistributableFormComponentParser,
						StringFiltersComponentParser<>,
						FormFiltersComponentParser<>,
						LevelFiltersComponentParser,
						TraitsFilterComponentParser,
						IndexOrCountComponentParser,
						ChanceComponentParser>(key, value);
					optData) {
					auto& data = *optData;
					data.path = path;
					if (data.recordTraits & RECORD::TRAITS::Final && data.type != RECORD::TYPE::kOutfit) {
						data.recordTraits &= ~RECORD::TRAITS::Final;
						logger::info("\t\t[{} = {}]", key, value);
						logger::info("\t\t\tFinal modifier can only be applied to Outfits.");
					}
					deathConfigs[data.type].emplace_back(data);
				} else {
					return false;
				}
			} catch (const std::exception& e) {
				logger::warn("\t\tFailed to parse entry [{} = {}]: {}", key, value, e.what());
			}
			return true;
		}
	}

#pragma endregion

#pragma region Lookup

	void Manager::LookupForms(RE::TESDataHandler* const dataHandler)
	{
		using namespace Forms;

		ForEachDistributable([&]<typename Form>(Distributables<Form>& a_distributable) {
			// If it's spells distributable we want to manually lookup forms to pick LevSpells that are added into the list.
			if constexpr (!std::is_same_v<Form, RE::SpellItem>) {
				const auto& recordName = RECORD::GetTypeName(a_distributable.GetType());

				a_distributable.LookupForms(dataHandler, recordName, INI::deathConfigs[a_distributable.GetType()]);
			}
		});

		// Sort out Spells and Leveled Spells into two separate lists.
		auto& rawSpells = INI::deathConfigs[RECORD::kSpell];

		for (auto& rawSpell : rawSpells) {
			LookupGenericForm<RE::TESForm>(dataHandler, rawSpell, [&](bool isValid, auto form, const bool& isFinal, const auto& idxOrCount, const auto& filters, const auto& path) {
				if (const auto spell = form->As<RE::SpellItem>(); spell) {
					spells.EmplaceForm(isValid, spell, isFinal, idxOrCount, filters, path);
				} else if (const auto levSpell = form->As<RE::TESLevSpell>(); levSpell) {
					levSpells.EmplaceForm(isValid, levSpell, isFinal, idxOrCount, filters, path);
				}
			});
		}

		auto& genericForms = INI::deathConfigs[RECORD::kForm];

		for (auto& rawForm : genericForms) {
			// Add to appropriate list. (Note that type inferring doesn't recognize SleepOutfit, Skin)
			LookupGenericForm<RE::TESForm>(dataHandler, rawForm, [&](bool isValid, auto form, const bool& isFinal, const auto& idxOrCount, const auto& filters, const auto& path) {
				if (const auto keyword = form->As<RE::BGSKeyword>(); keyword) {
					keywords.EmplaceForm(isValid, keyword, isFinal, idxOrCount, filters, path);
				} else if (const auto spell = form->As<RE::SpellItem>(); spell) {
					spells.EmplaceForm(isValid, spell, isFinal, idxOrCount, filters, path);
				} else if (const auto levSpell = form->As<RE::TESLevSpell>(); levSpell) {
					levSpells.EmplaceForm(isValid, levSpell, isFinal, idxOrCount, filters, path);
				} else if (const auto perk = form->As<RE::BGSPerk>(); perk) {
					perks.EmplaceForm(isValid, perk, isFinal, idxOrCount, filters, path);
				} else if (const auto shout = form->As<RE::TESShout>(); shout) {
					shouts.EmplaceForm(isValid, shout, isFinal, idxOrCount, filters, path);
				} else if (const auto item = form->As<RE::TESBoundObject>(); item) {
					items.EmplaceForm(isValid, item, isFinal, idxOrCount, filters, path);
				} else if (const auto outfit = form->As<RE::BGSOutfit>(); outfit) {
					outfits.EmplaceForm(isValid, outfit, isFinal, idxOrCount, filters, path);
				} else if (const auto faction = form->As<RE::TESFaction>(); faction) {
					factions.EmplaceForm(isValid, faction, isFinal, idxOrCount, filters, path);
				} else {
					auto type = form->GetFormType();
					if (type == RE::FormType::Package || type == RE::FormType::FormList) {
						// With generic Form entries we default to RandomCount, so we need to properly convert it to Index if it turned out to be a package.
						Index packageIndex = 1;
						if (std::holds_alternative<RandomCount>(idxOrCount)) {
							auto& count = std::get<RandomCount>(idxOrCount);
							if (!count.IsExact()) {
								logger::warn("\t[{}] Inferred Form is a Package, but specifies a random count instead of index. Min value ({}) of the range will be used as an index.", path, count.min);
							}
							packageIndex = count.min;
						} else {
							packageIndex = std::get<Index>(idxOrCount);
						}
						packages.EmplaceForm(isValid, form, isFinal, packageIndex, filters, path);
					} else {
						logger::warn("\t[{}] Unsupported Form type: {}", path, type);
					}
				}
			});
		}
	}

	bool Manager::IsEmpty()
	{
		return spells.GetForms().empty() &&
		       perks.GetForms().empty() &&
		       items.GetForms().empty() &&
		       shouts.GetForms().empty() &&
		       levSpells.GetForms().empty() &&
		       packages.GetForms().empty() &&
		       outfits.GetForms().empty() &&
		       keywords.GetForms().empty() &&
		       factions.GetForms().empty() &&
		       sleepOutfits.GetForms().empty() &&
		       skins.GetForms().empty();
	}

	void Manager::LogFormsLookup()
	{
		if (IsEmpty()) {
			return;
		}

		using namespace Forms;

		LOG_HEADER("ON DEATH");

		ForEachDistributable([]<typename Form>(Distributables<Form>& a_distributable) {
			const auto& recordName = RECORD::GetTypeName(a_distributable.GetType());

			const auto added = a_distributable.GetSize();
			const auto all = a_distributable.GetLookupCount();

			// Only log entries that are actually present in INIs.
			if (all > 0) {
				logger::info("Registered {}/{} {}s", added, all, recordName);
			}
		});
	}
#pragma endregion

#pragma region Distribution

	struct ShouldBackgroundClone
	{
		using Target = RE::Character;
		static inline constexpr std::size_t index{ 0x6D };

		static bool thunk(RE::Character* actor)
		{
#ifndef NDEBUG
			logger::info("Distribute: ShouldBackgroundClone({})", *(actor->As<RE::Actor>()));
#endif
			if (const auto npc = actor->GetActorBase()) {
				auto npcData = NPCData(actor, npc);
				if (npcData.IsDead()) {
					Manager::GetSingleton()->Distribute(npcData);
				}
			}
			return func(actor);
		}

		static inline void post_hook()
		{
			logger::info("Death Distribution: Installed ShouldBackgroundClone hook.");
		}

		static inline REL::Relocation<decltype(thunk)> func;
	};

	void Manager::HandleMessage(SKSE::MessagingInterface::Message* message)
	{
		switch (message->type) {
		case SKSE::MessagingInterface::kDataLoaded:
			if (INI::deathConfigs.empty()) {
				return;
			}

			if (const auto scripts = RE::ScriptEventSourceHolder::GetSingleton()) {
				scripts->PrependEventSink<RE::TESDeathEvent>(this);
				logger::info("Death Distribution: Registered for {}.", typeid(RE::TESDeathEvent).name());
			}

			stl::install_hook<ShouldBackgroundClone>();

			break;
		default:
			break;
		}
	}

	void Manager::Distribute(NPCData& data)
	{
		assert(data.IsDead() || data.IsDying());

		const auto input = PCLevelMult::Input{ data.GetActor(), data.GetNPC(), false };

		DistributedForms distributedForms{};

		Forms::DistributionSet entries{
			spells.GetForms(),
			perks.GetForms(),
			items.GetForms(),
			shouts.GetForms(),
			levSpells.GetForms(),
			packages.GetForms(),
			outfits.GetForms(),
			keywords.GetForms(),
			factions.GetForms(),
			sleepOutfits.GetForms(),
			skins.GetForms()
		};

		Distribute::Distribute(data, input, entries, &distributedForms, Outfits::SetDeathOutfit);

		if (!distributedForms.empty()) {
			LinkedDistribution::Manager::GetSingleton()->ForEachLinkedDistributionSet(LinkedDistribution::kDeath, distributedForms, [&](Forms::DistributionSet& set) {
				Distribute::Distribute(data, input, set, &distributedForms, Outfits::SetDeathOutfit);
			});
		}

		Distribute::LogDistribution(distributedForms, data);
	}

	RE::BSEventNotifyControl Manager::ProcessEvent(const RE::TESDeathEvent* a_event, RE::BSTEventSource<RE::TESDeathEvent>*)
	{
		if (!a_event || a_event->dead) {
			return RE::BSEventNotifyControl::kContinue;
		}

		if (const auto actor = a_event->actorDying->As<RE::Actor>(); actor && !actor->IsPlayerRef()) {
			if (const auto npc = actor->GetActorBase(); npc) {
				logger::info("Dying {}", *actor);
				auto npcData = NPCData(actor, npc, true);
				Distribute(npcData);
			}
		}

		return RE::BSEventNotifyControl::kContinue;
	}
#pragma endregion
}
