#include "Manager.h"

void Manager::LoadSettings()
{
	const auto path = std::format("Data/SKSE/Plugins/{}.ini", Version::PROJECT);

	CSimpleIniA ini;
	ini.SetUnicode();
	ini.LoadFile(path.c_str());

	auto readKey = [&](const char* section, const char* key, Key defaultVal, const char* comment) -> Key {
		const char* raw = ini.GetValue(section, key, nullptr);
		Key val = raw ? static_cast<Key>(std::stoul(raw)) : defaultVal;
		ini.SetValue(section, key, std::to_string(val).c_str(), comment);
		return val;
	};

	auto readFloat = [&](const char* section, const char* key, float defaultVal, const char* comment) -> float {
		const char* raw = ini.GetValue(section, key, nullptr);
		float val = raw ? std::stof(raw) : defaultVal;
		ini.SetValue(section, key, std::format("{:.2f}", val).c_str(), comment);
		return val;
	};

	auto readBool = [&](const char* section, const char* key, bool defaultVal, const char* comment) -> bool {
		const char* raw = ini.GetValue(section, key, nullptr);
		bool val = raw ? (std::string(raw) == "true" || std::string(raw) == "1") : defaultVal;
		ini.SetValue(section, key, val ? "true" : "false", comment);
		return val;
	};

	auto readString = [&](const char* section, const char* key, const char* defaultVal, const char* comment) -> std::string {
		const char* raw = ini.GetValue(section, key, nullptr);
		std::string val = raw ? raw : defaultVal;
		ini.SetValue(section, key, val.c_str(), comment);
		return val;
	};

	hotKey = readKey("Settings", "Vanilla action hotkey", 56,
		";Hold this key then press Activate to use vanilla Take/Steal instead of Buy.\n"
		";Default: 56 = Left Alt\n"
		";DXScanCodes: https://ck.uesp.net/wiki/Input_Script");

	hotKeyGamePad = readKey("Settings", "Vanilla action hotkey (Gamepad)", 0,
		";Gamepad button code for the same override. 0 = disabled.");

	keyHeldDuration = readFloat("Settings", "Hotkey hold duration", 0.7f,
		";Seconds the hotkey must be held before the 'held' state triggers.\n"
		";Only used when UseToggleMode = false");

	useToggleMode = readBool("Settings", "UseToggleMode", false,
		";If true, the hotkey toggles between Buy and Take/Steal modes.\n"
		";If false, the hotkey must be held (with hold duration).");

	buyLabel = readString("Settings", "Buy label", "Buy",
		";Text shown on the crosshair prompt for purchasable items.");

	insufficientGoldMessage = readString("Settings", "Insufficient gold message", "You don't have enough gold.",
		";Message shown when the player cannot afford a vendor item.");

	goldValueBelowBuyVisuals = readBool("Settings", "Gold Value Below Buy Visuals", false,
		";If true, the gold cost is displayed on its own line below the buy label.\n"
		";If false, the gold cost is displayed on the same line as the buy label.");

	ini.SaveFile(path.c_str());
}

void Manager::Register()
{
	logger::info("{:*^30}", "EVENTS");

	if (const auto inputMgr = RE::BSInputDeviceManager::GetSingleton()) {
		inputMgr->AddEventSink(GetSingleton());
		logger::info("Registered for hotkey event");
	}

	if (const auto ui = RE::UI::GetSingleton()) {
		ui->AddEventSink<RE::MenuOpenCloseEvent>(GetSingleton());
		logger::info("Registered for menu open/close event");
	}
}

bool Manager::IsMerchantNPC(RE::TESNPC* a_npc) const
{
	if (!a_npc) {
		return false;
	}

	const auto innkeeperFaction = RE::TESForm::LookupByID<RE::TESFaction>(kJobInnkeeperFactionID);
	const auto innServerFaction = RE::TESForm::LookupByID<RE::TESFaction>(kJobInnServerFactionID);
	const auto merchantFaction  = RE::TESForm::LookupByID<RE::TESFaction>(kJobMerchantFactionID);

	for (const auto& factionRank : a_npc->factions) {
		const auto faction = factionRank.faction;
		if (!faction) {
			continue;
		}
		if ((innkeeperFaction && faction == innkeeperFaction) ||
			(innServerFaction && faction == innServerFaction) ||
			(merchantFaction  && faction == merchantFaction)) {
			return true;
		}
	}

	return false;
}

