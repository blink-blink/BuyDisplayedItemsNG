#include "pch.h"
#include "UI.h"
#include "NPCRegistry.h"
#include "Settings.h"
#include "Strings.h"
#include "Manager.h"

namespace
{
	constexpr std::uint32_t MOUSE_OFFSET = 256;
	constexpr std::uint32_t GAMEPAD_OFFSET = 266;

	void SaveSettings()
	{
		Settings::GetSingleton().Save();
		Strings::SyncTranslation();
	}

	const char* dxKbNames[] = {
		"[NONE]",
		"Escape", "1", "2", "3", "4", "5", "6", "7", "8", "9", "0",
		"Minus", "Equals", "Backspace", "Tab",
		"Q", "W", "E", "R", "T", "Y", "U", "I", "O", "P",
		"Left Bracket", "Right Bracket", "Enter",
		"Left Control", "A", "S", "D", "F", "G", "H", "J", "K", "L",
		"Semicolon", "Apostrophe", "~ (Console)", "Left Shift", "Back Slash",
		"Z", "X", "C", "V", "B", "N", "M",
		"Comma", "Period", "Forward Slash", "Right Shift",
		"NUM*", "Left Alt", "Spacebar", "Caps Lock",
		"F1", "F2", "F3", "F4", "F5", "F6", "F7", "F8", "F9", "F10",
		"Num Lock", "Scroll Lock",
		"NUM7", "NUM8", "NUM9", "NUM-", "NUM4", "NUM5", "NUM6", "NUM+",
		"NUM1", "NUM2", "NUM3", "NUM0", "NUM.",
		"F11", "F12",
		"NUM Enter", "Right Control",
		"NUM/",
		"SysRq / PtrScr", "Right Alt",
		"Pause",
		"Home", "Up Arrow", "PgUp", "Left Arrow", "Right Arrow", "End", "Down Arrow", "PgDown",
		"Insert", "Delete",
		"Left Mouse Button", "Right Mouse Button", "Middle/Wheel Mouse Button",
		"Mouse Button 3", "Mouse Button 4", "Mouse Button 5",
		"Mouse Button 6", "Mouse Button 7", "Mouse Wheel Up", "Mouse Wheel Down"
	};

	const std::uint32_t dxKbValues[] = {
		0,
		1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11,
		12, 13, 14, 15,
		16, 17, 18, 19, 20, 21, 22, 23, 24, 25,
		26, 27, 28,
		29, 30, 31, 32, 33, 34, 35, 36, 37, 38,
		39, 40, 41, 42, 43,
		44, 45, 46, 47, 48, 49, 50,
		51, 52, 53, 54,
		55, 56, 57, 58,
		59, 60, 61, 62, 63, 64, 65, 66, 67, 68,
		69, 70,
		71, 72, 73, 74, 75, 76, 77, 78,
		79, 80, 81, 82, 83,
		87, 88,
		156, 157,
		181,
		183, 184,
		197,
		199, 200, 201, 203, 205, 207, 208, 209,
		210, 211,
		256, 257, 258,
		259, 260, 261,
		262, 263, 264, 265
	};

	const char* dxGpNames[] = {
		"[NONE]",
		"DPAD UP", "DPAD DOWN", "DPAD LEFT", "DPAD RIGHT",
		"START", "BACK",
		"LEFT THUMB", "RIGHT THUMB",
		"LEFT SHOULDER", "RIGHT SHOULDER",
		"A", "B", "X", "Y",
		"LT", "RT"
	};

	const std::uint32_t dxGpValues[] = {
		0,
		266, 267, 268, 269,
		270, 271,
		272, 273,
		274, 275,
		276, 277, 278, 279,
		280, 281
	};
}

const char* UI::GetKeyName(std::uint32_t a_key)
{
	if (a_key == 0) return "[NONE]";
	for (std::size_t i = 0; i < std::size(dxKbValues); ++i) {
		if (dxKbValues[i] == a_key) return dxKbNames[i];
	}
	for (std::size_t i = 0; i < std::size(dxGpValues); ++i) {
		if (dxGpValues[i] == a_key) return dxGpNames[i];
	}
	return "None";
}

void UI::HelpMarker(const char* a_desc)
{
    ImGui::PushStyleColor(ImGui::ImGuiCol_Text, ImGui::ImVec4{0.55f, 0.55f, 0.55f, 1.0f});
	ImGui::TextUnformatted("(?)");
	ImGui::PopStyleColor();
	if (ImGui::IsItemHovered()) {
		ImGui::BeginTooltip();
		ImGui::PushTextWrapPos(ImGui::GetFontSize() * 28.0f);
		ImGui::TextUnformatted(a_desc);
		ImGui::PopTextWrapPos();
		ImGui::EndTooltip();
	}
}

void UI::Register()
{
	if (!SKSEMenuFramework::IsInstalled()) {
		logger::warn("UI: SKSE Menu Framework not installed");
		return;
	}

	SKSEMenuFramework::SetSection(Strings::Get("ModName"));
	SKSEMenuFramework::AddSectionItem(Strings::Get("SettingsLabel"), RenderMenu);
	logger::info("UI: registered with SKSE Menu Framework");
}

