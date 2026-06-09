#include "pch.h"
#include "NPCRegistry.h"

static constexpr RE::FormID kJobInnkeeperFactionID{ 0x0005091B };
static constexpr RE::FormID kJobInnServerFactionID{ 0x000DEE93 };
static constexpr RE::FormID kJobMerchantFactionID{ 0x00051596 };

NPCRegistry& NPCRegistry::GetSingleton()
{
	static NPCRegistry registry;
	return registry;
}

std::string NPCRegistry::ToLower(std::string_view a_value)
{
	std::string result(a_value);
	std::ranges::transform(result, result.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
	return result;
}

bool NPCRegistry::IsMerchantNPC(RE::TESNPC* a_npc)
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

void NPCRegistry::BuildCache()
{
	if (_built) return;

	_entries.clear();

	const auto& [editorIDMap, mapLock] = RE::TESForm::GetAllFormsByEditorID();
	if (editorIDMap) {
		for (const auto& [editorID, form] : *editorIDMap) {
			if (!form || editorID.empty()) continue;

			auto* npc = form->As<RE::TESNPC>();
			if (!npc) continue;
			if (!IsMerchantNPC(npc)) continue;

			const char* name = npc->GetName();
			NPCEntry entry;
			entry.editorID = editorID.c_str();
			entry.editorIDLower = ToLower(entry.editorID);
			entry.displayName = (name && name[0] != '\0') ? name : entry.editorID;
			entry.displayNameLower = ToLower(entry.displayName);
			entry.formID = npc->GetFormID();
			entry.comboLabel = std::format("{} [{}] ({:08X})", entry.displayName, entry.editorID, entry.formID);
			_entries.push_back(std::move(entry));
		}
	}

	std::ranges::sort(_entries, [](const NPCEntry& a_lhs, const NPCEntry& a_rhs) {
		if (a_lhs.displayNameLower != a_rhs.displayNameLower) return a_lhs.displayNameLower < a_rhs.displayNameLower;
		return a_lhs.editorIDLower < a_rhs.editorIDLower;
	});

	_built = true;
	logger::info("NPCRegistry: cached {} merchant NPC entries", _entries.size());
}

const std::vector<NPCEntry>& NPCRegistry::GetEntries() const
{
	return _entries;
}

std::vector<std::size_t> NPCRegistry::Filter(std::string_view a_query) const
{
	std::vector<std::size_t> matches;
	const std::string needle = ToLower(a_query);
	matches.reserve(_entries.size());

	for (std::size_t i = 0; i < _entries.size(); ++i) {
		const auto& entry = _entries[i];
		if (needle.empty() ||
			entry.editorIDLower.find(needle) != std::string::npos ||
			entry.displayNameLower.find(needle) != std::string::npos) {
			matches.push_back(i);
		}
	}

	return matches;
}

const NPCEntry* NPCRegistry::FindByEditorID(std::string_view a_editorID) const
{
	for (const auto& entry : _entries) {
		if (entry.editorID == a_editorID) return std::addressof(entry);
	}
	return nullptr;
}
