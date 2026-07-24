#pragma once
// Core containers + FName resolution for Squad v10.5.1 (UE 5.7.4, LWC/double).
// This build has NO FNamePool (GNames=0). Names resolve via FName::AppendString.
#include <cstdint>
#include <string>

// Global engine addresses (set in init) - RVA from game base [Dumper-7 Basic.hpp]
extern uint64_t g_game_base;
extern uint64_t g_append_string;   // base + 0x013ED940
extern uint64_t g_gobjects;        // base + 0x0D0331B0
extern uint64_t g_gworld;          // base + 0x0D1C9EB8

template <class T>
class TArray
{
public:
	T* data;
	int32_t count;
	int32_t capacity;

	TArray() : data(nullptr), count(0), capacity(0) {}
	bool is_valid() const { return data != nullptr && count > 0; }
	T& operator[](int32_t i) const { return data[i]; }
};
static_assert(sizeof(TArray<void*>) == 0x10, "TArray size");

class FString : public TArray<wchar_t>
{
public:
	FString() = default;
	FString(const wchar_t* other) {
		capacity = count = *other ? (int32_t)std::wcslen(other) + 1 : 0;
		if (capacity) data = const_cast<wchar_t*>(other);
	}
	bool is_valid() const { return data != nullptr && count > 0; }
	const wchar_t* c_str() const { return data; }
	std::string to_string() const {
		if (!data || count <= 0) return std::string();
		std::string str;
		str.reserve(count);
		for (int32_t i = 0; i < count - 1; i++) {
			wchar_t c = data[i];
			str += (c < 128) ? (char)c : '?';
		}
		return str;
	}
};
static_assert(sizeof(FString) == 0x10, "FString size");

// FName: ComparisonIndex(0x0) + Number(0x4). No DisplayIndex in this build.
struct FName
{
	int32_t index;
	int32_t number;

	// Resolve via engine FName::AppendString(const FName*, FString&)
	std::string to_string() const;
};

// Pointer sanity for user-mode game addresses
inline bool ptr_sane(const void* p) {
	uint64_t v = (uint64_t)p;
	return v > 0x10000 && v < 0x00007FFFFFFFFFFFULL;
}