static int FindComboIndex(const std::uint32_t* a_values, std::size_t a_count, std::uint32_t a_current)
{
	for (std::size_t i = 0; i < a_count; ++i) {
		if (a_values[i] == a_current) return static_cast<int>(i);
	}
	return -1;
}

void __stdcall UI::RenderMenu()
{
	auto& settings = Settings::GetSingleton();
	bool changed = false;

	float winWidth = ImGui::GetWindowWidth();

	ImGui::TextUnformatted(Strings::Get("GeneralSection"));
	ImGui::Separator();

	if (ImGui::Checkbox(Strings::Get("SideHustleCheckbox"), &settings.enableSideHustle)) {
		changed = true;
	}
	ImGui::SameLine();
	HelpMarker(Strings::Get("SideHustleHelp"));

	if (ImGui::Checkbox(Strings::Get("OIICompatCheckbox"), &settings.oblivionInteractionIcons)) {
		changed = true;
	}
	ImGui::SameLine();
	HelpMarker(Strings::Get("OIICompatHelp"));

	ImGui::TextUnformatted(Strings::Get("BuyLabelInput"));
	ImGui::SameLine();
	HelpMarker(Strings::Get("BuyLabelHelp"));

	if (_buyLabelBuf[0] == '\0' && !settings.buyLabel.empty()) {
		std::strncpy(_buyLabelBuf, settings.buyLabel.c_str(), sizeof(_buyLabelBuf) - 1);
	}
	ImGui::SetNextItemWidth(winWidth * 0.4f);
	if (ImGui::InputText("##buylabel", _buyLabelBuf, sizeof(_buyLabelBuf))) {
		settings.buyLabel = _buyLabelBuf;
		changed = true;
	}
	ImGui::SameLine();
	if (ImGui::Button("Save")) {
		settings.buyLabel = _buyLabelBuf;
		Strings::Set("BuyLabelInput", settings.buyLabel);
		SaveSettings();
		_buyLabelBuf[0] = '\0';
	}

	if (ImGui::Checkbox(Strings::Get("GoldBelowVisuals"), &settings.goldValueBelowBuyVisuals)) {
		changed = true;
	}
	ImGui::SameLine();
	HelpMarker(Strings::Get("GoldBelowVisualsHelp"));

	ImGui::TextUnformatted(Strings::Get("InsuffGoldMessage"));
	ImGui::SameLine();
	HelpMarker(Strings::Get("InsuffGoldMessageHelp"));

	if (_insuffGoldBuf[0] == '\0' && !settings.insufficientGoldMessage.empty()) {
		std::strncpy(_insuffGoldBuf, settings.insufficientGoldMessage.c_str(), sizeof(_insuffGoldBuf) - 1);
	}
	ImGui::SetNextItemWidth(winWidth * 0.4f);
	if (ImGui::InputText("##insuffgold", _insuffGoldBuf, sizeof(_insuffGoldBuf))) {
		settings.insufficientGoldMessage = _insuffGoldBuf;
		changed = true;
	}
	ImGui::SameLine();
	if (ImGui::Button("Save")) {
		settings.insufficientGoldMessage = _insuffGoldBuf;
		Strings::Set("InsuffGoldMessage", settings.insufficientGoldMessage);
		SaveSettings();
		_insuffGoldBuf[0] = '\0';
	}

	ImGui::TextUnformatted(Strings::Get("HotkeysSection"));
	ImGui::Separator();

	constexpr auto kbComboCount = std::size(dxKbValues);
	ImGui::TextUnformatted(Strings::Get("KBHotkeyLabel"));
	ImGui::SameLine();
	HelpMarker(Strings::Get("KBHotkeyHelp"));
	ImGui::SetNextItemWidth(winWidth * 0.4f);
	int kbIdx = FindComboIndex(dxKbValues, kbComboCount, settings.keyboardHotkey);
	if (kbIdx < 0) kbIdx = 0;
	if (ImGui::Combo("##kbhotkey", &kbIdx, dxKbNames, static_cast<int>(kbComboCount))) {
		settings.keyboardHotkey = dxKbValues[kbIdx];
		changed = true;
	}

	constexpr auto gpComboCount = std::size(dxGpValues);
	ImGui::TextUnformatted(Strings::Get("GPHotkeyLabel"));
	ImGui::SameLine();
	HelpMarker(Strings::Get("GPHotkeyHelp"));
	ImGui::SetNextItemWidth(winWidth * 0.4f);
	int gpIdx = FindComboIndex(dxGpValues, gpComboCount, settings.gamepadHotkey);
	if (gpIdx < 0) gpIdx = 0;
	if (ImGui::Combo("##gphotkey", &gpIdx, dxGpNames, static_cast<int>(gpComboCount))) {
		settings.gamepadHotkey = dxGpValues[gpIdx];
		changed = true;
	}

	if (ImGui::Checkbox(Strings::Get("ToggleModeCheckbox"), &settings.enableToggleMode)) {
		changed = true;
	}
	ImGui::SameLine();
	HelpMarker(Strings::Get("ToggleModeHelp"));

	if (ImGui::Checkbox(Strings::Get("CrouchCheckbox"), &settings.crouchForVanillaAction)) {
		changed = true;
	}
	ImGui::SameLine();
	HelpMarker(Strings::Get("CrouchHelp"));

	ImGui::TextUnformatted(Strings::Get("DistanceSection"));
	ImGui::Separator();

	if (ImGui::Checkbox(Strings::Get("DistanceCheckbox"), &settings.enableDistanceToMerchant)) {
		changed = true;
	}
	ImGui::SameLine();
	HelpMarker(Strings::Get("DistanceCheckboxHelp"));

	if (settings.enableDistanceToMerchant) {
		ImGui::Indent();
		if (ImGui::SliderFloat(Strings::Get("DistanceInput"), &settings.maxDistanceToMerchant, 100.0f, 10000.0f, "%.0f")) {
			changed = true;
		}
		ImGui::Unindent();
	}

	ImGui::TextUnformatted(Strings::Get("PricingSection"));
	ImGui::Separator();

	if (ImGui::SliderFloat(Strings::Get("PriceMultiplier"), &settings.priceMultiplier, 0.1f, 10.0f, "%.1f")) {
		changed = true;
	}
	ImGui::SameLine();
	HelpMarker(Strings::Get("PriceMultiplierHelp"));

	ImGui::TextUnformatted(Strings::Get("DebugSection"));
	ImGui::Separator();
	if (ImGui::Checkbox(Strings::Get("DebugCheckbox"), &settings.debug)) {
		changed = true;
	}

	if (changed) {
		SaveSettings();
	}

	ImGui::TextUnformatted(Strings::Get("BlacklistSection"));
	ImGui::Separator();

	if (settings.enableSideHustle) {
		ImGui::TextUnformatted(Strings::Get("BlacklistSectionHelp"));
	} else {
		ImGui::SameLine();
		HelpMarker(Strings::Get("BlacklistSectionHelp"));

		ImGui::SetNextItemWidth(winWidth * 0.4f);
		if (ImGui::InputText("##blfilter", _blacklistFilterBuf, sizeof(_blacklistFilterBuf))) {}
		ImGui::SameLine();
		HelpMarker(Strings::Get("AddButtonHelp"));

		const auto& npcEntries = NPCRegistry::GetSingleton().GetEntries();
		auto filteredIndices = NPCRegistry::GetSingleton().Filter(_blacklistFilterBuf);
		static int selectedNpcIdx = 0;

		const char* preview = "Select NPC...";
		if (selectedNpcIdx >= 0 && static_cast<std::size_t>(selectedNpcIdx) < filteredIndices.size()) {
			preview = npcEntries[filteredIndices[selectedNpcIdx]].comboLabel.c_str();
		}

		ImGui::SetNextItemWidth(winWidth * 0.4f);
		if (ImGui::BeginCombo("##blacklist_npc_dropdown", preview)) {
			for (std::size_t i = 0; i < filteredIndices.size(); ++i) {
				const auto& npc = npcEntries[filteredIndices[i]];
				const bool isSelected = (static_cast<std::size_t>(selectedNpcIdx) == i);
				if (ImGui::Selectable(npc.comboLabel.c_str(), isSelected)) {
					selectedNpcIdx = static_cast<int>(i);
				}
				if (isSelected) ImGui::SetItemDefaultFocus();
			}
			ImGui::EndCombo();
		}

		ImGui::SameLine();
		if (ImGui::Button(Strings::Get("AddButton"))) {
			if (selectedNpcIdx >= 0 && static_cast<std::size_t>(selectedNpcIdx) < filteredIndices.size()) {
				const auto& npc = npcEntries[filteredIndices[selectedNpcIdx]];
				bool found = false;
				for (const auto& name : settings.blacklistedNPCs) {
					if (name == npc.editorID) { found = true; break; }
				}
				if (!found) {
					settings.blacklistedNPCs.push_back(npc.editorID);
					Manager::GetSingleton()->RequestBlacklistRebuild();
					changed = true;
				}
			}
		}

		ImGui::TextUnformatted(Strings::Get("BlacklistedSection"));
		for (std::size_t i = 0; i < settings.blacklistedNPCs.size(); ++i) {
			ImGui::TextUnformatted(settings.blacklistedNPCs[i].c_str());
			ImGui::SameLine();
			ImGui::PushID(static_cast<int>(i));
            ImGui::PushStyleColor(ImGui::ImGuiCol_Text, ImGui::ImVec4(1.0f, 0.2f, 0.2f, 1.0f));
			if (ImGui::SmallButton("[X]")) {
				settings.blacklistedNPCs.erase(settings.blacklistedNPCs.begin() + static_cast<std::ptrdiff_t>(i));
				Manager::GetSingleton()->RequestBlacklistRebuild();
				changed = true;
				ImGui::PopStyleColor();
				ImGui::PopID();
				break;
			}
			ImGui::PopStyleColor();
			ImGui::PopID();
		}
	}
}
