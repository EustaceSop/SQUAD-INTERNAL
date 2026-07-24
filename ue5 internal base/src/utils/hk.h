#pragma once
#include "../sdk/sdk_functions.h"
#include "../sdk/sdk.h"
#include <future>

// ProcessEvent vtable index [Dumper-7 Basic.hpp: ProcessEventIdx]
constexpr int process_event_index = 0x4C;
// PostRender vtable index: auto-detected at runtime (see dllmain)
extern int post_render_index;

std::future<void> main_future;
inline TUObjectArray* ObjectArray = nullptr;
inline UObject* engine_font = nullptr;

using pevent_fn = void(__fastcall*)(UObject*, UObject*, void*);
inline pevent_fn process_event_original = nullptr;

using postrender_fn = void(__fastcall*)(UGameViewportClient*, UCanvas*);
inline postrender_fn post_render_original = nullptr;

void post_render_hook(UGameViewportClient* viewport_client, UCanvas* canvas);

void __fastcall process_event_hook(UObject* caller, UObject* fn, void* parms) {
    process_event_original(caller, fn, parms);
}
