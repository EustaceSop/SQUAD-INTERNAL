#include "engine.h"
#include "sdk.h"

void UObject::ProcessEvent(void* fn, void* parms)
{
	auto vt = *reinterpret_cast<void***>(this);
	reinterpret_cast<void(*)(void*, void*, void*)>(vt[rva::ProcessEventIdx])(this, fn, parms);
}

std::string UObject::GetName()
{
	try {
		return NamePrivate.to_string();
	} catch (...) { return std::string(); }
}

std::string UObject::GetFullName()
{
	try {
		std::string name;
		for (auto outer = OuterPrivate; outer && ptr_sane(outer); outer = outer->OuterPrivate) {
			std::string on = outer->GetName();
			if (on.empty()) break;
			name = on + "." + name;
		}
		if (!ptr_sane(ClassPrivate)) return std::string();
		name = ClassPrivate->GetName() + " " + name + this->GetName();
		return name;
	} catch (...) { return std::string(); }
}

UObject* TUObjectArray::GetObjectPtr(uint32_t id) const
{
	if ((int32_t)id >= NumElements) return nullptr;
	uint64_t chunkIndex = id / 0x10000;
	if (chunkIndex >= (uint32_t)NumChunks) return nullptr;
	auto chunk = Objects[chunkIndex];
	if (!chunk) return nullptr;
	uint64_t item = (uint64_t)chunk + (uint64_t)(id % 0x10000) * 0x18;
	return *reinterpret_cast<UObject**>(item + 0x8);   // FUObjectItem::Object @ +0x8
}

UObject* TUObjectArray::FindObject(const char* name) const
{
	for (uint32_t i = 0; i < (uint32_t)NumElements; i++) {
		auto object = GetObjectPtr(i);
		if (object && ptr_sane(object) && object->GetFullName() == name) return object;
	}
	return nullptr;
}

UObject* TUObjectArray::FindObjectByString(const char* name)
{
	for (uint32_t i = 0; i < (uint32_t)NumElements; i++) {
		auto object = GetObjectPtr(i);
		if (object && ptr_sane(object)) {
			std::string fn = object->GetFullName();
			if (fn.find(name) != std::string::npos) return object;
		}
	}
	return nullptr;
}

std::vector<UObject*> TUObjectArray::FindObjectsByString(const char* name)
{
	std::vector<UObject*> res;
	for (uint32_t i = 0; i < (uint32_t)NumElements; i++) {
		auto object = GetObjectPtr(i);
		if (object && ptr_sane(object)) {
			std::string fn = object->GetFullName();
			if (fn.find(name) != std::string::npos) res.push_back(object);
		}
	}
	return res;
}

UObject* WeakObjResolve(const FWeakObjectPtr& weak)
{
	extern TUObjectArray* ObjectArray;
	if (!ObjectArray || weak.ObjectIndex < 0) return nullptr;
	return ObjectArray->GetObjectPtr((uint32_t)weak.ObjectIndex);
}
