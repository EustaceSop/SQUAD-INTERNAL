#include <Windows.h>
#include <vector>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <cstdio>
#include <cstdarg>
#include <chrono>
#include <thread>
#include <future>
#include <atomic>
#include "utils/draw.h"
#include "minhook/minhook.h"
#include "utils/hk.h"
#include "utils/menu.h"
#include "utils/entities.h"

int post_render_index = -1;

static void save_config();
static void load_config();
static void chams_restore_all();
static void chams_state_write(const char* line);

static CameraView g_cam{};
static bool g_first_frame_logged = false;

// ==================== unhook state ====================
static HMODULE g_hmodule = nullptr;
static std::atomic<bool> g_unhooking{ false };     // hook body short-circuits when true
static std::atomic<bool> g_init_active{ false };   // init thread still running
static std::atomic<bool> g_pr_hooked{ false };     // PostRender MinHook currently installed
static void* g_pr_target = nullptr;                // hooked PostRender address
// chams restore must run on the GAME thread (inside PostRender), never on the
// watcher thread: SetMaterial touches component end-of-frame update state and
// racing it with LevelTick asserts (LevelTick.cpp:1078 crash)
static std::atomic<bool> g_chams_restore_req{ false };
static std::atomic<bool> g_chams_restore_done{ false };

// ==================== logging ====================
static FILE* g_log = nullptr;
void ilog(const char* fmt, ...) {
    char buf[1024];
    va_list ap; va_start(ap, fmt);
    vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    printf("%s", buf);
    if (g_log) { fputs(buf, g_log); fflush(g_log); }
}

// ==================== real unhook + eject ====================
static void unhook_and_eject() {
    // 1. stop cheat logic in the render hook immediately
    g_unhooking = true;

    // mark clean exit so the last-tested material is NOT blacklisted
    chams_state_write("CLEAN");

    // request material restore ON THE GAME THREAD (PostRender hook performs it)
    g_chams_restore_done = false;
    g_chams_restore_req = true;
    for (int i = 0; i < 30 && !g_chams_restore_done; i++) Sleep(100);   // max 3s
    if (!g_chams_restore_done)
        ilog("[unhook] WARN: no frame ran the chams restore (leftovers possible)\n");

    // 2. wait for init thread to finish (it aborts early when unhooking)
    for (int i = 0; i < 100 && g_init_active; i++) Sleep(100);   // max 10s

    // 3. let in-flight PostRender calls leave our code
    Sleep(300);

    // 4. remove hooks, restore original bytes (log status + verify bytes restored)
    if (g_pr_hooked && g_pr_target) {
        unsigned char before[8] = { 0 };
        memcpy(before, g_pr_target, 8);
        MH_STATUS s1 = MH_DisableHook(g_pr_target);
        MH_STATUS s2 = MH_RemoveHook(g_pr_target);
        unsigned char after[8] = { 0 };
        memcpy(after, g_pr_target, 8);
        ilog("[unhook] disable=%d remove=%d bytes %02X %02X %02X -> %02X %02X %02X\n",
             (int)s1, (int)s2, before[0], before[1], before[2], after[0], after[1], after[2]);
        g_pr_hooked = false;
    }

    // 5. settle, then tear down MinHook (frees trampolines)
    Sleep(200);
    MH_Uninitialize();

    save_config();
    ilog("[unhook] hooks removed, config saved, ejecting\n");
    if (g_log) { fclose(g_log); g_log = nullptr; }

    // 6. close console window explicitly (freopen'd stdio handles keep it alive)
    HWND con = GetConsoleWindow();
    if (con) {
        ShowWindow(con, SW_HIDE);
        PostMessage(con, WM_CLOSE, 0, 0);
    }
    FreeConsole();

    // 7. unload this DLL from the game process (this thread does not return)
    FreeLibraryAndExitThread(g_hmodule, 0);
}

static DWORD WINAPI unhook_watcher(LPVOID) {
    while (true) {
        if (GetAsyncKeyState(VK_END) & 0x8000) {
            unhook_and_eject();
            return 0;   // unreachable
        }
        Sleep(50);
    }
}

// ==================== config ====================
static std::string config_path() {
    char tmp[MAX_PATH];
    if (GetTempPathA(MAX_PATH, tmp)) return std::string(tmp) + "squad_int_cfg.txt";
    return "squad_int_cfg.txt";
}

static void save_config() {
    FILE* f = fopen(config_path().c_str(), "w");
    if (!f) return;
    fprintf(f, "aimbot=%d\naim_fov=%f\naim_smooth=%f\naim_visible_only=%d\naim_max_step=%f\n",
            vars::aimbot, vars::aim_fov, vars::aim_smooth, vars::aim_visible_only, vars::aim_max_step);
    fprintf(f, "box=%d\nskeleton=%d\nname=%d\ndistance=%d\nhealth_bar=%d\n",
            vars::box, vars::skeleton, vars::name, vars::distance, vars::health_bar);
    fprintf(f, "weapon_info=%d\nrole_info=%d\nstance_info=%d\nkd_info=%d\ntickets_hud=%d\n",
            vars::weapon_info, vars::role_info, vars::stance_info, vars::kd_info, vars::tickets_hud);
    fprintf(f, "show_bots=%d\nshow_teammates=%d\nshow_downed=%d\nesp_max_dist=%f\n",
            vars::show_bots, vars::show_teammates, vars::show_downed, vars::esp_max_dist);
    fprintf(f, "vehicle_esp=%d\nvehicle_wp=%d\nfob_esp=%d\ndeploy_esp=%d\nmine_warn=%d\ncapzone_hud=%d\nworld_max_dist=%f\nworld_label_dist=%f\n",
            vars::vehicle_esp, vars::vehicle_wp, vars::fob_esp, vars::deploy_esp,
            vars::mine_warn, vars::capzone_hud, vars::world_max_dist, vars::world_label_dist);
    fprintf(f, "chams=%d\nchams_vehicles=%d\nchams_deployables=%d\nchams_mat=%d\nchams_mat_veh=%d\nchams_mat_dep=%d\nchams_list=%d\nradar=%d\nradar_range=%f\ngrenade_warn=%d\naim_bone=%d\naim_predict=%d\n",
            vars::chams, vars::chams_vehicles, vars::chams_deployables, vars::chams_mat,
            vars::chams_mat_veh, vars::chams_mat_dep,
            vars::chams_list, vars::radar, vars::radar_range, vars::grenade_warn,
            vars::aim_bone, vars::aim_predict);
    fclose(f);
}

static void load_config() {
    FILE* f = fopen(config_path().c_str(), "r");
    if (!f) return;
    char line[128];
    auto b = [](const char* v) { return atoi(v) != 0; };
    while (fgets(line, sizeof(line), f)) {
        char* eq = strchr(line, '=');
        if (!eq) continue;
        *eq = 0;
        const char* v = eq + 1;
        if (!strcmp(line, "aimbot")) vars::aimbot = b(v);
        else if (!strcmp(line, "aim_fov")) vars::aim_fov = (float)atof(v);
        else if (!strcmp(line, "aim_smooth")) vars::aim_smooth = (float)atof(v);
        else if (!strcmp(line, "aim_visible_only")) vars::aim_visible_only = b(v);
        else if (!strcmp(line, "aim_max_step")) vars::aim_max_step = (float)atof(v);
        else if (!strcmp(line, "box")) vars::box = b(v);
        else if (!strcmp(line, "skeleton")) vars::skeleton = b(v);
        else if (!strcmp(line, "name")) vars::name = b(v);
        else if (!strcmp(line, "distance")) vars::distance = b(v);
        else if (!strcmp(line, "health_bar")) vars::health_bar = b(v);
        else if (!strcmp(line, "weapon_info")) vars::weapon_info = b(v);
        else if (!strcmp(line, "role_info")) vars::role_info = b(v);
        else if (!strcmp(line, "stance_info")) vars::stance_info = b(v);
        else if (!strcmp(line, "kd_info")) vars::kd_info = b(v);
        else if (!strcmp(line, "tickets_hud")) vars::tickets_hud = b(v);
        else if (!strcmp(line, "show_bots")) vars::show_bots = b(v);
        else if (!strcmp(line, "show_teammates")) vars::show_teammates = b(v);
        else if (!strcmp(line, "show_downed")) vars::show_downed = b(v);
        else if (!strcmp(line, "esp_max_dist")) vars::esp_max_dist = (float)atof(v);
        else if (!strcmp(line, "vehicle_esp")) vars::vehicle_esp = b(v);
        else if (!strcmp(line, "vehicle_wp")) vars::vehicle_wp = b(v);
        else if (!strcmp(line, "fob_esp")) vars::fob_esp = b(v);
        else if (!strcmp(line, "deploy_esp")) vars::deploy_esp = b(v);
        else if (!strcmp(line, "mine_warn")) vars::mine_warn = b(v);
        else if (!strcmp(line, "capzone_hud")) vars::capzone_hud = b(v);
        else if (!strcmp(line, "world_max_dist")) vars::world_max_dist = (float)atof(v);
        else if (!strcmp(line, "world_label_dist")) vars::world_label_dist = (float)atof(v);
        else if (!strcmp(line, "chams")) vars::chams = b(v);
        else if (!strcmp(line, "chams_vehicles")) vars::chams_vehicles = b(v);
        else if (!strcmp(line, "chams_deployables")) vars::chams_deployables = b(v);
        else if (!strcmp(line, "chams_mat")) vars::chams_mat = atoi(v);
        else if (!strcmp(line, "chams_mat_veh")) vars::chams_mat_veh = atoi(v);
        else if (!strcmp(line, "chams_mat_dep")) vars::chams_mat_dep = atoi(v);
        else if (!strcmp(line, "chams_list")) vars::chams_list = atoi(v) & 1;
        else if (!strcmp(line, "radar")) vars::radar = b(v);
        else if (!strcmp(line, "radar_range")) vars::radar_range = (float)atof(v);
        else if (!strcmp(line, "grenade_warn")) vars::grenade_warn = b(v);
        else if (!strcmp(line, "aim_bone")) vars::aim_bone = atoi(v) % 3;
        else if (!strcmp(line, "aim_predict")) vars::aim_predict = b(v);
    }
    fclose(f);
}