bool Manager::IsShopFaction(RE::TESFaction* a_faction) const
{
	if (!a_faction) {
		return false;
	}
	return shopFactionCache.contains(a_faction->GetFormID());
}

void Manager::BuildShopFactionCache()
{
	logger::info("{:*^30}", "CACHE");

	const auto dataHandler = RE::TESDataHandler::GetSingleton();
	if (!dataHandler) {
		return;
	}

	std::uint32_t npcCount     = 0;
	std::uint32_t factionCount = 0;

	for (const auto& npcForm : dataHandler->GetFormArray<RE::TESNPC>()) {
		if (!npcForm) {
			continue;
		}

		if (IsMerchantNPC(npcForm)) {
			npcCount++;
			for (const auto& factionRank : npcForm->factions) {
				if (factionRank.faction) {
					const auto id = factionRank.faction->GetFormID();
					if (!shopFactionCache.contains(id)) {
						shopFactionCache.insert(id);
						factionCount++;
					}
				}
			}
		}
	}

	logger::info("Shop faction cache built: {} merchant NPCs, {} factions cached",
		npcCount, factionCount);
}

bool Manager::IsVendorItem(RE::TESObjectREFR* a_ref) const
{
	if (!a_ref) {
		return false;
	}

	if (const auto owner = a_ref->extraList.GetOwner()) {
		if (const auto npc = owner->As<RE::TESNPC>()) {
			return IsMerchantNPC(npc);
		}
		if (const auto faction = owner->As<RE::TESFaction>()) {
			return IsShopFaction(faction);
		}
	}

	if (const auto actorOwner = a_ref->GetActorOwner()) {
		if (const auto npc = actorOwner->As<RE::TESNPC>()) {
			return IsMerchantNPC(npc);
		}
	}

	auto* cell = a_ref->GetParentCell();
	if (cell) {
		if (const auto* cellOwner = cell->GetOwner()) {
			if (cellOwner->As<RE::TESFaction>()) {
				if (FindCellMerchant(a_ref)) {
					return true;
				}
			}
		}
	}

	return false;
}

RE::Actor* Manager::FindCellMerchant(RE::TESObjectREFR* a_ref) const
{
	if (!a_ref) {
		return nullptr;
	}

	auto* cell = a_ref->GetParentCell();
	if (!cell) {
		return nullptr;
	}

	const auto* cellOwner = cell->GetOwner();
	if (!cellOwner) {
		return nullptr;
	}

	const auto* ownerFaction = cellOwner->As<RE::TESFaction>();
	if (!ownerFaction) {
		return nullptr;
	}

	for (const auto& refHandle : cell->GetRuntimeData().references) {
		const auto ref = refHandle.get();
		if (!ref) { continue; }
		auto* actor = ref->As<RE::Actor>();
		if (!actor || actor->IsDead()) { continue; }
		auto* npc = actor->GetActorBase();
		if (!npc) { continue; }
		if (!IsMerchantNPC(npc)) { continue; }
		for (const auto& factionRank : npc->factions) {
			if (factionRank.faction == ownerFaction) {
				return actor;
			}
		}
	}

	return nullptr;
}

std::uint32_t Manager::GetBuyCost(RE::TESObjectREFR* a_ref) const
{
	if (!a_ref) {
		return 0;
	}
	const auto base = a_ref->GetBaseObject();
	if (!base) {
		return 0;
	}
	const auto bounded = base->As<RE::TESBoundObject>();
	if (!bounded) {
		return 0;
	}

	const std::uint32_t baseVal = bounded->GetGoldValue() > 0 ? bounded->GetGoldValue() : 1;

	auto* settings = RE::GameSettingCollection::GetSingleton();

	auto* barterMaxSetting = settings->GetSetting("fBarterMax");
	auto* barterMinSetting = settings->GetSetting("fBarterMin");
	const float barterMax  = barterMaxSetting ? barterMaxSetting->GetFloat() : 3.3f;
	const float barterMin  = barterMinSetting ? barterMinSetting->GetFloat() : 2.0f;

	const auto  player = RE::PlayerCharacter::GetSingleton();
	const float speech = (player && player->AsActorValueOwner())
	                       ? player->AsActorValueOwner()->GetActorValue(RE::ActorValue::kSpeech)
	                       : 0.0f;

	const float skillCapped     = speech < 100.0f ? speech : 100.0f;
	const float basePriceFactor = barterMax - (barterMax - barterMin) * (skillCapped / 100.0f);

	const auto          rawCount = a_ref->extraList.GetCount();
	const std::uint32_t count    = static_cast<std::uint32_t>(rawCount > 1 ? rawCount : 1);

	const float unitPrice = static_cast<float>(baseVal) * basePriceFactor;
	return static_cast<std::uint32_t>(std::round(unitPrice)) * count;
}

