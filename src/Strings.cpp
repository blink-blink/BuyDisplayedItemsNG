#include "pch.h"
#include "Strings.h"

std::unordered_map<std::string, std::string> Strings::_map;

void Strings::Load()
{
	_map.clear();

	_map["ModName"] = "Buy Displayed Items";
	_map["SettingsLabel"] = "Settings";
	_map["GeneralSection"] = "--- General ---";
	_map["SideHustleCheckbox"] = "Enable Side Hustle";
	_map["SideHustleHelp"] = "Make every owned item buyable, not just those owned by merchants.";
	_map["OIICompatCheckbox"] = "Oblivion Interaction Icons";
	_map["OIICompatHelp"] = "Adds a hand icon above the buy prompt. Requires Oblivion Interaction Icons mod.";
	_map["BuyLabelInput"] = "Buy Label Text";
	_map["BuyLabelHelp"] = "Custom text on buyable items. Shows 'Buy' by default.";
	_map["GoldBelowVisuals"] = "Gold on Separate Line";
	_map["GoldBelowVisualsHelp"] = "Display the gold cost below the buy label instead of next to it.";
	_map["InsuffGoldMessage"] = "Not Enough Gold Message";
	_map["InsuffGoldMessageHelp"] = "Notification shown when you cannot afford an item.";
	_map["HotkeysSection"] = "--- Hotkeys ---";
	_map["KBHotkeyLabel"] = "Keyboard / Mouse Hotkey";
	_map["KBHotkeyHelp"] = "Key to hold or toggle. While active, items use Take/Steal instead of Buy.";
	_map["GPHotkeyLabel"] = "Gamepad Hotkey";
	_map["GPHotkeyHelp"] = "Gamepad button to hold or toggle for Take/Steal behavior.";
	_map["SaveButton"] = "Save";
	_map["ToggleModeCheckbox"] = "Toggle Instead of Hold";
	_map["ToggleModeHelp"] = "Press hotkey once to switch between Buy and Take. Press again to switch back.";
	_map["CrouchCheckbox"] = "Sneak to Use Take/Steal";
	_map["CrouchHelp"] = "While sneaking, items show Take/Steal. Stand up to show Buy prices.";
	_map["BlacklistSection"] = "--- Blacklist ---";
	_map["BlacklistSectionHelp"] = "Prevent specific merchants from selling items through the buy system. Ignored when Side Hustle is on.";
	_map["NPCFilter"] = "Search Merchants";
	_map["NPCFilterHelp"] = "Type a merchant's name to find them in the list.";
	_map["NPCDropdown"] = "Select Merchant";
	_map["NPCDropdownHelp"] = "Choose a merchant to add to your blacklist.";
	_map["AddButton"] = "Add to Blacklist";
	_map["AddButtonHelp"] = "Prevent this merchant's items from being buyable.";
	_map["BlacklistedSection"] = "Blacklisted Merchants";
	_map["RemoveButtonHelp"] = "Remove this merchant from the blacklist.";
	_map["NoNPCSelected"] = "No merchant selected";
	_map["NoNPCMatches"] = "No matching merchants found";
	_map["DistanceSection"] = "--- Distance ---";
	_map["DistanceCheckbox"] = "Distance to Merchant Check";
	_map["DistanceCheckboxHelp"] = "Only show buy option when close enough to the merchant.";
	_map["DistanceInput"] = "Maximum Distance";
	_map["DistanceInputHelp"] = "Items beyond this distance from the merchant will not be buyable.";
	_map["PricingSection"] = "--- Pricing ---";
	_map["PriceMultiplier"] = "Buy Price Multiplier";
	_map["PriceMultiplierHelp"] = "Adjusts the final buy price. 1.0 = normal, 2.0 = double, 0.5 = half.";
	_map["DebugCheckbox"] = "Debug Logging";
	_map["DebugHelp"] = "Write extra information to the SKSE log for troubleshooting.";
	_map["DebugSection"] = "--- Debug ---";

	constexpr auto path = "Data/SKSE/Plugins/BuyDisplayedItems_Translation.ini";
	if (!std::filesystem::exists(path)) {
		logger::info("Strings: translation file missing, using built-in English");
		SyncTranslation();
		return;
	}

	CSimpleIniA ini;
	ini.SetUnicode();
	if (ini.LoadFile(path) != SI_OK) {
		logger::warn("Strings: translation file exists but failed to parse");
		return;
	}

	CSimpleIniA::TNamesDepend keys;
	if (ini.GetAllKeys("Strings", keys)) {
		for (const auto& key : keys) {
			if (const char* val = ini.GetValue("Strings", key.pItem)) {
				_map[key.pItem] = val;
			}
		}
	}

	logger::info("Strings: translation overrides loaded");
}

const char* Strings::Get(std::string_view a_key)
{
	const auto it = _map.find(std::string(a_key));
	return it != _map.end() ? it->second.c_str() : "";
}

void Strings::Set(std::string_view a_key, std::string_view a_value)
{
	_map[std::string(a_key)] = std::string(a_value);
}

void Strings::SyncTranslation()
{
	constexpr auto path = "Data/SKSE/Plugins/BuyDisplayedItems_Translation.ini";

	CSimpleIniA ini;
	ini.SetUnicode();

	for (const auto& [key, value] : _map) {
		ini.SetValue("Strings", key.c_str(), value.c_str());
	}

	ini.SaveFile(path);
}
