#include "Manager.h"

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
	if (!a_npc) return false;

	const auto innkeeperFaction = RE::TESForm::LookupByID<RE::TESFaction>(kJobInnkeeperFactionID);
	const auto innServerFaction = RE::TESForm::LookupByID<RE::TESFaction>(kJobInnServerFactionID);
	const auto merchantFaction  = RE::TESForm::LookupByID<RE::TESFaction>(kJobMerchantFactionID);

	for (const auto& factionRank : a_npc->factions) {
		const auto faction = factionRank.faction;
		if (!faction) continue;
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
	if (!a_faction) return false;
	return shopFactionCache.contains(a_faction->GetFormID());
}

bool Manager::IsBlacklistedNPC(RE::TESNPC* a_npc) const
{
	const auto& settings = Settings::GetSingleton();
	if (!a_npc || settings.blacklistedNPCs.empty()) return false;

	const char* rawName = a_npc->GetFullName();
	if (!rawName || rawName[0] == '\0') return false;

	for (const auto& pattern : settings.blacklistedNPCs) {
		std::string lowerName(rawName);
		std::string lowerPattern = pattern;
		std::transform(lowerName.begin(), lowerName.end(), lowerName.begin(),
			[](unsigned char c) { return static_cast<char>(std::tolower(c)); });
		std::transform(lowerPattern.begin(), lowerPattern.end(), lowerPattern.begin(),
			[](unsigned char c) { return static_cast<char>(std::tolower(c)); });
		if (lowerName.find(lowerPattern) != std::string::npos) return true;
	}

	return false;
}

void Manager::RequestBlacklistRebuild()
{
	std::lock_guard lock(cacheMutex);
	cacheDirty = true;
	SKSE::GetTaskInterface()->AddTask([this]() {
		BuildShopFactionCache();
	});
}

void Manager::BuildShopFactionCache()
{
	std::lock_guard lock(cacheMutex);
	logger::info("{:*^30}", "CACHE");

	const auto dataHandler = RE::TESDataHandler::GetSingleton();
	if (!dataHandler) return;

	shopFactionCache.clear();
	blacklistedFactionCache.clear();
	blacklistedNPCCache.clear();

	std::uint32_t npcCount         = 0;
	std::uint32_t factionCount     = 0;
	std::uint32_t blacklistedNPCs  = 0;
	std::uint32_t blacklistedFacts = 0;

	for (const auto& pattern : Settings::GetSingleton().blacklistedNPCs) {
		auto* resolvedNPC = RE::TESForm::LookupByEditorID<RE::TESNPC>(pattern);
		if (resolvedNPC && IsMerchantNPC(resolvedNPC)) {
			blacklistedNPCCache.insert(resolvedNPC->GetFormID());
			blacklistedNPCs++;
			for (const auto& factionRank : resolvedNPC->factions) {
				if (!factionRank.faction) continue;
				const auto id = factionRank.faction->GetFormID();
				if (!blacklistedFactionCache.contains(id)) {
					blacklistedFactionCache.insert(id);
					blacklistedFacts++;
				}
			}
		}
	}

	for (const auto& npcForm : dataHandler->GetFormArray<RE::TESNPC>()) {
		if (!npcForm || !IsMerchantNPC(npcForm)) continue;

		npcCount++;

		const bool blacklisted = IsBlacklistedNPC(npcForm);

		if (blacklisted) {
			logger::info("Blacklisted NPC: '{}' editorID='{}' form={:08X}", npcForm->GetFullName(), npcForm->GetFormEditorID(), npcForm->GetFormID());
			blacklistedNPCCache.insert(npcForm->GetFormID());
			blacklistedNPCs++;
		}

		for (const auto& factionRank : npcForm->factions) {
			if (!factionRank.faction) continue;
			const auto id = factionRank.faction->GetFormID();
			if (!shopFactionCache.contains(id)) {
				shopFactionCache.insert(id);
				factionCount++;
			}
			if (blacklisted && !blacklistedFactionCache.contains(id)) {
				blacklistedFactionCache.insert(id);
				blacklistedFacts++;
			}
		}
	}

	cacheDirty = false;

	logger::info("Shop faction cache built: {} merchant NPCs, {} factions cached", npcCount, factionCount);
	logger::info("Blacklist cache built: {} blacklisted NPCs, {} blacklisted factions", blacklistedNPCs, blacklistedFacts);
}