// ==================== PostRender auto-detect ====================
using det_fn = void(__fastcall*)(void*, void*, void*, void*);
static volatile long g_det_hits = 0;
static void* g_det_arg2 = nullptr;
static det_fn g_det_orig = nullptr;

static void __fastcall postrender_detector(void* a1, void* a2, void* a3, void* a4) {
    InterlockedIncrement(&g_det_hits);
    if (!g_det_arg2) g_det_arg2 = a2;
    g_det_orig(a1, a2, a3, a4);
}

static bool validate_canvas(void* p) {
    if (!ptr_sane(p)) return false;
    try {
        UObject* obj = (UObject*)p;
        if (!ptr_sane(obj->vtable)) return false;
        if (!ptr_sane(obj->ClassPrivate)) return false;
        return obj->ClassPrivate->GetName() == "Canvas";
    } catch (...) { return false; }
}

static int detect_post_render(UGameViewportClient* viewport) {
    void** vt = *(void***)viewport;
    ilog("[pr] scanning viewport vtable %p ...\n", vt);
    for (int round = 0; round < 30; round++) {
        if (g_unhooking) return -1;
        void* tested[64]; int ntested = 0;
        for (int i = 0x58; i <= 0x78; i++) {
            if (g_unhooking) return -1;
            void* target = vt[i];
            if (!ptr_sane(target)) continue;
            unsigned char first = 0;
            try { first = *(unsigned char*)target; } catch (...) { continue; }
            if (first == 0xCC || first == 0x00) continue;
            bool dup = false;
            for (int k = 0; k < ntested; k++) if (tested[k] == target) { dup = true; break; }
            if (dup) continue;
            tested[ntested++] = target;

            g_det_hits = 0; g_det_arg2 = nullptr; g_det_orig = nullptr;
            if (MH_CreateHook(target, &postrender_detector, reinterpret_cast<void**>(&g_det_orig)) != MH_OK) {
                ilog("[pr] slot 0x%02X %p: MH_CreateHook failed\n", i, target);
                continue;
            }
            if (MH_EnableHook(target) != MH_OK) {
                ilog("[pr] slot 0x%02X %p: MH_EnableHook failed\n", i, target);
                MH_RemoveHook(target);
                continue;
            }
            Sleep(300);
            long hits = g_det_hits;
            void* arg2 = g_det_arg2;
            MH_DisableHook(target);
            MH_RemoveHook(target);

            if (hits > 0 || round == 0) ilog("[pr] slot 0x%02X %p: hits=%d arg2=%p", i, target, (int)hits, arg2);
            if (hits >= 3 && validate_canvas(arg2)) {
                ilog("  <== PostRender (Canvas arg confirmed)\n");
                return i;
            }
            if (hits > 0 || round == 0) ilog("\n");
        }
        ilog("[pr] round %d: no PostRender activity - keep the game focused/rendering, retrying...\n", round);
        Sleep(1000);
    }
    return -1;
}

// ==================== bot structural probe ====================
// Bots (ASQBot) have no PlayerState; find them in level actors by structure:
// actor+0x338 (ACharacter::Mesh) -> mesh+0x620 bone array, count in [100,300].
struct BotEntry {
    uint64_t mesh;
    std::chrono::steady_clock::time_point last_ok;
    // last good bone snapshot (anti-flicker grace when a frame's read fails)
    std::vector<FVector> bones;
    int bone_count = 0;
    std::chrono::steady_clock::time_point bones_time{};
};
static std::unordered_map<uint64_t, BotEntry> g_bots;          // actor -> entry
static std::unordered_map<uint64_t, std::chrono::steady_clock::time_point> g_probe_fail;

static bool probe_soldier_mesh(uint64_t actor, uint64_t& out_mesh) {
    try {
        uint64_t mesh = fld<uint64_t>(actor, off::CharacterMesh);
        if (!ptr_sane((void*)mesh)) return false;
        FTransform* data = fld<FTransform*>(mesh, off::BoneArray);
        int32_t count = fld<int32_t>(mesh, off::BoneArray + 0x8);
        if (!ptr_sane(data) || count < 100 || count > 300) return false;
        // sanity: first transform quat ~unit, scale ~1
        const FTransform& t0 = data[0];
        double qn = t0.rotation.x * t0.rotation.x + t0.rotation.y * t0.rotation.y +
                    t0.rotation.z * t0.rotation.z + t0.rotation.w * t0.rotation.w;
        if (qn < 0.5 || qn > 2.0) return false;
        if (t0.scale3d.x < 0.01 || t0.scale3d.x > 100.0) return false;
        out_mesh = mesh;
        return true;
    } catch (...) { return false; }
}

static void scan_bots(UWorld* world, APawn* local_pawn) {
    auto levels = world->Levels();
    if (!ptr_sane(levels.data)) return;
    auto now = std::chrono::steady_clock::now();
    int budget = 96;
    for (int li = 0; li < levels.count && budget > 0; li++) {
        ULevel* level = levels[li];
        if (!ptr_sane(level)) continue;
        auto actors = level->Actors();
        if (!ptr_sane(actors.data) || actors.count <= 0 || actors.count > 100000) continue;
        for (int ai = 0; ai < actors.count && budget > 0; ai++) {
            uint64_t actor = (uint64_t)actors[ai];
            if (!ptr_sane((void*)actor) || actor == (uint64_t)local_pawn) continue;
            if (g_bots.count(actor)) continue;
            auto fit = g_probe_fail.find(actor);
            if (fit != g_probe_fail.end() &&
                std::chrono::duration_cast<std::chrono::seconds>(now - fit->second).count() < 5)
                continue;
            budget--;
            uint64_t mesh = 0;
            if (!probe_soldier_mesh(actor, mesh)) { g_probe_fail[actor] = now; continue; }
            // exclude vehicles/deployables misclassified by the bone probe
            EntCat ac = classify_actor(actor);
            if (ac == EntCat::Vehicle || ac == EntCat::Deployable || ac == EntCat::FOB) {
                g_probe_fail[actor] = now;
                continue;
            }
            g_bots[actor] = { mesh, now };
        }
    }
    // expire bots not revalidated for 10s
    for (auto it = g_bots.begin(); it != g_bots.end();) {
        if (std::chrono::duration_cast<std::chrono::seconds>(now - it->second.last_ok).count() > 10)
            it = g_bots.erase(it);
        else ++it;
    }
}

