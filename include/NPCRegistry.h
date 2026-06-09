#pragma once

#include <string>
#include <vector>

struct NPCEntry
{
	std::string editorID;
	std::string editorIDLower;
	std::string displayName;
	std::string displayNameLower;
	std::string comboLabel;
	RE::FormID  formID{ 0 };
};

class NPCRegistry
{
public:
	static NPCRegistry& GetSingleton();

	void BuildCache();
	[[nodiscard]] const std::vector<NPCEntry>& GetEntries() const;
	[[nodiscard]] std::vector<std::size_t> Filter(std::string_view a_query) const;
	[[nodiscard]] const NPCEntry* FindByEditorID(std::string_view a_editorID) const;

private:
	NPCRegistry() = default;

	[[nodiscard]] static bool IsMerchantNPC(RE::TESNPC* a_npc);

	static std::string ToLower(std::string_view a_value);

	std::vector<NPCEntry> _entries;
	bool                  _built{ false };
};
