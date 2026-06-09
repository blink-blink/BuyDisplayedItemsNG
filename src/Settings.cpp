#include "pch.h"
#include "Settings.h"

Settings& Settings::GetSingleton()
{
	static Settings settings;
	return settings;
}

void Settings::Load()
{
	constexpr auto path = "Data/SKSE/Plugins/BuyDisplayedItems.ini";

	CSimpleIniA ini;
	ini.SetUnicode();
	ini.LoadFile(path);

	enableSideHustle           = ini.GetBoolValue("General", "bEnableSideHustle", false);
	oblivionInteractionIcons   = ini.GetBoolValue("General", "bOblivionInteractionIcons", false);
	buyLabel                   = ini.GetValue("General", "sBuyLabel", "Buy");
	goldValueBelowBuyVisuals   = ini.GetBoolValue("General", "bGoldValueBelowBuyVisuals", false);
	insufficientGoldMessage    = ini.GetValue("General", "sInsufficientGoldMessage", "You don't have enough gold.");
	enableToggleMode           = ini.GetBoolValue("General", "bEnableToggleMode", false);
	crouchForVanillaAction     = ini.GetBoolValue("General", "bCrouchForVanillaAction", false);
	enableDistanceToMerchant   = ini.GetBoolValue("General", "bEnableDistanceToMerchant", false);
	maxDistanceToMerchant      = static_cast<float>(ini.GetDoubleValue("General", "fMaxDistanceToMerchant", 0.0));
	priceMultiplier            = static_cast<float>(ini.GetDoubleValue("General", "fPriceMultiplier", 1.0));
	keyboardHotkey             = static_cast<std::uint32_t>(ini.GetLongValue("General", "uKeyboardHotkey", 42));
	gamepadHotkey              = static_cast<std::uint32_t>(ini.GetLongValue("General", "uGamepadHotkey", 272));
	debug                      = ini.GetBoolValue("General", "bDebug", false);

	const char* raw = ini.GetValue("General", "sBlacklistedNPCs", "Darnette Lauven,Millie Lauven,Varicio the Collector,Eriana,Eringoth,Yolanda,Egriss");
	blacklistedNPCs.clear();
	std::string rawStr(raw ? raw : "");
	std::istringstream stream(rawStr);
	std::string token;
	while (std::getline(stream, token, ',')) {
		auto start = token.find_first_not_of(" \t");
		auto end   = token.find_last_not_of(" \t");
		if (start == std::string::npos) continue;
		blacklistedNPCs.push_back(token.substr(start, end - start + 1));
	}

	logger::info("Settings loaded: sideHustle={} oii={} buyLabel='{}' goldBelow={} insuffMsg='{}' toggle={} crouch={} distCheck={} dist={:.2f} priceMult={:.2f} kbHotkey={} gpHotkey={} debug={} blacklisted={}",
		enableSideHustle, oblivionInteractionIcons, buyLabel, goldValueBelowBuyVisuals,
		insufficientGoldMessage, enableToggleMode, crouchForVanillaAction,
		enableDistanceToMerchant, maxDistanceToMerchant, priceMultiplier,
		keyboardHotkey, gamepadHotkey, debug, blacklistedNPCs.size());

	Save();
}

void Settings::Save()
{
	CSimpleIniA ini;
	ini.SetUnicode();

	ini.SetBoolValue("General", "bEnableSideHustle", enableSideHustle);
	ini.SetBoolValue("General", "bOblivionInteractionIcons", oblivionInteractionIcons);
	ini.SetValue("General", "sBuyLabel", buyLabel.c_str());
	ini.SetBoolValue("General", "bGoldValueBelowBuyVisuals", goldValueBelowBuyVisuals);
	ini.SetValue("General", "sInsufficientGoldMessage", insufficientGoldMessage.c_str());
	ini.SetBoolValue("General", "bEnableToggleMode", enableToggleMode);
	ini.SetBoolValue("General", "bCrouchForVanillaAction", crouchForVanillaAction);
	ini.SetBoolValue("General", "bEnableDistanceToMerchant", enableDistanceToMerchant);
	ini.SetDoubleValue("General", "fMaxDistanceToMerchant", maxDistanceToMerchant);
	ini.SetDoubleValue("General", "fPriceMultiplier", priceMultiplier);
	ini.SetLongValue("General", "uKeyboardHotkey", keyboardHotkey);
	ini.SetLongValue("General", "uGamepadHotkey", gamepadHotkey);
	ini.SetBoolValue("General", "bDebug", debug);

	std::string blacklistStr;
	for (std::size_t i = 0; i < blacklistedNPCs.size(); ++i) {
		if (i > 0) blacklistStr += ",";
		blacklistStr += blacklistedNPCs[i];
	}
	ini.SetValue("General", "sBlacklistedNPCs", blacklistStr.c_str());

	ini.SaveFile("Data/SKSE/Plugins/BuyDisplayedItems.ini");
}