// ==================== ESP ====================
struct EspTarget {
    uint64_t actor = 0;
    uint64_t mesh = 0;
    std::string name;
    bool is_bot = false;
    bool teammate = false;
    float health = -1.f;
    FVector bones[141];
    int bone_count = 0;
    FVector pos;
    // ---- batch A: info ----
    std::string weapon_name;
    int ammo_mag = -1;
    int ammo_reserve = -1;
    std::string role;
    int kills = -1;
    int deaths = -1;
    bool is_admin = false;
    bool is_commander = false;
    bool prone = false;
    bool crouched = false;
    bool sprinting = false;
    bool ads = false;
};

// weapon display-name cache (weapon actor ptr -> name; names don't change)
static std::unordered_map<uint64_t, std::string> g_weapon_name_cache;

static void fill_weapon_info(EspTarget& t) {
    try {
        ASQWeapon* w = ((ASQSoldier*)t.actor)->HeldWeapon();
        if (!ptr_sane(w)) return;
        uint64_t wa = (uint64_t)w;
        auto it = g_weapon_name_cache.find(wa);
        if (it != g_weapon_name_cache.end()) {
            t.weapon_name = it->second;
        } else {
            std::string wn = w->DisplayName();
            if (wn.size() > 40) wn = wn.substr(0, 40);
            g_weapon_name_cache[wa] = wn;
            t.weapon_name = wn;
        }
        int mag = 0, res = 0;
        w->Ammo(mag, res);
        t.ammo_mag = mag;
        t.ammo_reserve = res;
    } catch (...) {}
}

static bool read_bones(uint64_t mesh, FVector* out, int& out_count, FVector& head_out) {
    try {
        FTransform* data; int32_t count;
        FTransform ctw = fld<FTransform>(mesh, off::ComponentToWorld);
        data = fld<FTransform*>(mesh, off::BoneArray);
        count = fld<int32_t>(mesh, off::BoneArray + 0x8);
        if (!ptr_sane(data) || count < 100 || count > 300) return false;
        int n = count > 141 ? 141 : count;
        for (int i = 0; i < n; i++)
            out[i] = transform_bone_to_world(ctw, data[i]);
        out_count = n;
        head_out = out[off::bones::Head];
        if (head_out.is_zero()) head_out = out[off::bones::HeadNub];
        return true;
    } catch (...) { return false; }
}

static void draw_skeleton(UCanvas* canvas, const CameraView& cam, const EspTarget& t, FLinearColor color) {
    using namespace off::bones;
    auto line = [&](int a, int b) {
        if (a >= t.bone_count || b >= t.bone_count) return;
        FVector2D s1, s2;
        if (!cam.world_to_screen(t.bones[a], s1)) return;
        if (!cam.world_to_screen(t.bones[b], s2)) return;
        RenderLine(canvas, s1, s2, 1.f, color);
    };
    line(Pelvis, Spine); line(Spine, Spine1); line(Spine1, Spine2);
    line(Spine2, Neck); line(Neck, Head);
    line(Spine2, R_Clavicle); line(R_Clavicle, R_UpperArm);
    line(R_UpperArm, R_Forearm); line(R_Forearm, R_Hand);
    line(Spine2, L_Clavicle); line(L_Clavicle, L_UpperArm);
    line(L_UpperArm, L_Forearm); line(L_Forearm, L_Hand);
    line(Pelvis, R_Thigh); line(R_Thigh, R_Calf); line(R_Calf, R_Foot);
    line(Pelvis, L_Thigh); line(L_Thigh, L_Calf); line(L_Calf, L_Foot);
}

// bone set used for the projected box minmax (22 bones, matches external build)
static const int kBoxBones[] = {
    off::bones::Pelvis, off::bones::Spine, off::bones::Spine1, off::bones::Spine2,
    off::bones::Neck, off::bones::Head,
    off::bones::R_Clavicle, off::bones::R_UpperArm, off::bones::R_Forearm, off::bones::R_Hand,
    off::bones::L_Clavicle, off::bones::L_UpperArm, off::bones::L_Forearm, off::bones::L_Hand,
    off::bones::L_Thigh, off::bones::L_Calf, off::bones::L_Foot, off::bones::L_Toe0,
    off::bones::R_Thigh, off::bones::R_Calf, off::bones::R_Foot, off::bones::R_Toe0
};

static void draw_target(UCanvas* canvas, const CameraView& cam, const EspTarget& t, APlayerController* ctrl) {
    if (t.bone_count <= 0) return;   // bone-projected box requires bones

    // ---- bone projection minmax ----
    double min_x = 1e18, min_y = 1e18, max_x = -1e18, max_y = -1e18;
    int projected = 0;
    for (int bi : kBoxBones) {
        if (bi >= t.bone_count) continue;
        FVector2D s;
        if (!cam.project_unclamped(t.bones[bi], s)) continue;
        if (s.x < min_x) min_x = s.x;
        if (s.x > max_x) max_x = s.x;
        if (s.y < min_y) min_y = s.y;
        if (s.y > max_y) max_y = s.y;
        projected++;
    }
    if (projected < 4) return;

    double h = max_y - min_y;
    if (h < 6.0) return;

    // padding: sides/bottom small gap, larger gap above the head
    double pad_x   = h * 0.10;
    double pad_top = h * 0.18;   // head room
    double pad_bot = h * 0.06;
    double x1 = min_x - pad_x, x2 = max_x + pad_x;
    double y1 = min_y - pad_top, y2 = max_y + pad_bot;
    double cx = (x1 + x2) * 0.5;

    FLinearColor color = t.teammate ? FLinearColor{ 0.2f, 0.5f, 1.f, 1.f }
                       : t.is_bot   ? FLinearColor{ 1.f, 0.6f, 0.f, 1.f }
                                    : FLinearColor{ 1.f, 0.15f, 0.15f, 1.f };

    if (vars::box)
        RenderBox(canvas, { x1, y1 }, { x2, y2 }, color, 1.f);

    // health bar: 6px left of the box edge. frac is domain-adaptive:
    // health stored 0-100 (players) -> /100; stored 0-1 ratio (some pawns) -> direct
    if (vars::health_bar && t.health >= 0.f) {
        float frac = t.health > 1.0f ? t.health / 100.f : t.health;
        RenderHealthBar(canvas, { x1 - 6.0, y1 }, { x1 - 6.0, y2 }, frac);
    }

    if (vars::skeleton)
        draw_skeleton(canvas, cam, t, color);

    double dist_m = cam.location.distance_m(t.pos);

    // ---- top line: [ADMIN]/[CMD] name (K/D) ----
    if (vars::name && !t.name.empty()) {
        std::string top;
        if (t.is_admin) top += "[ADMIN] ";
        else if (t.is_commander) top += "[CMD] ";
        top += t.name;
        if (vars::kd_info && t.kills >= 0) {
            char kd[32];
            snprintf(kd, sizeof(kd), " (%d/%d)", t.kills, t.deaths);
            top += kd;
        }
        FLinearColor nc = t.is_admin ? FLinearColor{ 1.f, 0.2f, 0.8f, 1.f } : color;
        RenderTextCentered(canvas, { cx, y1 - 16.0 }, top, nc);
    }

    // ---- bottom line 1: [dist] ----
    double by = y2 + 4.0;
    if (vars::distance) {
        char buf[64];
        snprintf(buf, sizeof(buf), "[%.0fm]", dist_m);
        RenderTextCentered(canvas, { cx, by }, buf, { 1.f, 1.f, 1.f, 0.9f });
        by += 14.0;
    }

    // ---- bottom line 2: weapon ammo · role · stance ----
    {
        std::string info;
        if (vars::weapon_info && !t.weapon_name.empty()) {
            info += t.weapon_name;
            if (t.ammo_mag >= 0) {
                char am[32];
                snprintf(am, sizeof(am), " %d/%d", t.ammo_mag, t.ammo_reserve);
                info += am;
            }
        }
        if (vars::role_info && !t.role.empty()) {
            if (!info.empty()) info += " · ";
            info += t.role;
        }
        if (vars::stance_info) {
            std::string st;
            if (t.prone) st += "P";
            if (t.crouched) st += "C";
            if (t.sprinting) st += "S";
            if (t.ads) st += "A";
            if (!st.empty()) {
                if (!info.empty()) info += " · ";
                info += st;
            }
        }
        if (!info.empty())
            RenderTextCentered(canvas, { cx, by }, info, { 0.9f, 0.9f, 0.6f, 0.95f });
    }
}

// ==================== chams (through-wall material swap) ====================
struct ChamMat {
    std::string full;
    std::string short_name;
    UObject* obj;
    bool invisible_hint;
};

