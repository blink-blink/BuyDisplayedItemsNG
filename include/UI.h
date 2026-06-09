#pragma once

#include "SKSEMenuFramework.h"

namespace ImGui = ImGuiMCP;

class UI
{
public:
	static void Register();
	static void HelpMarker(const char* a_desc);

private:
	static const char* GetKeyName(std::uint32_t a_key);

	static void __stdcall RenderMenu();

	inline static char _npcFilterBuf[256]{};
	inline static char _buyLabelBuf[256]{};
	inline static char _insuffGoldBuf[256]{};
	inline static char _blacklistFilterBuf[256]{};
};
