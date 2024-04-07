#include "DeathDistribution.h"
#include "Distribute.h"
#include "LinkedDistribution.h"
#include "LookupNPC.h"
#include "PCLevelMultManager.h"

namespace DeathDistribution
{
#pragma region Parsing
	namespace INI
	{
		enum Sections : std::uint32_t
		{
			kForm = 0,
			kStrings,
			kFilterIDs,
			kLevels,
			kTraits,
			kIdxOrCount,
			kChance,

			kRequired = kForm
		};

		using Data = Configs::INI::Data;
		using DataVec = Configs::INI::DataVec;

		Map<RECORD::TYPE, DataVec> deathConfigs{};

		bool TryParse(const std::string& key, const std::string& value, const Path& path)
		{
			if (!key.starts_with("Death"sv)) {
				return false;
			}

			std::string rawType = key.substr(5);
			auto        type = RECORD::GetType(rawType);

			if (type == RECORD::kTotal) {
				logger::warn("IGNORED: Unsupported Form type ({}): {} = {}"sv, rawType, key, value);
				return true;
			}

			const auto sections = string::split(value, "|");
			const auto size = sections.size();

			if (size <= kRequired) {
				logger::warn("IGNORED: Entry must at least FormID or EditorID: {} = {}"sv, key, value);
				return true;
			}

			Data data{};

			data.rawForm = distribution::get_record(sections[kForm]);

			//KEYWORDS
			if (kStrings < size) {
				StringFilters filters;

				auto split_str = distribution::split_entry(sections[kStrings]);
				for (auto& str : split_str) {
					if (str.contains("+"sv)) {
						auto strings = distribution::split_entry(str, "+");
						data.stringFilters.ALL.insert(data.stringFilters.ALL.end(), strings.begin(), strings.end());

					} else if (str.at(0) == '-') {
						str.erase(0, 1);
						data.stringFilters.NOT.emplace_back(str);

					} else if (str.at(0) == '*') {
						str.erase(0, 1);
						data.stringFilters.ANY.emplace_back(str);

					} else {
						data.stringFilters.MATCH.emplace_back(str);
					}
				}
			}

			//FILTER FORMS
			if (kFilterIDs < size) {
				auto split_IDs = distribution::split_entry(sections[kFilterIDs]);
				for (auto& IDs : split_IDs) {
					if (IDs.contains("+"sv)) {
						auto splitIDs_ALL = distribution::split_entry(IDs, "+");
						for (auto& IDs_ALL : splitIDs_ALL) {
							data.rawFormFilters.ALL.push_back(distribution::get_record(IDs_ALL));
						}
					} else if (IDs.at(0) == '-') {
						IDs.erase(0, 1);
						data.rawFormFilters.NOT.push_back(distribution::get_record(IDs));

					} else {
						data.rawFormFilters.MATCH.push_back(distribution::get_record(IDs));
					}
				}
			}
			//LEVEL
			if (kLevels < size) {
				Range<std::uint16_t>    actorLevel;
				std::vector<SkillLevel> skillLevels;
				std::vector<SkillLevel> skillWeights;
				auto                    split_levels = distribution::split_entry(sections[kLevels]);
				for (auto& levels : split_levels) {
					if (levels.contains('(')) {
						//skill(min/max)
						const auto isWeightFilter = levels.starts_with('w');
						auto       sanitizedLevel = string::remove_non_alphanumeric(levels);
						if (isWeightFilter) {
							sanitizedLevel.erase(0, 1);
						}
						//skill min max
						if (auto skills = string::split(sanitizedLevel, " "); !skills.empty()) {
							if (auto type = string::to_num<std::uint32_t>(skills[0]); type < 18) {
								auto minLevel = string::to_num<std::uint8_t>(skills[1]);
								if (skills.size() > 2) {
									auto maxLevel = string::to_num<std::uint8_t>(skills[2]);
									if (isWeightFilter) {
										skillWeights.push_back({ type, Range(minLevel, maxLevel) });
									} else {
										skillLevels.push_back({ type, Range(minLevel, maxLevel) });
									}
								} else {
									if (isWeightFilter) {
										// Single value is treated as exact match.
										skillWeights.push_back({ type, Range(minLevel) });
									} else {
										skillLevels.push_back({ type, Range(minLevel) });
									}
								}
							}
						}
					} else {
						if (auto actor_level = string::split(levels, "/"); actor_level.size() > 1) {
							auto minLevel = string::to_num<std::uint16_t>(actor_level[0]);
							auto maxLevel = string::to_num<std::uint16_t>(actor_level[1]);

							actorLevel = Range(minLevel, maxLevel);
						} else {
							auto level = string::to_num<std::uint16_t>(levels);

							actorLevel = Range(level);
						}
					}
				}
				data.levelFilters = { actorLevel, skillLevels, skillWeights };
			}

			//TRAITS
			if (kTraits < size) {
				auto split_traits = distribution::split_entry(sections[kTraits], "/");
				for (auto& trait : split_traits) {
					switch (string::const_hash(trait)) {
					case "M"_h:
					case "-F"_h:
						data.traits.sex = RE::SEX::kMale;
						break;
					case "F"_h:
					case "-M"_h:
						data.traits.sex = RE::SEX::kFemale;
						break;
					case "U"_h:
						data.traits.unique = true;
						break;
					case "-U"_h:
						data.traits.unique = false;
						break;
					case "S"_h:
						data.traits.summonable = true;
						break;
					case "-S"_h:
						data.traits.summonable = false;
						break;
					case "C"_h:
						data.traits.child = true;
						break;
					case "-C"_h:
						data.traits.child = false;
						break;
					case "L"_h:
						data.traits.leveled = true;
						break;
					case "-L"_h:
						data.traits.leveled = false;
						break;
					case "T"_h:
						data.traits.teammate = true;
						break;
					case "-T"_h:
						data.traits.teammate = false;
						break;
					default:
						break;
					}
				}
			}

			//ITEMCOUNT/INDEX
			if (type == RECORD::kPackage) {  // reuse item count for package stack index
				data.idxOrCount = 0;
			}

			if (kIdxOrCount < size) {
				if (type == RECORD::kPackage) {  // If it's a package, then we only expect a single number.
					if (const auto& str = sections[kIdxOrCount]; distribution::is_valid_entry(str)) {
						data.idxOrCount = string::to_num<Index>(str);
					}
				} else {
					if (const auto& str = sections[kIdxOrCount]; distribution::is_valid_entry(str)) {
						if (auto countPair = string::split(str, "-"); countPair.size() > 1) {
							auto minCount = string::to_num<Count>(countPair[0]);
							auto maxCount = string::to_num<Count>(countPair[1]);

							data.idxOrCount = RandomCount(minCount, maxCount);
						} else {
							auto count = string::to_num<Count>(str);

							data.idxOrCount = RandomCount(count, count);  // create the exact match range.
						}
					}
				}
			}

			//CHANCE
			if (kChance < size) {
				if (const auto& str = sections[kChance]; distribution::is_valid_entry(str)) {
					data.chance = string::to_num<PercentChance>(str);
				}
			}

			data.path = path;

			deathConfigs[type].emplace_back(data);
			return true;
		}
	}
#pragma endregion