struct ChamState {
    std::vector<UObject*> originals;
    bool applied = false;
    std::chrono::steady_clock::time_point seen{};
};
static std::unordered_map<uint64_t, ChamState> g_chams_state;

// aliveness check for components we swapped: vtable must point into the game
// image (destroyed actors linger but stale/reused memory fails this)
static bool uobject_alive(const void* p) {
    if (!ptr_sane(p)) return false;
    try {
        uint64_t vt = *(const uint64_t*)p;
        return vt >= g_game_base && vt < g_game_base + 0x40000000ULL;
    } catch (...) { return false; }
}

static void chams_restore_all() {
    for (auto& kv : g_chams_state) {
        if (!kv.second.applied) continue;
        USkeletalMeshComponent* mesh = (USkeletalMeshComponent*)kv.first;
        if (!uobject_alive(mesh)) { kv.second.applied = false; continue; }
        for (size_t i = 0; i < kv.second.originals.size(); i++) {
            try { mesh->SetMaterial((int)i, kv.second.originals[i]); } catch (...) {}
        }
        kv.second.applied = false;
    }
}

// ==================== chams crash-state + blacklist ====================
static std::string chams_state_path() {
    char tmp[MAX_PATH];
    if (GetTempPathA(MAX_PATH, tmp)) return std::string(tmp) + "squad_int_chams_state.txt";
    return "squad_int_chams_state.txt";
}
static std::string chams_blacklist_path() {
    char tmp[MAX_PATH];
    if (GetTempPathA(MAX_PATH, tmp)) return std::string(tmp) + "squad_int_chams_blacklist.txt";
    return "squad_int_chams_blacklist.txt";
}

static void chams_state_write(const char* line) {
    FILE* f = fopen(chams_state_path().c_str(), "a");
    if (!f) return;
    fprintf(f, "%s\n", line);
    fclose(f);
}

static std::unordered_set<std::string> g_chams_blacklist;

static void chams_blacklist_add(const std::string& name) {
    if (g_chams_blacklist.count(name)) return;
    g_chams_blacklist.insert(name);
    FILE* f = fopen(chams_blacklist_path().c_str(), "a");
    if (f) { fprintf(f, "%s\n", name.c_str()); fclose(f); }
    ilog("[chams] BLACKLISTED (crashed last session): %s\n", name.c_str());
}

// on init: TESTING without following CLEAN = that material crashed the game
static void chams_crash_check() {
    FILE* f = fopen(chams_blacklist_path().c_str(), "r");
    if (f) {
        char line[512];
        while (fgets(line, sizeof(line), f)) {
            std::string s = line;
            while (!s.empty() && (s.back() == '\n' || s.back() == '\r' || s.back() == ' ')) s.pop_back();
            if (!s.empty()) g_chams_blacklist.insert(s);
        }
        fclose(f);
    }
    f = fopen(chams_state_path().c_str(), "r");
    if (!f) return;
    char line[512];
    std::string last_testing;
    while (fgets(line, sizeof(line), f)) {
        if (!strncmp(line, "TESTING ", 8)) {
            last_testing = line + 8;
            while (!last_testing.empty() && (last_testing.back() == '\n' || last_testing.back() == '\r'))
                last_testing.pop_back();
            // strip category tag "[P] "/"[V] "/"[D] " so blacklist stores the plain name
            if (last_testing.size() > 4 && last_testing[0] == '[')
                last_testing = last_testing.substr(4);
        } else if (!strncmp(line, "CLEAN", 5)) {
            last_testing.clear();
        }
    }
    fclose(f);
    // rotate state file for this session
    f = fopen(chams_state_path().c_str(), "w");
    if (f) fclose(f);
    if (!last_testing.empty()) chams_blacklist_add(last_testing);
}

// ==================== material lists (per-category) ====================
static std::vector<ChamMat> g_mats_pv;    // players+vehicles safe whitelist (shared)
static std::vector<ChamMat> g_mats_dep;   // deployables safe whitelist (extended)
static std::vector<ChamMat> g_chams_mats_all;  // broad scan (testing)

// per-category material slots: 0=players 1=vehicles 2=deployables
static UObject* g_chams_mat_cat[3] = { nullptr, nullptr, nullptr };
static const char* kChamsCatTag[3] = { "P", "V", "D" };

static std::vector<ChamMat>& chams_list_for(int cat) {
    if (vars::chams_list == 1) return g_chams_mats_all;
    return cat == 2 ? g_mats_dep : g_mats_pv;
}

static int chams_idx_get(int cat) {
    return cat == 0 ? vars::chams_mat : cat == 1 ? vars::chams_mat_veh : vars::chams_mat_dep;
}
static void chams_idx_set(int cat, int v) {
    if (cat == 0) vars::chams_mat = v;
    else if (cat == 1) vars::chams_mat_veh = v;
    else vars::chams_mat_dep = v;
}

static void chams_select(int cat, int idx) {
    auto& list = chams_list_for(cat);
    if (list.empty()) return;
    int n = (int)list.size();
    int i = ((idx % n) + n) % n;
    chams_idx_set(cat, i);
    chams_restore_all();
    ChamMat& cm = list[i];
    g_chams_mat_cat[cat] = cm.obj;
    try {
        // depth test off (bit0 @0x1D8). Do NOT set usage bits - shaders are
        // already cooked; runtime flag can't add skel-mesh permutations.
        fld_ref<uint8_t>((uint64_t)cm.obj, off::MatDisableDepthTest) |= 0x01;
    } catch (...) {}
    // record immediately: if the game dies now, this material gets blacklisted
    chams_state_write((std::string("TESTING [") + kChamsCatTag[cat] + "] " + cm.full).c_str());
    ilog("[chams] %s selected [%d/%d] %s%s\n", kChamsCatTag[cat], i + 1, n, cm.full.c_str(),
         cm.invisible_hint ? " (invisible-type)" : "");
}

static void chams_use_list() {
    for (int c = 0; c < 3; c++) chams_select(c, chams_idx_get(c));
}

// helpers to build a whitelist with dedupe
static void chams_wl_add_full(std::vector<ChamMat>& list, const char* full, bool invis) {
    if (g_chams_blacklist.count(full)) { ilog("[chams] skip blacklisted: %s\n", full); return; }
    for (auto& m : list) if (m.full == full) return;
    UObject* o = ObjectArray->FindObject(full);
    if (!o) { ilog("[chams] missing: %s\n", full); return; }
    ChamMat cm;
    cm.full = full;
    cm.short_name = cm.full.substr(cm.full.find_last_of('.') + 1);
    cm.obj = o;
    cm.invisible_hint = invis;
    list.push_back(cm);
}
static void chams_wl_add_sub(std::vector<ChamMat>& list, const char* sub) {
    auto hits = ObjectArray->FindObjectsByString(sub);
    for (auto h : hits) {
        std::string fn = h->GetFullName();
        if (fn.rfind("Material ", 0) != 0) continue;
        if (g_chams_blacklist.count(fn)) { ilog("[chams] skip blacklisted: %s\n", fn.c_str()); return; }
        for (auto& m : list) if (m.full == fn) return;
        ChamMat cm;
        cm.full = fn;
        cm.short_name = fn.substr(fn.find_last_of('.') + 1);
        cm.obj = h;
        cm.invisible_hint = false;
        list.push_back(cm);
        ilog("[chams] whitelist+ resolved '%s' -> %s\n", sub, fn.c_str());
        return;
    }
    ilog("[chams] whitelist+ NOT FOUND: %s\n", sub);
}