void Manager::BuildBuyPriceCache()
{
	buyPriceCache.clear();

	const auto dataHandler = RE::TESDataHandler::GetSingleton();
	if (!dataHandler) return;

	for (const auto& perkForm : dataHandler->GetFormArray<RE::BGSPerk>()) {
		if (!perkForm) continue;
		for (auto& entry : perkForm->perkEntries) {
			if (entry->GetType() != RE::PERK_ENTRY_TYPE::kEntryPoint) continue;
			const auto epEntry = static_cast<RE::BGSEntryPointPerkEntry*>(entry);
			if (epEntry->entryData.entryPoint != RE::BGSEntryPoint::ENTRY_POINTS::kModBuyPrices) continue;
			if (!epEntry->functionData) continue;
			const auto oneVal = static_cast<RE::BGSEntryPointFunctionDataOneValue*>(epEntry->functionData);
			buyPriceCache.push_back({ perkForm->GetFormID(), oneVal->data, static_cast<std::uint8_t>(*epEntry->entryData.function), entry->header.rank });
		}
	}

	logger::info("Buy price cache built: {} perk entries from {} perks", buyPriceCache.size(),
		[&]() { std::unordered_set<RE::FormID> unique; for (auto& c : buyPriceCache) unique.insert(c.perkID); return unique.size(); }());
}

bool Manager::IsVendorItem(RE::TESObjectREFR* a_ref)
{
	if (!a_ref) return false;

	const auto& settings = Settings::GetSingleton();

	if (settings.enableSideHustle) {
		if (!a_ref->IsCrimeToActivate()) return false;

		const auto player     = RE::PlayerCharacter::GetSingleton();
		const auto playerBase = player ? player->GetActorBase() : nullptr;

		auto isNonPlayerOwner = [&](RE::TESForm* owner) -> bool {
			if (!owner) return false;
			if (const auto npc = owner->As<RE::TESNPC>()) {
				return playerBase ? npc != playerBase : true;
			}
			if (owner->As<RE::TESFaction>()) return true;
			return false;
		};

		if (isNonPlayerOwner(a_ref->extraList.GetOwner())) return true;
		if (isNonPlayerOwner(a_ref->GetActorOwner())) return true;
		if (const auto* cell = a_ref->GetParentCell()) {
			if (isNonPlayerOwner(const_cast<RE::TESObjectCELL*>(cell)->GetOwner())) return true;
		}

		return false;
	}

	{
		std::lock_guard lock(cacheMutex);
		if (cacheDirty) return false;
	}

	if (settings.enableDistanceToMerchant && settings.maxDistanceToMerchant > 0.0f) {
		const auto merchant = FindCellMerchant(a_ref);
		const auto merchantRef = merchant.get();
		if (merchantRef && merchantRef->Is3DLoaded()) {
			const float dist = a_ref->GetPosition().GetDistance(merchantRef->GetPosition());
			if (dist > settings.maxDistanceToMerchant) return false;
		}
	}

	if (const auto owner = a_ref->extraList.GetOwner()) {
		if (const auto npc = owner->As<RE::TESNPC>()) {
			const auto id = npc->GetFormID();
			if (blacklistedNPCCache.contains(id)) return false;
			return IsMerchantNPC(npc);
		}
		if (const auto faction = owner->As<RE::TESFaction>()) {
			const auto id = faction->GetFormID();
			if (blacklistedFactionCache.contains(id)) return false;
			return shopFactionCache.contains(id);
		}
	}

	if (const auto actorOwner = a_ref->GetActorOwner()) {
		if (const auto npc = actorOwner->As<RE::TESNPC>()) {
			const auto id = npc->GetFormID();
			if (blacklistedNPCCache.contains(id)) return false;
			return IsMerchantNPC(npc);
		}
	}

	auto* cell = a_ref->GetParentCell();
	if (cell && cell->IsAttached()) {
		if (const auto* cellOwner = cell->GetOwner()) {
			if (const auto faction = cellOwner->As<RE::TESFaction>()) {
				const auto id = faction->GetFormID();
				if (blacklistedFactionCache.contains(id)) return false;
				return shopFactionCache.contains(id);
			}
		}
	}

	return false;
}

