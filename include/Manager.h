#pragma once

class Manager :
	public REX::Singleton<Manager>,
	public RE::BSTEventSink<RE::InputEvent*>,
	public RE::BSTEventSink<RE::MenuOpenCloseEvent>
{
public:
	static constexpr RE::FormID kJobInnkeeperFactionID{ 0x0005091B };
	static constexpr RE::FormID kJobInnServerFactionID{ 0x000DEE93 };
	static constexpr RE::FormID kJobMerchantFactionID{ 0x00051596 };

	void        LoadSettings();
	static void Register();

	[[nodiscard]] bool       IsVendorItem(RE::TESObjectREFR* a_ref) const;
	[[nodiscard]] RE::Actor* FindCellMerchant(RE::TESObjectREFR* a_ref) const;

	[[nodiscard]] Key                GetHotkey() const;
	[[nodiscard]] Key                GetHotkeyGamePad() const;
	[[nodiscard]] float              GetKeyHeldDuration() const;
	[[nodiscard]] bool               GetUseToggleMode() const;
	[[nodiscard]] const std::string& GetBuyLabel() const;
	[[nodiscard]] const std::string& GetInsufficientGoldMessage() const;
	[[nodiscard]] std::uint32_t      GetBuyCost(RE::TESObjectREFR* a_ref) const;
	[[nodiscard]] bool               GetGoldValueBelowBuyVisuals() const;

	[[nodiscard]] bool GetHotkeyPressed() const;
	void               SetHotkeyPressed(bool a_pressed);
	[[nodiscard]] bool GetHotkeyHeld() const;
	void               SetHotkeyHeld(bool a_held);

	void BuildShopFactionCache();

private:
	[[nodiscard]] bool IsMerchantNPC(RE::TESNPC* a_npc) const;
	[[nodiscard]] bool IsShopFaction(RE::TESFaction* a_faction) const;

	static void UpdateCrosshairs();

	RE::BSEventNotifyControl ProcessEvent(RE::InputEvent* const* a_evn, RE::BSTEventSource<RE::InputEvent*>*) override;
	RE::BSEventNotifyControl ProcessEvent(const RE::MenuOpenCloseEvent* a_event, RE::BSTEventSource<RE::MenuOpenCloseEvent>*) override;

	Key         hotKey{ 56 };
	Key         hotKeyGamePad{ 0 };
	float       keyHeldDuration{ 0.7f };
	bool        useToggleMode{ false };
	std::string buyLabel{ "Buy" };
	std::string insufficientGoldMessage{ "You don't have enough gold." };
	bool        goldValueBelowBuyVisuals{ false };

	std::atomic_bool keyPressed{ false };
	std::atomic_bool keyHeld{ false };
	std::atomic_bool toggleActive{ false };

	std::unordered_set<RE::FormID> shopFactionCache;
};