static void chams_find_material() {
    chams_crash_check();

    // ---- P/V safe whitelist [user-verified on players AND vehicles] ----
    chams_wl_add_full(g_mats_pv, "Material /Game/Gameplay/Deployables/M_DeployableGhostMat.M_DeployableGhostMat", false);
    chams_wl_add_full(g_mats_pv, "Material /Game/Materials/M_Glass2.M_Glass2", false);
    chams_wl_add_full(g_mats_pv, "Material /Game/Art/Master_Materials/M_TransparentBlur.M_TransparentBlur", false);
    chams_wl_add_sub(g_mats_pv, "Master_Glass_singlesided");

    // ---- D safe whitelist [user-verified on deployables] = PV set + extras ----
    for (auto& m : g_mats_pv) g_mats_dep.push_back(m);
    chams_wl_add_full(g_mats_dep, "Material /Game/UI/Widgets/Menu_Main/M_UI_TitleBgGradient.M_UI_TitleBgGradient", false);
    chams_wl_add_full(g_mats_dep, "Material /Game/UI/Widgets/Menu_Escape/Materials/M_Horizontal_Gradient.M_Horizontal_Gradient", false);
    chams_wl_add_full(g_mats_dep, "Material /Game/UI/Widgets/Menu_Deployment/Screen_Team/M_UI_InfoGradient.M_UI_InfoGradient", false);
    chams_wl_add_full(g_mats_dep, "Material /Game/UI/Widgets/Menu_Deployment/Screen_Team/M_UI_VerticalGradient.M_UI_VerticalGradient", false);
    chams_wl_add_full(g_mats_dep, "Material /Game/UI/Widgets/Menu_Escape/Materials/M_UI_AngledGradient.M_UI_AngledGradient", false);
    chams_wl_add_full(g_mats_dep, "Material /Game/Vehicles/Emplaced_Kornet/Reticles/M_Reticle_Glow.M_Reticle_Glow", false);
    chams_wl_add_full(g_mats_dep, "Material /Game/UI/Widgets/Menu_Escape/Materials/M_Vertical_Gradient_Inverted.M_Vertical_Gradient_Inverted", false);
    chams_wl_add_full(g_mats_dep, "Material /Game/UI/Widgets/GameEventHUD/M_DoubleGradient.M_DoubleGradient", false);
    chams_wl_add_full(g_mats_dep, "Material /Game/Gameplay/GameModes/TC/HexGrid/Base_Transparent.Base_Transparent", false);
    chams_wl_add_sub(g_mats_dep, "M_ReflectiveLens_ADs");
    chams_wl_add_sub(g_mats_dep, "ShelterGlass");

    ilog("[chams] safe lists: PV=%zu D=%zu\n", g_mats_pv.size(), g_mats_dep.size());

    // ---- all list: broad keyword scan (Ribbon/Raymarching excluded - proven
    // D3D12 index-buffer crashers from earlier test; blacklist also applied) ----
    const char* keys[] = { "GradientRadial", "Gradient", "Glow", "FlatColor", "SolidColor",
                           "Invis", "Transparent", "Ghost", "Glass", "Lens", "Smoke" };
    const char* banned[] = { "Ribbon", "Raymarching" };
    for (uint32_t i = 0; i < (uint32_t)ObjectArray->NumElements; i++) {
        UObject* o = ObjectArray->GetObjectPtr(i);
        if (!o || !ptr_sane(o)) continue;
        std::string fn = o->GetFullName();
        if (fn.rfind("Material ", 0) != 0) continue;
        bool invis = false, match = false, ban = false;
        for (auto k : banned) if (fn.find(k) != std::string::npos) { ban = true; break; }
        if (ban) continue;
        for (auto k : keys) {
            if (fn.find(k) != std::string::npos) {
                match = true;
                for (auto ik : { "Invis", "Transparent", "Ghost", "Glass", "Lens", "Smoke" })
                    if (fn.find(ik) != std::string::npos) { invis = true; break; }
                break;
            }
        }
        if (!match || g_chams_blacklist.count(fn)) continue;
        bool dup = false;
        for (auto& m : g_chams_mats_all) if (m.obj == o) { dup = true; break; }
        if (dup) continue;
        ChamMat cm;
        cm.full = fn;
        cm.short_name = fn.substr(fn.find_last_of('.') + 1);
        cm.obj = o;
        cm.invisible_hint = invis;
        g_chams_mats_all.push_back(cm);
    }
    ilog("[chams] all list: %zu materials (blacklist active: %zu)\n",
         g_chams_mats_all.size(), g_chams_blacklist.size());

    chams_use_list();
}

std::string chams_current_name(int cat) {
    auto& list = chams_list_for(cat);
    if (list.empty() || cat < 0 || cat > 2) return "none";
    int n = (int)list.size();
    int idx = chams_idx_get(cat) % n;
    if (idx < 0) idx = 0;
    return list[idx].short_name;
}
std::string chams_list_name() {
    return vars::chams_list == 1 ? "All" : "Safe";
}
void chams_next(int cat) { chams_select(cat, chams_idx_get(cat) + 1); }
void chams_toggle_list() {
    vars::chams_list = vars::chams_list == 1 ? 0 : 1;
    chams_use_list();
}

static void chams_apply(uint64_t mesh_addr, UObject* mat) {
    if (!mat || !ptr_sane((void*)mesh_addr)) return;
    auto& st = g_chams_state[mesh_addr];
    st.seen = std::chrono::steady_clock::now();
    if (st.applied) return;
    USkeletalMeshComponent* mesh = (USkeletalMeshComponent*)mesh_addr;
    auto mats = mesh->GetMaterials();
    if (!ptr_sane(mats.data) || mats.count <= 0 || mats.count > 64) {
        static int fail_log = 0;
        if (fail_log++ < 8) ilog("[chams] apply failed mesh=%p count=%d\n", (void*)mesh_addr, mats.count);
        return;
    }
    st.originals.assign(mats.data, mats.data + mats.count);
    for (int i = 0; i < mats.count; i++)
        mesh->SetMaterial(i, mat);
    st.applied = true;
    static int ok_log = 0;
    if (ok_log++ < 8) ilog("[chams] applied mesh=%p slots=%d\n", (void*)mesh_addr, mats.count);
}

// ==================== radar ====================
static void draw_radar(UCanvas* canvas, const std::vector<EspTarget>& targets,
                       const std::vector<Entity>& ents, int32_t local_team) {
    if (!vars::radar) return;
    double size = 200.0;
    double cx = 20.0 + size * 0.5;
    double cy = g_cam.screen_h - 20.0 - size * 0.5;
    double scale = (size * 0.5) / vars::radar_range;   // px per meter
    double yaw = g_cam.rotation.yaw * (M_PI / 180.0);
    double cyaw = cos(yaw), syaw = sin(yaw);

    RenderBox(canvas, { cx - size * 0.5, cy - size * 0.5 }, { cx + size * 0.5, cy + size * 0.5 },
              { 1.f, 1.f, 1.f, 0.4f }, 1.f);
    // cross at center (self)
    RenderLine(canvas, { cx - 4, cy }, { cx + 4, cy }, 1.f, { 1.f, 1.f, 1.f, 0.8f });
    RenderLine(canvas, { cx, cy - 4 }, { cx, cy + 4 }, 1.f, { 1.f, 1.f, 1.f, 0.8f });

    auto plot = [&](const FVector& pos, FLinearColor c) {
        FVector d = pos - g_cam.location;
        double fwd = (d.x * cyaw + d.y * syaw) * 0.01;   // meters
        double rgt = (-d.x * syaw + d.y * cyaw) * 0.01;
        if (fabs(fwd) > vars::radar_range || fabs(rgt) > vars::radar_range) return;
        double px = cx + rgt * scale;
        double py = cy - fwd * scale;
        if (px < cx - size * 0.5 + 2) px = cx - size * 0.5 + 2;
        if (px > cx + size * 0.5 - 2) px = cx + size * 0.5 - 2;
        if (py < cy - size * 0.5 + 2) py = cy - size * 0.5 + 2;
        if (py > cy + size * 0.5 - 2) py = cy + size * 0.5 - 2;
        RenderLine(canvas, { px - 2.5, py }, { px + 2.5, py }, 2.f, c);
        RenderLine(canvas, { px, py - 2.5 }, { px, py + 2.5 }, 2.f, c);
    };

    for (const auto& t : targets) {
        if (t.teammate && !vars::show_teammates) continue;
        plot(t.pos, t.teammate ? FLinearColor{ 0.2f, 0.5f, 1.f, 1.f }
             : t.is_bot   ? FLinearColor{ 1.f, 0.6f, 0.f, 1.f }
                          : FLinearColor{ 1.f, 0.1f, 0.1f, 1.f });
    }
    for (const auto& e : ents) {
        if (e.cat == EntCat::Vehicle)
            plot(e.pos, { 1.f, 0.45f, 0.1f, 1.f });
        else if (e.cat == EntCat::FOB)
            plot(e.pos, { 1.f, 0.1f, 0.6f, 1.f });
    }
}