RE::ActorHandle Manager::FindCellMerchant(RE::TESObjectREFR* a_ref) const
{
	if (!a_ref) return {};

	auto* cell = a_ref->GetParentCell();
	if (!cell || !cell->IsAttached()) return {};

	const auto* cellOwner = cell->GetOwner();
	if (!cellOwner) return {};

	const auto* ownerFaction = cellOwner->As<RE::TESFaction>();
	if (!ownerFaction) return {};

	for (const auto& refHandle : cell->GetRuntimeData().references) {
		const auto ref = refHandle.get();
		if (!ref || !ref->Is3DLoaded()) continue;
		auto* actor = ref->As<RE::Actor>();
		if (!actor || actor->IsDead()) continue;
		auto* npc = actor->GetActorBase();
		if (!npc || !IsMerchantNPC(npc)) continue;
		for (const auto& factionRank : npc->factions) {
			if (factionRank.faction == ownerFaction) return actor->GetHandle();
		}
	}

	return {};
}

std::uint32_t Manager::GetBuyCost(RE::TESObjectREFR* a_ref) const
{
	if (!a_ref) return 0;
	const auto base = a_ref->GetBaseObject();
	if (!base) return 0;
	const auto bounded = base->As<RE::TESBoundObject>();
	if (!bounded) return 0;

	const std::uint32_t baseVal = bounded->GetGoldValue() > 0 ? bounded->GetGoldValue() : 1;

	auto* gameSettings = RE::GameSettingCollection::GetSingleton();
	auto* barterMaxSetting = gameSettings->GetSetting("fBarterMax");
	auto* barterMinSetting = gameSettings->GetSetting("fBarterMin");
	const float barterMax = barterMaxSetting ? barterMaxSetting->GetFloat() : 3.3f;
	const float barterMin = barterMinSetting ? barterMinSetting->GetFloat() : 2.0f;

	const auto  player = RE::PlayerCharacter::GetSingleton();
	const float speech = (player && player->AsActorValueOwner())
	                       ? player->AsActorValueOwner()->GetActorValue(RE::ActorValue::kSpeech)
	                       : 0.0f;

	const float skillCapped     = speech < 100.0f ? speech : 100.0f;
	const float basePriceFactor = barterMax - (barterMax - barterMin) * (skillCapped / 100.0f);

	float entryPointModifier = 1.0f;
	if (player) {
		struct ModVisitor : RE::PerkEntryVisitor {
		float* mod;
			int count;
			float minMult = 2.0f;
			const RE::FormID allureID = 0x00058F64;
			const RE::FormID loversInsightID = 0x00058F65;
			float allureMult = 1.0f;
			ModVisitor(float* m) : mod(m), count(0), minMult(2.0f), allureMult(1.0f) {}
			RE::BSContainer::ForEachResult Visit(RE::BGSPerkEntry* e) override {
				count++;
				if (e->GetType() != RE::PERK_ENTRY_TYPE::kEntryPoint) return RE::BSContainer::ForEachResult::kContinue;
				auto* ep = static_cast<RE::BGSEntryPointPerkEntry*>(e);
				if (ep->entryData.entryPoint != RE::BGSEntryPoint::ENTRY_POINTS::kModBuyPrices) return RE::BSContainer::ForEachResult::kContinue;
				if (!ep->functionData || !ep->perk) return RE::BSContainer::ForEachResult::kContinue;
				auto* fdata = static_cast<RE::BGSEntryPointFunctionDataOneValue*>(ep->functionData);
				const auto funcType = static_cast<std::uint8_t>(*ep->entryData.function);
				if (funcType != 3) return RE::BSContainer::ForEachResult::kContinue;
				const auto pid = ep->perk->GetFormID();
				if (pid == allureID || pid == loversInsightID) {
					if (fdata->data < allureMult) allureMult = fdata->data;
				} else {
					if (fdata->data < minMult) minMult = fdata->data;
				}
				return RE::BSContainer::ForEachResult::kContinue;
			}
		};
		ModVisitor v{&entryPointModifier};
		player->ForEachPerkEntry(RE::BGSEntryPoint::ENTRY_POINTS::kModBuyPrices, v);
		if (v.minMult < 1.0f) entryPointModifier = v.minMult;
		if (v.allureMult < 1.0f) entryPointModifier *= v.allureMult;
		static bool s_logged = false;
		if (!s_logged && entryPointModifier != 1.0f) {
			logger::info("Buy price perk modifier: {:.4f}", entryPointModifier);
			s_logged = true;
		}

		const float fbMod = player->AsActorValueOwner()->GetActorValue(RE::ActorValue::kSpeechcraftModifier);
		if (fbMod > 0.0f) {
			entryPointModifier *= 1.0f - (std::min)(fbMod, 100.0f) / 100.0f;
		}
	}

	const auto          rawCount = a_ref->extraList.GetCount();
	const std::uint32_t count    = static_cast<std::uint32_t>(rawCount > 1 ? rawCount : 1);

	const float finalPrice = static_cast<float>(baseVal) * basePriceFactor * entryPointModifier * Settings::GetSingleton().priceMultiplier;
	return static_cast<std::uint32_t>((std::max)(std::round(finalPrice), 1.0f)) * count;
}