Key                Manager::GetHotkey() const { return hotKey; }
Key                Manager::GetHotkeyGamePad() const { return hotKeyGamePad; }
float              Manager::GetKeyHeldDuration() const { return keyHeldDuration; }
bool               Manager::GetUseToggleMode() const { return useToggleMode; }
const std::string& Manager::GetBuyLabel() const { return buyLabel; }
const std::string& Manager::GetInsufficientGoldMessage() const { return insufficientGoldMessage; }
bool               Manager::GetGoldValueBelowBuyVisuals() const { return goldValueBelowBuyVisuals; }
bool               Manager::GetHotkeyPressed() const { return keyPressed; }
void               Manager::SetHotkeyPressed(bool a_pressed) { keyPressed = a_pressed; }
bool               Manager::GetHotkeyHeld() const { return keyHeld; }
void               Manager::SetHotkeyHeld(bool a_held) { keyHeld = a_held; }

void Manager::UpdateCrosshairs()
{
	if (const auto crossHairPickData = RE::CrosshairPickData::GetSingleton()) {
		if (crossHairPickData->target) {
			RE::PlayerCharacter::GetSingleton()->UpdateCrosshairs();
		}
	}
}

RE::BSEventNotifyControl Manager::ProcessEvent(const RE::MenuOpenCloseEvent* a_event, RE::BSTEventSource<RE::MenuOpenCloseEvent>*)
{
	if (!a_event || a_event->opening) {
		return RE::BSEventNotifyControl::kContinue;
	}

	if (!useToggleMode) {
		SetHotkeyPressed(false);
		SetHotkeyHeld(false);
		UpdateCrosshairs();
	}

	return RE::BSEventNotifyControl::kContinue;
}

RE::BSEventNotifyControl Manager::ProcessEvent(RE::InputEvent* const* a_evn, RE::BSTEventSource<RE::InputEvent*>*)
{
	if (!a_evn) {
		return RE::BSEventNotifyControl::kContinue;
	}

	const auto player = RE::PlayerCharacter::GetSingleton();
	if (!player || !player->Is3DLoaded()) {
		return RE::BSEventNotifyControl::kContinue;
	}

	if (const auto UI = RE::UI::GetSingleton();
		!UI || UI->IsMenuOpen(RE::Console::MENU_NAME) || UI->GameIsPaused()) {
		return RE::BSEventNotifyControl::kContinue;
	}

	for (auto event = *a_evn; event; event = event->next) {
		if (const auto buttonEvent = event->AsButtonEvent()) {
			const auto device = event->GetDevice();
			auto       key    = buttonEvent->GetIDCode();

			switch (device) {
			case RE::INPUT_DEVICE::kMouse:
				key += SKSE::InputMap::kMacro_MouseButtonOffset;
				break;
			case RE::INPUT_DEVICE::kGamepad:
				key = SKSE::InputMap::GamepadMaskToKeycode(key);
				break;
			default:
				break;
			}

			const bool matchesKey = (key == GetHotkey()) ||
			                        (device == RE::INPUT_DEVICE::kGamepad && key == GetHotkeyGamePad());

			if (matchesKey) {
				if (useToggleMode) {
					if (buttonEvent->IsPressed() && !buttonEvent->IsRepeating()) {
						toggleActive = !toggleActive;
						SetHotkeyPressed(toggleActive);
						SetHotkeyHeld(toggleActive);
						UpdateCrosshairs();
					}
				} else {
					if (GetHotkeyPressed() != buttonEvent->IsPressed()) {
						SetHotkeyPressed(buttonEvent->IsPressed());
						if (!buttonEvent->IsPressed()) {
							SetHotkeyHeld(false);
						}
						UpdateCrosshairs();
					} else if (GetHotkeyPressed() && !GetHotkeyHeld() &&
					           buttonEvent->HeldDuration() > GetKeyHeldDuration()) {
						SetHotkeyHeld(true);
						UpdateCrosshairs();
					} else if (!buttonEvent->IsPressed()) {
						SetHotkeyPressed(false);
						SetHotkeyHeld(false);
					}
				}
			}
		}
	}

	return RE::BSEventNotifyControl::kContinue;
}