// ==================== tickets HUD ====================
static void draw_tickets(UCanvas* canvas, AGameStateBase* gs, int32_t local_team) {
    if (!vars::tickets_hud || !ptr_sane(gs)) return;
    try {
        auto ts = ((ASQGameState*)gs)->IndexedTeamStates();
        if (!ptr_sane(ts.data) || ts.count <= 0 || ts.count > 8) return;
        std::string line;
        for (int i = 0; i < ts.count; i++) {
            ASQTeamState* t = ts[i];
            if (!ptr_sane(t)) continue;
            char buf[64];
            snprintf(buf, sizeof(buf), "T%d: %d%s", t->Id(), t->Tickets(),
                     (local_team >= 0 && t->Id() == local_team) ? " *" : "");
            if (!line.empty()) line += "   |   ";
            line += buf;
        }
        if (!line.empty())
            RenderTextCentered(canvas, { g_cam.screen_w * 0.5, 30.0 }, line, { 1.f, 0.85f, 0.2f, 1.f });
    } catch (...) {}
}

// ==================== frame body ====================
static void hook_body(UGameViewportClient* viewport_client, UCanvas* canvas) {
    UWorld* world = viewport_client->World();
    if (!ptr_sane(world)) return;
    UGameInstance* gi = world->OwningGameInstance();
    if (!ptr_sane(gi)) return;
    auto lps = gi->LocalPlayers();
    if (!ptr_sane(lps.data) || lps.count < 1 || !ptr_sane(lps[0])) return;
    ULocalPlayer* lp = lps[0];
    APlayerController* ctrl = lp->PlayerController();
    if (!ptr_sane(ctrl)) return;
    APlayerCameraManager* cam_mgr = ctrl->CameraManager();
    if (!ptr_sane(cam_mgr)) return;

    g_cam.location = cam_mgr->CameraLocation();
    g_cam.rotation = cam_mgr->CameraRotation();
    g_cam.fov = cam_mgr->CameraFOV();
    g_cam.screen_w = fld<float>((uint64_t)canvas, 0x30);  // UCanvas::ClipX [SDK]
    g_cam.screen_h = fld<float>((uint64_t)canvas, 0x34);  // UCanvas::ClipY [SDK]
    if (g_cam.fov < 1.f || g_cam.fov > 170.f) g_cam.fov = 90.f;

    APawn* local_pawn = ctrl->AcknowledgedPawn();
    int32_t local_team = -1;
    ASQPlayerState* local_ps = (ASQPlayerState*)ctrl->PlayerState();
    if (ptr_sane(local_ps)) local_team = local_ps->TeamId();

    std::vector<EspTarget> targets;

    // ---- players via GameState->PlayerArray ----
    AGameStateBase* gs = world->GameState();
    if (ptr_sane(gs)) {
        auto pa = gs->PlayerArray();
        if (ptr_sane(pa.data) && pa.count > 0 && pa.count < 512) {
            for (int i = 0; i < pa.count; i++) {
                APlayerState* ps = pa[i];
                if (!ptr_sane(ps)) continue;
                // identity check first: PlayerState is the stable local identity
                // (AcknowledgedPawn is null/stale during respawn/spectate/vehicle transitions)
                if (ps == (APlayerState*)local_ps) continue;
                APawn* pawn = ps->PawnPrivate();
                if (!ptr_sane(pawn) || pawn == local_pawn) continue;

                EspTarget t;
                t.actor = (uint64_t)pawn;
                t.is_bot = ps->IsBot();
                ASQPlayerState* sqps = (ASQPlayerState*)ps;
                int32_t team = sqps->TeamId();
                t.teammate = (local_team >= 0 && team == local_team);
                t.name = ps->Name();
                t.pos = pawn->GetActorLocation();

                // batch A: PS-sourced info
                try {
                    t.role = sqps->RoleId();
                    t.kills = sqps->Kills();
                    t.deaths = sqps->Deaths();
                    t.is_admin = sqps->IsAdmin();
                    t.is_commander = sqps->IsCommander();
                } catch (...) {}

                ASQSoldier* soldier = (ASQSoldier*)pawn;
                uint64_t mesh = 0;
                if (probe_soldier_mesh((uint64_t)pawn, mesh)) {
                    t.mesh = mesh;
                    t.health = soldier->Health();
                    // alive check is BIT-BASED (external-verified): Health value
                    // is NOT replicated reliably for remote pawns - never filter on it
                    if (soldier->IsDying()) continue;             // dead
                    bool downed = soldier->IsWounded();
                    if (downed && !vars::show_downed) continue;   // incapacitated
                    // batch A: stance + weapon
                    try {
                        t.prone = soldier->IsProne();
                        t.crouched = soldier->IsCrouched();
                        t.sprinting = soldier->IsSprinting();
                        t.ads = soldier->IsADS();
                    } catch (...) {}
                    if (downed && t.name.find("(DOWN)") == std::string::npos)
                        t.name = "(DOWN) " + t.name;
                    fill_weapon_info(t);
                    FVector head;
                    t.bone_count = 0;
                    if (read_bones(mesh, t.bones, t.bone_count, head))
                        t.pos = t.bones[off::bones::Pelvis];
                }
                if (t.pos.is_zero()) continue;
                targets.push_back(t);
            }
        }
    }

    // ---- bots via level structural probe ----
    scan_bots(world, local_pawn);
    {
        auto now = std::chrono::steady_clock::now();
        for (auto& kv : g_bots) {
            uint64_t mesh = kv.second.mesh;
            if (!ptr_sane((void*)mesh)) continue;
            EspTarget t;
            t.actor = kv.first;
            t.mesh = mesh;
            t.is_bot = true;
            t.name = "BOT";
            try {
                ASQSoldier* bs = (ASQSoldier*)kv.first;
                // bit-based alive check only - Health@0x2740 is 0/stale for bots
                // (filtering on it made live bots invisible while they still shoot)
                if (bs->IsDying()) continue;
                bool downed = bs->IsWounded();
                if (downed && !vars::show_downed) continue;
                if (downed) t.name = "(DOWN) BOT";
                t.health = fld<float>(kv.first, off::SQHealth);   // display only
                if (!(t.health >= 0.f && t.health <= 10000.f)) t.health = -1.f;  // garbage (bots: not replicated)
                t.prone = bs->IsProne();
                t.crouched = bs->IsCrouched();
                t.sprinting = bs->IsSprinting();
                t.ads = bs->IsADS();
            } catch (...) { continue; }
            fill_weapon_info(t);
            FVector head;
            bool fresh = read_bones(mesh, t.bones, t.bone_count, head);
            if (fresh) {
                kv.second.last_ok = now;
                kv.second.bones.assign(t.bones, t.bones + t.bone_count);
                kv.second.bone_count = t.bone_count;
                kv.second.bones_time = now;
            } else if (kv.second.bone_count > 0 &&
                       std::chrono::duration_cast<std::chrono::milliseconds>(now - kv.second.bones_time).count() < 1000) {
                // grace: draw last good snapshot for up to 1s (mesh re-init flicker)
                memcpy(t.bones, kv.second.bones.data(), kv.second.bone_count * sizeof(FVector));
                t.bone_count = kv.second.bone_count;
            } else {
                continue;
            }
            t.pos = t.bones[off::bones::Pelvis];
            if (t.pos.is_zero()) continue;
            targets.push_back(t);
        }
    }

    // ---- world entities (vehicles/deployables/FOB) + capture zones ----
    static uint64_t g_frame = 0;
    g_frame++;
    std::vector<Entity> ents;
    collect_entities(world, ents);
    scan_capzones(world);

    // F9: dump entity mesh lists (chams coverage debug)
    static bool f9_prev = false;
    bool f9 = (GetAsyncKeyState(VK_F9) & 0x8000) != 0;
    if (f9 && !f9_prev) {
        ilog("[f9] entity mesh dump (%zu ents):\n", ents.size());
        int shown = 0;
        for (auto& e : ents) {
            if (shown++ >= 8) break;
            ilog("[f9] %s cat=%d meshes=%zu:\n", e.name.c_str(), (int)e.cat, e.meshes.size());
            for (uint64_t m : e.meshes) {
                std::string cn;
                try { cn = uclass_name(((UObject*)m)->ClassPrivate); } catch (...) {}
                ilog("[f9]   %p %s\n", (void*)m, cn.c_str());
            }
        }
    }
    f9_prev = f9;

    // ---- aimbot: pick closest to crosshair within fov (with target stickiness) ----
    EspTarget* aim_target = nullptr;
    double best_dist = 1e9;
    FRotator best_rot;
    FVector best_head;
    static uint64_t g_aim_lock = 0;
    bool rmb_held = (GetAsyncKeyState(VK_RBUTTON) & 0x8000) != 0;
    if (!rmb_held) g_aim_lock = 0;   // release clears the lock

    static const int bone_ids[3] = { off::bones::Head, off::bones::Neck, off::bones::Spine2 };
    const FRotator cam_norm = normalize_rot(g_cam.rotation);

    // ---- draw ----
    for (auto& t : targets) {
        if (t.is_bot && !vars::show_bots) continue;
        if (t.teammate && !vars::show_teammates) continue;
        double dist_m = g_cam.location.distance_m(t.pos);
        if (dist_m > vars::esp_max_dist) continue;

        draw_target(canvas, g_cam, t, ctrl);

        if (vars::aimbot && !t.teammate && t.bone_count > off::bones::Head) {
            int bid = bone_ids[vars::aim_bone % 3];
            if (bid >= t.bone_count) bid = off::bones::Head;
            FVector head = t.bones[bid];
            FRotator want = look_at_rotation(g_cam.location, head);
            double dp = clamp_angle(want.pitch - cam_norm.pitch);
            double dy = clamp_angle(want.yaw - cam_norm.yaw);
            double ang = sqrt(dp * dp + dy * dy);

            // sticky target: keep the locked actor while it stays valid & near-crosshair
            if (g_aim_lock != 0) {
                if (t.actor != g_aim_lock) continue;   // skip non-locked candidates
                if (ang > vars::aim_fov * 1.3) { g_aim_lock = 0; continue; }  // lost: reselect
            } else if (ang >= vars::aim_fov || ang >= best_dist) {
                continue;
            }

            if (vars::aim_visible_only) {
                if (!ctrl->LineOfSightTo((AActor*)t.actor, g_cam.location, false)) {
                    if (t.actor == g_aim_lock) g_aim_lock = 0;
                    continue;
                }
            }
            best_dist = ang;
            best_rot = want;
            best_head = head;
            aim_target = &t;
        }
    }
    if (aim_target) g_aim_lock = aim_target->actor;

    // ---- chams apply/restore (players + vehicles + deployables/FOBs) ----
    static bool chams_were_on = false;
    if (vars::chams) {
        for (auto& t : targets)
            if (!t.teammate && t.mesh) chams_apply(t.mesh, g_chams_mat_cat[0]);
        static int dbg_veh_total = 0, dbg_veh_mesh = 0, dbg_veh_skip = 0, dbg_dep_total = 0, dbg_dep_mesh = 0;
        dbg_veh_total = dbg_veh_mesh = dbg_veh_skip = dbg_dep_total = dbg_dep_mesh = 0;
        static std::string dbg_first_skip;
        for (auto& e : ents) {
            if (e.cat == EntCat::Vehicle) {
                dbg_veh_total++;
                if (e.meshes.empty() && !e.mesh) { dbg_veh_skip++; if (dbg_first_skip.empty()) dbg_first_skip = e.name + ":no-mesh"; continue; }
                dbg_veh_mesh++;
                if (!vars::chams_vehicles) { dbg_veh_skip++; if (dbg_first_skip.empty()) dbg_first_skip = e.name + ":toggle-off"; continue; }
                if (e.destroyed) { dbg_veh_skip++; if (dbg_first_skip.empty()) dbg_first_skip = e.name + ":destroyed"; continue; }
                // all mesh components (tripod + launcher/turret) - both teams
                for (uint64_t m : e.meshes) chams_apply(m, g_chams_mat_cat[1]);
                if (e.meshes.empty() && e.mesh) chams_apply(e.mesh, g_chams_mat_cat[1]);
            } else {
                dbg_dep_total++;
                if (e.meshes.empty() && !e.mesh) continue;
                dbg_dep_mesh++;
                if (vars::chams_deployables) {
                    for (uint64_t m : e.meshes) chams_apply(m, g_chams_mat_cat[2]);
                    if (e.meshes.empty() && e.mesh) chams_apply(e.mesh, g_chams_mat_cat[2]);
                }
            }
        }
        {
            static auto last_dbg = std::chrono::steady_clock::now();
            auto now = std::chrono::steady_clock::now();
            if (std::chrono::duration_cast<std::chrono::seconds>(now - last_dbg).count() >= 3) {
                last_dbg = now;
                int applied = 0;
                for (auto& kv : g_chams_state) if (kv.second.applied) applied++;
                ilog("[chams] veh=%d(mesh %d, skip %d%s) dep=%d(mesh %d) applied_total=%d P=%s V=%s D=%s\n",
                     dbg_veh_total, dbg_veh_mesh, dbg_veh_skip,
                     dbg_first_skip.empty() ? "" : (" first-skip=" + dbg_first_skip).c_str(),
                     dbg_dep_total, dbg_dep_mesh, applied,
                     chams_current_name(0).c_str(), chams_current_name(1).c_str(), chams_current_name(2).c_str());
                dbg_first_skip.clear();
            }
        }
        chams_were_on = true;
    } else if (chams_were_on) {
        chams_restore_all();
        chams_were_on = false;
    }

    // ---- aimbot FOV circle ----
    if (vars::aimbot) {
        // screen radius = tan(aim/2) / tan(camfov/2) * half_width
        double radius = tan((double)vars::aim_fov * 0.5 * (M_PI / 180.0)) /
                        tan((double)g_cam.fov * 0.5 * (M_PI / 180.0)) * (g_cam.screen_w * 0.5);
        if (radius > 1.0 && radius < 4000.0) {
            const int segs = 48;
            double cx = g_cam.screen_w * 0.5, cyc = g_cam.screen_h * 0.5;
            FVector2D prev{ cx + radius, cyc };
            for (int i = 1; i <= segs; i++) {
                double a = (double)i * (2.0 * M_PI) / segs;
                FVector2D p{ cx + cos(a) * radius, cyc + sin(a) * radius };
                RenderLine(canvas, prev, p, 1.f, { 1.f, 1.f, 1.f, 0.35f });
                prev = p;
            }
        }
    }

    // ---- aimbot: prediction + write (normalized, step-limited, dead-zoned) ----
    if (aim_target && rmb_held) {
        FVector aim_point = best_head;
        if (vars::aim_predict) {
            try {
                float muzzle = 0.f;
                ASQWeapon* lw = local_pawn ? ((ASQSoldier*)local_pawn)->HeldWeapon() : nullptr;
                if (ptr_sane(lw)) muzzle = fld<float>((uint64_t)lw, off::SQWeaponMuzzleVel);
                if (muzzle > 1000.f) {   // cm/s sanity
                    double dist_m = g_cam.location.distance_m(best_head);
                    double tof = (dist_m * 100.0) / (double)muzzle;
                    uint64_t movecomp = fld<uint64_t>(aim_target->actor, off::CharacterMovement);
                    if (ptr_sane((void*)movecomp)) {
                        FVector vel = fld<FVector>(movecomp, off::MoveVelocity);
                        aim_point = best_head + vel * tof;
                    }
                    best_rot = look_at_rotation(g_cam.location, aim_point);
                }
            } catch (...) {}
        }
        FRotator cur = normalize_rot(ctrl->ControlRotation());
        double dp = clamp_angle(best_rot.pitch - cur.pitch);
        double dy = clamp_angle(best_rot.yaw - cur.yaw);

        // dead zone: already on target, don't write (stops micro-jitter)
        if (fabs(dp) > 0.15 || fabs(dy) > 0.15) {
            double s = vars::aim_smooth < 1.f ? 1.f : vars::aim_smooth;
            double step_p = dp / s;
            double step_y = dy / s;
            // per-frame turn cap: no instant snaps on target switch
            double cap = vars::aim_max_step < 0.5f ? 0.5f : vars::aim_max_step;
            if (step_p > cap) step_p = cap; else if (step_p < -cap) step_p = -cap;
            if (step_y > cap) step_y = cap; else if (step_y < -cap) step_y = -cap;
            ctrl->ControlRotation() = { clamp_pitch(cur.pitch + step_p),
                                        clamp_angle(cur.yaw + step_y), 0.0 };
        }
        FVector2D hs;
        if (g_cam.world_to_screen(aim_point, hs))
            RenderTextCentered(canvas, hs, "X", { 0.f, 1.f, 0.f, 1.f });
    }

    draw_tickets(canvas, gs, local_team);
    draw_entities(canvas, g_cam, ents, local_team, g_frame);
    draw_capzones(canvas, g_cam, local_team);
    draw_grenades(canvas, g_cam, world);
    draw_radar(canvas, targets, ents, local_team);

    RenderText(canvas, { 15.0, 15.0 }, "Squad Int | v10.5.1", { 1.f, 0.f, 1.f, 1.f });

    if (!g_first_frame_logged) {
        g_first_frame_logged = true;
        ilog("[frame] first frame ok: screen=%.0fx%.0f fov=%.1f targets=%zu (bots cached=%zu) ents=%zu capzones=%zu local_team=%d\n",
             g_cam.screen_w, g_cam.screen_h, g_cam.fov, targets.size(), g_bots.size(),
             ents.size(), g_capzones.size(), local_team);
    }

    // periodic diagnostic: raw health/flags of first enemy target (health domain check)
    {
        static auto last_diag = std::chrono::steady_clock::now();
        auto now = std::chrono::steady_clock::now();
        if (std::chrono::duration_cast<std::chrono::seconds>(now - last_diag).count() >= 5) {
            last_diag = now;
            for (auto& t : targets) {
                if (t.teammate) continue;
                uint8_t flags = 0;
                try { flags = fld<uint8_t>(t.actor, off::SQHealthFlags); } catch (...) {}
                ilog("[diag] target %s bot=%d health=%f flags=0x%02X\n",
                     t.name.c_str(), t.is_bot ? 1 : 0, t.health, flags);
                break;
            }
        }
    }

    update_keybind();
    update_menu(canvas);

    // save config when menu closes
    static bool prev_open = true;
    if (prev_open && !vars::is_open) save_config();
    prev_open = vars::is_open;
}