bool Manager::GetHotkeyPressed() const { return keyPressed; }
void Manager::SetHotkeyPressed(bool a_pressed) { keyPressed = a_pressed; }
bool Manager::GetHotkeyHeld() const { return keyHeld; }
void Manager::SetHotkeyHeld(bool a_held) { keyHeld = a_held; }

void Manager::UpdateCrosshairs() {
    if (const auto crossHairPickData = RE::CrosshairPickData::GetSingleton()) {
#if defined(EXCLUSIVE_SKYRIM_FLAT)
        const bool hasTarget = crossHairPickData->target.get() != nullptr;
#else
        const bool hasTarget = [&]() {
            for (std::uint32_t i = 0; i < RE::VR_DEVICE::kTotal; ++i) {
                if (crossHairPickData->target[i].get()) return true;
            }
            return false;
        }();
#endif
        if (hasTarget) {
            RE::PlayerCharacter::GetSingleton()->UpdateCrosshairs();
        }
    }
}

RE::BSEventNotifyControl Manager::ProcessEvent(const RE::MenuOpenCloseEvent* a_event, RE::BSTEventSource<RE::MenuOpenCloseEvent>*)
{
	if (!a_event || a_event->opening) return RE::BSEventNotifyControl::kContinue;

	logger::debug("MenuCloseEvent: menuName={}", a_event->menuName.c_str());

	const auto& settings = Settings::GetSingleton();

	if (!settings.enableToggleMode) {
		SetHotkeyPressed(false);
		SetHotkeyHeld(false);
		UpdateCrosshairs();
	}

	return RE::BSEventNotifyControl::kContinue;
}

RE::BSEventNotifyControl Manager::ProcessEvent(RE::InputEvent* const* a_evn, RE::BSTEventSource<RE::InputEvent*>*)
{
	if (!a_evn) return RE::BSEventNotifyControl::kContinue;

	const auto player = RE::PlayerCharacter::GetSingleton();
	if (!player || !player->Is3DLoaded()) return RE::BSEventNotifyControl::kContinue;

	if (const auto skyrimUI = RE::UI::GetSingleton();
		!skyrimUI || skyrimUI->IsMenuOpen(RE::Console::MENU_NAME) || skyrimUI->GameIsPaused()) {
		return RE::BSEventNotifyControl::kContinue;
	}

	const auto& settings = Settings::GetSingleton();

	if (settings.crouchForVanillaAction) {
		bool isSneaking = player->IsSneaking();
		if (isSneaking != keyHeld.load()) {
			SetHotkeyPressed(isSneaking);
			SetHotkeyHeld(isSneaking);
			UpdateCrosshairs();
		}
	}

	for (auto event = *a_evn; event; event = event->next) {
		if (const auto buttonEvent = event->AsButtonEvent()) {
			const auto device = event->GetDevice();
			const auto key    = [&]() -> std::uint32_t {
				auto k = buttonEvent->GetIDCode();
				switch (device) {
				case RE::INPUT_DEVICE::kMouse: return k + 256;
				case RE::INPUT_DEVICE::kGamepad: return SKSE::InputMap::GamepadMaskToKeycode(k);
				default: return k;
				}
			}();

			const bool matchesKey = (settings.keyboardHotkey != 0 && key == settings.keyboardHotkey) ||
			                        (device == RE::INPUT_DEVICE::kGamepad && settings.gamepadHotkey != 0 && key == settings.gamepadHotkey);

			if (matchesKey) {
				if (settings.enableToggleMode) {
					if (buttonEvent->IsPressed() && !buttonEvent->IsRepeating()) {
						toggleActive = !toggleActive;
						SetHotkeyPressed(toggleActive);
						SetHotkeyHeld(toggleActive);
						UpdateCrosshairs();
					}
				} else {
					if (GetHotkeyPressed() != buttonEvent->IsPressed()) {
						SetHotkeyPressed(buttonEvent->IsPressed());
						if (!buttonEvent->IsPressed()) SetHotkeyHeld(false);
						UpdateCrosshairs();
					} else if (GetHotkeyPressed() && !GetHotkeyHeld() &&
					           buttonEvent->HeldDuration() > 0.7f) {
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
