#pragma once

#include <string>
#include <vector>

class Settings
{
public:
	static Settings& GetSingleton();

	void Load();
	void Save();

	bool        enableSideHustle{ false };
	bool        oblivionInteractionIcons{ false };
	std::string buyLabel{ "Buy" };
	bool        goldValueBelowBuyVisuals{ false };
	std::string insufficientGoldMessage{ "You don't have enough gold." };
	bool        enableToggleMode{ false };
	bool        crouchForVanillaAction{ false };
	bool        enableDistanceToMerchant{ false };
	float       maxDistanceToMerchant{ 0.0f };
	float       priceMultiplier{ 1.0f };
	std::uint32_t keyboardHotkey{ 42 };
	std::uint32_t gamepadHotkey{ 272 };
	std::vector<std::string> blacklistedNPCs;
	bool        debug{ false };

private:
	Settings() = default;
};