void post_render_hook(UGameViewportClient* viewport_client, UCanvas* canvas) {
    // chams restore runs HERE (game thread) - requested by the unhook watcher
    if (g_chams_restore_req && !g_chams_restore_done) {
        try { chams_restore_all(); } catch (...) {}
        g_chams_restore_done = true;
    }
    if (!g_unhooking) {
        try {
            if (viewport_client && canvas)
                hook_body(viewport_client, canvas);
        } catch (...) {}
    }
    post_render_original(viewport_client, canvas);
}

// ==================== init ====================
static BYTE* vmt_hook(void** VFTable, uint32_t index, void* TargetFunction) {
    BYTE* org = reinterpret_cast<BYTE*>(VFTable[index]);
    DWORD protect = 0;
    VirtualProtect(&VFTable[index], 8, PAGE_EXECUTE_READWRITE, &protect);
    VFTable[index] = TargetFunction;
    VirtualProtect(&VFTable[index], 8, protect, 0);
    return org;
}

void init() {
    g_init_active = true;
    AllocConsole();
    freopen("CONIN$", "r", stdin);
    freopen("CONOUT$", "w", stdout);
    freopen("CONOUT$", "w", stderr);

    char tmp[MAX_PATH];
    if (GetTempPathA(MAX_PATH, tmp)) {
        std::string path = std::string(tmp) + "squad_int_log.txt";
        g_log = fopen(path.c_str(), "w");
        ilog("[init] log file: %s\n", path.c_str());
    }

    g_game_base = reinterpret_cast<uint64_t>(GetModuleHandle(NULL));
    g_append_string = g_game_base + rva::AppendString;
    g_gobjects = g_game_base + rva::GObjects;
    g_gworld = g_game_base + rva::GWorld;
    ilog("[init] base=%p appendstr=%p gobj=%p gworld=%p\n",
         (void*)g_game_base, (void*)g_append_string, (void*)g_gobjects, (void*)g_gworld);

    UWorld* world = *reinterpret_cast<UWorld**>(g_gworld);
    ilog("[init] world=%p\n", world);
    if (!ptr_sane(world)) { ilog("[init] FATAL: bad GWorld\n"); g_init_active = false; return; }
    ilog("[init] world name: %s\n", world->GetName().c_str());

    ObjectArray = reinterpret_cast<TUObjectArray*>(g_gobjects);
    ilog("[init] ObjectArray num=%d chunks=%d\n", ObjectArray->NumElements, ObjectArray->NumChunks);

    // ---- GObjects diagnostic: verify item resolution + full name format ----
    {
        int nonnull = 0, named = 0;
        for (uint32_t i = 0; i < (uint32_t)ObjectArray->NumElements; i++) {
            UObject* o = ObjectArray->GetObjectPtr(i);
            if (!o || !ptr_sane(o)) continue;
            nonnull++;
            if (named < 8) {
                std::string fn = o->GetFullName();
                if (!fn.empty()) { ilog("[diag] obj[%u] %s\n", i, fn.c_str()); named++; }
            }
            if (nonnull > 2000) break;
        }
        ilog("[diag] nonnull in first scan window: %d\n", nonnull);

        auto hits = ObjectArray->FindObjectsByString("K2_DrawText");
        ilog("[diag] K2_DrawText candidates: %zu\n", hits.size());
        for (auto h : hits) ilog("[diag]   %s\n", h->GetFullName().c_str());

        auto fonts = ObjectArray->FindObjectsByString("Roboto");
        ilog("[diag] Roboto candidates: %zu\n", fonts.size());
        for (auto f : fonts) ilog("[diag]   %s\n", f->GetFullName().c_str());
    }

    // verify ProcessEvent vtable index matches dump RVA
    auto pe_addr = world->GetProcessEventAddr();
    uint64_t pe_expect = g_game_base + rva::ProcessEvent;
    ilog("[init] vtable[0x4C]=%p expect=%p %s\n", (void*)pe_addr, (void*)pe_expect,
         pe_addr == pe_expect ? "MATCH" : "MISMATCH");

    // chain walk with retries (game may be at loading screen)
    UGameInstance* gi = nullptr;
    ULocalPlayer* lp = nullptr;
    APlayerController* ctrl = nullptr;
    UGameViewportClient* viewport = nullptr;
    for (int tries = 0; tries < 60; tries++) {
        if (g_unhooking) { g_init_active = false; return; }
        try {
            gi = world->OwningGameInstance();
            if (ptr_sane(gi)) {
                auto lps = gi->LocalPlayers();
                if (ptr_sane(lps.data) && lps.count > 0 && ptr_sane(lps[0])) {
                    lp = lps[0];
                    ctrl = lp->PlayerController();
                    viewport = lp->ViewportClient();
                    if (ptr_sane(ctrl) && ptr_sane(viewport)) break;
                }
            }
        } catch (...) {}
        Sleep(1000);
    }
    ilog("[init] gi=%p lp=%p ctrl=%p viewport=%p\n", gi, lp, ctrl, viewport);
    if (!ptr_sane(viewport)) { ilog("[init] FATAL: no viewport after 60s\n"); g_init_active = false; return; }
    if (g_unhooking) { g_init_active = false; return; }
    if (ptr_sane(ctrl)) ilog("[init] ctrl class: %s\n", ctrl->ClassPrivate ? ctrl->ClassPrivate->GetName().c_str() : "?");

    functions::init();

    chams_find_material();

    engine_font = ObjectArray->FindObject("Font /Engine/EngineFonts/Roboto.Roboto");
    ilog("[init] engine_font=%p\n", engine_font);
    if (!engine_font) ilog("[init] WARN: font not found (text disabled this run, lines still draw)\n");

    MH_Initialize();

    post_render_index = detect_post_render(viewport);
    ilog("[init] post_render_index=0x%X\n", post_render_index);
    if (post_render_index < 0) { ilog("[init] FATAL: PostRender not found\n"); g_init_active = false; return; }
    if (g_unhooking) { g_init_active = false; return; }

    void** vt = *(void***)viewport;
    void* target = vt[post_render_index];
    if (MH_CreateHook(target, &post_render_hook, reinterpret_cast<void**>(&post_render_original)) != MH_OK ||
        MH_EnableHook(target) != MH_OK) {
        ilog("[init] FATAL: PostRender hook install failed\n");
        g_init_active = false;
        return;
    }
    g_pr_target = target;
    g_pr_hooked = true;
    g_init_active = false;
    load_config();
    // apply config's list/index (renormalizes stale indices)
    chams_use_list();
    ilog("[init] PostRender hooked at slot 0x%02X (%p) - ready (END = unhook+eject)\n", post_render_index, target);
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved) {
    switch (ul_reason_for_call) {
    case DLL_PROCESS_ATTACH: {
        g_hmodule = hModule;
        CreateThread(nullptr, 0, unhook_watcher, nullptr, 0, nullptr);
        std::async(std::launch::async, init);
        break;
    }
    }
    return TRUE;
}
