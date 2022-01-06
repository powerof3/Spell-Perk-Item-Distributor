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
		
		inline constexpr frozen::map<RE::FormType, std::string_view, 138> blacklistMap = {
			{ RE::FormType::None, "NONE"sv },
			{ RE::FormType::PluginInfo, "TES4"sv },
			{ RE::FormType::FormGroup, "GRUP"sv },
			{ RE::FormType::GameSetting, "GMST"sv },
			{ RE::FormType::Keyword, "KYWD"sv },
			{ RE::FormType::LocationRefType, "LCRT"sv },
			{ RE::FormType::Action, "AACT"sv },
			{ RE::FormType::TextureSet, "TXST"sv },
			{ RE::FormType::MenuIcon, "MICN"sv },
			{ RE::FormType::Global, "GLOB"sv },
			{ RE::FormType::Class, "CLAS"sv },
			{ RE::FormType::Faction, "FACT"sv },
			{ RE::FormType::HeadPart, "HDPT"sv },
			{ RE::FormType::Eyes, "EYES"sv },
			{ RE::FormType::Race, "RACE"sv },
			{ RE::FormType::Sound, "SOUN"sv },
			{ RE::FormType::AcousticSpace, "ASPC"sv },
			{ RE::FormType::Skill, "SKIL"sv },
			{ RE::FormType::MagicEffect, "MGEF"sv },
			{ RE::FormType::Script, "SCPT"sv },
			{ RE::FormType::LandTexture, "LTEX"sv },
			{ RE::FormType::Enchantment, "ENCH"sv },
			{ RE::FormType::Spell, "SPEL"sv },
			{ RE::FormType::Scroll, "SCRL"sv },
			{ RE::FormType::Activator, "ACTI"sv },
			{ RE::FormType::TalkingActivator, "TACT"sv },
			{ RE::FormType::Armor, "ARMO"sv },
			{ RE::FormType::Book, "BOOK"sv },
			{ RE::FormType::Container, "CONT"sv },
			{ RE::FormType::Door, "DOOR"sv },
			{ RE::FormType::Ingredient, "INGR"sv },
			{ RE::FormType::Light, "LIGH"sv },
			{ RE::FormType::Misc, "MISC"sv },
			{ RE::FormType::Apparatus, "APPA"sv },
			{ RE::FormType::Static, "STAT"sv },
			{ RE::FormType::StaticCollection, "SCOL"sv },
			{ RE::FormType::MovableStatic, "MSTT"sv },
			{ RE::FormType::Grass, "GRAS"sv },
			{ RE::FormType::Tree, "TREE"sv },
			{ RE::FormType::Flora, "FLOR"sv },
			{ RE::FormType::Furniture, "FURN"sv },
			{ RE::FormType::Weapon, "WEAP"sv },
			{ RE::FormType::Ammo, "AMMO"sv },
			{ RE::FormType::NPC, "NPC_"sv },
			{ RE::FormType::LeveledNPC, "LVLN"sv },
			{ RE::FormType::KeyMaster, "KEYM"sv },
			{ RE::FormType::AlchemyItem, "ALCH"sv },
			{ RE::FormType::IdleMarker, "IDLM"sv },
			{ RE::FormType::Note, "NOTE"sv },
			{ RE::FormType::ConstructibleObject, "COBJ"sv },
			{ RE::FormType::Projectile, "PROJ"sv },
			{ RE::FormType::Hazard, "HAZD"sv },
			{ RE::FormType::SoulGem, "SLGM"sv },
			{ RE::FormType::LeveledItem, "LVLI"sv },
			{ RE::FormType::Weather, "WTHR"sv },
			{ RE::FormType::Climate, "CLMT"sv },
			{ RE::FormType::ShaderParticleGeometryData, "SPGD"sv },
			{ RE::FormType::ReferenceEffect, "RFCT"sv },
			{ RE::FormType::Region, "REGN"sv },
			{ RE::FormType::Navigation, "NAVI"sv },
			{ RE::FormType::Cell, "CELL"sv },
			{ RE::FormType::Reference, "REFR"sv },
			{ RE::FormType::ActorCharacter, "ACHR"sv },
			{ RE::FormType::ProjectileMissile, "PMIS"sv },
			{ RE::FormType::ProjectileArrow, "PARW"sv },
			{ RE::FormType::ProjectileGrenade, "PGRE"sv },
			{ RE::FormType::ProjectileBeam, "PBEA"sv },
			{ RE::FormType::ProjectileFlame, "PFLA"sv },
			{ RE::FormType::ProjectileCone, "PCON"sv },
			{ RE::FormType::ProjectileBarrier, "PBAR"sv },
			{ RE::FormType::PlacedHazard, "PHZD"sv },
			{ RE::FormType::WorldSpace, "WRLD"sv },
			{ RE::FormType::Land, "LAND"sv },
			{ RE::FormType::NavMesh, "NAVM"sv },
			{ RE::FormType::TLOD, "TLOD"sv },
			{ RE::FormType::Dialogue, "DIAL"sv },
			{ RE::FormType::Info, "INFO"sv },
			{ RE::FormType::Quest, "QUST"sv },
			{ RE::FormType::Idle, "IDLE"sv },
			{ RE::FormType::Package, "PACK"sv },
			{ RE::FormType::CombatStyle, "CSTY"sv },
			{ RE::FormType::LoadScreen, "LSCR"sv },
			{ RE::FormType::LeveledSpell, "LVSP"sv },
			{ RE::FormType::AnimatedObject, "ANIO"sv },
			{ RE::FormType::Water, "WATR"sv },
			{ RE::FormType::EffectShader, "EFSH"sv },
			{ RE::FormType::TOFT, "TOFT"sv },
			{ RE::FormType::Explosion, "EXPL"sv },
			{ RE::FormType::Debris, "DEBR"sv },
			{ RE::FormType::ImageSpace, "IMGS"sv },
			{ RE::FormType::ImageAdapter, "IMAD"sv },
			{ RE::FormType::FormList, "FLST"sv },
			{ RE::FormType::Perk, "PERK"sv },
			{ RE::FormType::BodyPartData, "BPTD"sv },
			{ RE::FormType::AddonNode, "ADDN"sv },
			{ RE::FormType::ActorValueInfo, "AVIF"sv },
			{ RE::FormType::CameraShot, "CAMS"sv },
			{ RE::FormType::CameraPath, "CPTH"sv },
			{ RE::FormType::VoiceType, "VTYP"sv },
			{ RE::FormType::MaterialType, "MATT"sv },
			{ RE::FormType::Impact, "IPCT"sv },
			{ RE::FormType::ImpactDataSet, "IPDS"sv },
			{ RE::FormType::Armature, "ARMA"sv },
			{ RE::FormType::EncounterZone, "ECZN"sv },
			{ RE::FormType::Location, "LCTN"sv },
			{ RE::FormType::Message, "MESG"sv },
			{ RE::FormType::Ragdoll, "RGDL"sv },
			{ RE::FormType::DefaultObject, "DOBJ"sv },
			{ RE::FormType::LightingMaster, "LGTM"sv },
			{ RE::FormType::MusicType, "MUSC"sv },
			{ RE::FormType::Footstep, "FSTP"sv },
			{ RE::FormType::FootstepSet, "FSTS"sv },
			{ RE::FormType::StoryManagerBranchNode, "SMBN"sv },
			{ RE::FormType::StoryManagerQuestNode, "SMQN"sv },
			{ RE::FormType::StoryManagerEventNode, "SMEN"sv },
			{ RE::FormType::DialogueBranch, "DLBR"sv },
			{ RE::FormType::MusicTrack, "MUST"sv },
			{ RE::FormType::DialogueView, "DLVW"sv },
			{ RE::FormType::WordOfPower, "WOOP"sv },
			{ RE::FormType::Shout, "SHOU"sv },
			{ RE::FormType::EquipSlot, "EQUP"sv },
			{ RE::FormType::Relationship, "RELA"sv },
			{ RE::FormType::Scene, "SCEN"sv },
			{ RE::FormType::AssociationType, "ASTP"sv },
			{ RE::FormType::Outfit, "OTFT"sv },
			{ RE::FormType::ArtObject, "ARTO"sv },
			{ RE::FormType::MaterialObject, "MATO"sv },
			{ RE::FormType::MovementType, "MOVT"sv },
			{ RE::FormType::SoundRecord, "SNDR"sv },
			{ RE::FormType::DualCastData, "DUAL"sv },
			{ RE::FormType::SoundCategory, "SNCT"sv },
			{ RE::FormType::SoundOutputModel, "SOPM"sv },
			{ RE::FormType::CollisionLayer, "COLL"sv },
			{ RE::FormType::ColorForm, "CLFM"sv },
			{ RE::FormType::ReverbParam, "REVB"sv },
			{ RE::FormType::LensFlare, "LENS"sv },
			{ RE::FormType::LensSprite, "LSPR"sv },
			{ RE::FormType::VolumetricLighting, "VOLI"sv }
		};

		std::string GetWhitelistFormString(RE::FormType a_type);

		std::string GetBlacklistFormString(RE::FormType a_type);
	}
}