	void Manager::Register()
	{
		if (INI::deathConfigs.empty()) {
			return;
		}

		if (const auto scripts = RE::ScriptEventSourceHolder::GetSingleton()) {
			scripts->AddEventSink<RE::TESDeathEvent>(GetSingleton());
			logger::info("Registered for {}", typeid(RE::TESDeathEvent).name());
		}
	}

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
			LookupGenericForm<RE::TESForm>(dataHandler, rawSpell, [&](bool isValid, auto form, const auto& idxOrCount, const auto& filters, const auto& path) {
				if (const auto spell = form->As<RE::SpellItem>(); spell) {
					spells.EmplaceForm(isValid, spell, idxOrCount, filters, path);
				} else if (const auto levSpell = form->As<RE::TESLevSpell>(); levSpell) {
					levSpells.EmplaceForm(isValid, levSpell, idxOrCount, filters, path);
				}
			});
		}

		auto& genericForms = INI::deathConfigs[RECORD::kForm];

		for (auto& rawForm : genericForms) {
			// Add to appropriate list. (Note that type inferring doesn't recognize SleepOutfit, Skin)
			LookupGenericForm<RE::TESForm>(dataHandler, rawForm, [&](bool isValid, auto form, const auto& idxOrCount, const auto& filters, const auto& path) {
				if (const auto keyword = form->As<RE::BGSKeyword>(); keyword) {
					keywords.EmplaceForm(isValid, keyword, idxOrCount, filters, path);
				} else if (const auto spell = form->As<RE::SpellItem>(); spell) {
					spells.EmplaceForm(isValid, spell, idxOrCount, filters, path);
				} else if (const auto levSpell = form->As<RE::TESLevSpell>(); levSpell) {
					levSpells.EmplaceForm(isValid, levSpell, idxOrCount, filters, path);
				} else if (const auto perk = form->As<RE::BGSPerk>(); perk) {
					perks.EmplaceForm(isValid, perk, idxOrCount, filters, path);
				} else if (const auto shout = form->As<RE::TESShout>(); shout) {
					shouts.EmplaceForm(isValid, shout, idxOrCount, filters, path);
				} else if (const auto item = form->As<RE::TESBoundObject>(); item) {
					items.EmplaceForm(isValid, item, idxOrCount, filters, path);
				} else if (const auto outfit = form->As<RE::BGSOutfit>(); outfit) {
					outfits.EmplaceForm(isValid, outfit, idxOrCount, filters, path);
				} else if (const auto faction = form->As<RE::TESFaction>(); faction) {
					factions.EmplaceForm(isValid, faction, idxOrCount, filters, path);
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
						packages.EmplaceForm(isValid, form, packageIndex, filters, path);
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

		logger::info("{:*^50}", "ON DEATH");

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

	RE::BSEventNotifyControl Manager::ProcessEvent(const RE::TESDeathEvent* a_event, RE::BSTEventSource<RE::TESDeathEvent>*)
	{
		constexpr auto is_NPC = [](auto&& a_ref) {
			return a_ref && !a_ref->IsPlayerRef();
		};

		if (a_event && a_event->dead && is_NPC(a_event->actorDying)) {
			const auto actor = a_event->actorDying->As<RE::Actor>();
			const auto npc = actor ? actor->GetActorBase() : nullptr;
			if (actor && npc) {
				auto       npcData = NPCData(actor, npc);
				const auto input = PCLevelMult::Input{ actor, npc, false };

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

				Distribute::Distribute(npcData, input, entries, false, &distributedForms);
				// TODO: We can now log per-NPC distributed forms.

				if (!distributedForms.empty()) {
					LinkedDistribution::Manager::GetSingleton()->ForEachLinkedDeathDistributionSet(distributedForms, [&](Forms::DistributionSet& set) {
						Distribute::Distribute(npcData, input, set, true, nullptr);  // TODO: Accumulate forms here? to log what was distributed.
					});
				}
			}
		}

		return RE::BSEventNotifyControl::kContinue;
	}
}
