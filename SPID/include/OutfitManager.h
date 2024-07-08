#pragma once

namespace Outfits
{
	class Manager : public ISingleton<Manager>
	{
	public:
		static void Register();

		bool IsLoadingGame() const
		{
			return isLoadingGame;
		}

		void StartLoadingGame()
		{
			isLoadingGame = true;
		}

		void FinishLoadingGame()
		{
			isLoadingGame = false;
		}

		/// <summary>
		/// Sets given outfit as default outfit for the actor.
		/// 
		/// This method also makes sure to properly remove previously distributed outfit.
		/// </summary>
		/// <param name="Actor">Target Actor for whom the outfit will be set.</param>
		/// <param name="Outfit">A new outfit to set as the default.</param>
		bool SetDefaultOutfit(RE::Actor*, RE::BGSOutfit*);

		/// <summary>
		/// Resets current default outfit for the actor.
		/// 
		/// Use this method when outfit distribution doesn't find any suitable Outfit for an NPC.
		/// In such cases we need to reset previously distributed outfit if any. 
		/// This is needed to preserve "runtime" behavior of the SPID, where SPID is expected to not leave any permanent changes.
		/// Due to the way outfits work, the only reliable way to equip those is to use game's equipping logic, which stores equipped outfit in a save file, and then clean-up afterwards :)
		/// 
		/// This method looks up any cached distributed outfits for this specific actor 
		/// and properly removes it from the NPC, then restores defaultOutfit that was used before the distribution.
		/// </summary>
		/// <param name="Actor">Target Actor for whom the outfit should be reset.</param>
		/// <param name="previous">Previously loaded outfit that needs to be unequipped.</param>
		void ResetDefaultOutfit(RE::Actor*, RE::BGSOutfit* previous);

	private:
		static void Load(SKSE::SerializationInterface*);
		static void Save(SKSE::SerializationInterface*);
		static void Revert(SKSE::SerializationInterface*);

		bool isLoadingGame = false;

		void ApplyDefaultOutfit(RE::Actor*);

		/// Map of Actor -> Outfit associations.
		std::unordered_map<RE::Actor*, RE::BGSOutfit*> outfits;
	};
}
