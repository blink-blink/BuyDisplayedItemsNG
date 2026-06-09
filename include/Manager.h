#pragma once

#include "Settings.h"

class Manager :
	public REX::Singleton<Manager>,
	public RE::BSTEventSink<RE::InputEvent*>,
	public RE::BSTEventSink<RE::MenuOpenCloseEvent>
{
public:
	static constexpr RE::FormID kJobInnkeeperFactionID{ 0x0005091B };
	static constexpr RE::FormID kJobInnServerFactionID{ 0x000DEE93 };
	static constexpr RE::FormID kJobMerchantFactionID{ 0x00051596 };

	static void Register();

	[[nodiscard]] bool            IsVendorItem(RE::TESObjectREFR* a_ref);
	[[nodiscard]] RE::ActorHandle FindCellMerchant(RE::TESObjectREFR* a_ref) const;

	[[nodiscard]] std::uint32_t      GetBuyCost(RE::TESObjectREFR* a_ref) const;

	[[nodiscard]] bool GetHotkeyPressed() const;
	void               SetHotkeyPressed(bool a_pressed);
	[[nodiscard]] bool GetHotkeyHeld() const;
	void               SetHotkeyHeld(bool a_held);

	void BuildShopFactionCache();
	void BuildBuyPriceCache();
	void RequestBlacklistRebuild();

private:
	[[nodiscard]] bool IsMerchantNPC(RE::TESNPC* a_npc) const;
	[[nodiscard]] bool IsShopFaction(RE::TESFaction* a_faction) const;
	[[nodiscard]] bool IsBlacklistedNPC(RE::TESNPC* a_npc) const;

	static void UpdateCrosshairs();

	RE::BSEventNotifyControl ProcessEvent(RE::InputEvent* const* a_evn, RE::BSTEventSource<RE::InputEvent*>*) override;
	RE::BSEventNotifyControl ProcessEvent(const RE::MenuOpenCloseEvent* a_event, RE::BSTEventSource<RE::MenuOpenCloseEvent>*) override;

	std::atomic_bool keyPressed{ false };
	std::atomic_bool keyHeld{ false };
	std::atomic_bool toggleActive{ false };

	std::mutex cacheMutex;
	bool cacheDirty{ false };
	std::unordered_set<RE::FormID> shopFactionCache;
	std::unordered_set<RE::FormID> blacklistedFactionCache;
	std::unordered_set<RE::FormID> blacklistedNPCCache;

	struct CachedBuyModifier
	{
		RE::FormID  perkID;
		float       value;
		std::uint8_t function;
		std::uint8_t rank;
	};
	std::vector<CachedBuyModifier> buyPriceCache;
};
