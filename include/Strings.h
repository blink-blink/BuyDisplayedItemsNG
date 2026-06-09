#pragma once

#include <string>
#include <string_view>
#include <unordered_map>

class Strings
{
public:
	static void Load();
	static const char* Get(std::string_view a_key);
	static void Set(std::string_view a_key, std::string_view a_value);
	static void SyncTranslation();

private:
	static std::unordered_map<std::string, std::string> _map;
};
