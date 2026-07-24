#include "fnamepool.h"

uint64_t g_game_base = 0;
uint64_t g_append_string = 0;
uint64_t g_gobjects = 0;
uint64_t g_gworld = 0;

std::string FName::to_string() const
{
	if (!g_append_string || index <= 0) return std::string();
	wchar_t buf[1024];
	FString out;
	out.data = buf;
	out.count = 0;
	out.capacity = 1024;
	try {
		reinterpret_cast<void(*)(const FName*, FString*)>(g_append_string)(this, &out);
	}
	catch (...) { return std::string(); }
	if (out.count <= 0 || out.count > 1023) return std::string();
	buf[out.count < 1024 ? out.count : 1023] = 0;
	return out.to_string();
}
