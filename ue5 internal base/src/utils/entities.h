#pragma once
// Entity system: vehicles / deployables / FOB / capture zones.
// Classification via UClass chain walk (UStruct::SuperStruct @0x40 [SDK]).
#include "../sdk/sdk.h"
#include "draw.h"
#include "global.h"
#include <unordered_map>
#include <chrono>
#include <algorithm>

struct FBoxSphereBounds { FVector origin; FVector extent; double radius; };
static_assert(sizeof(FBoxSphereBounds) == 0x38, "FBoxSphereBounds size");

enum class EntCat : int { None = 0, Soldier, Vehicle, FOB, Deployable, Grenade, Other };

// ==================== class chain classification (cached) ====================
static std::unordered_map<uint64_t, EntCat> g_class_cat;
static std::unordered_map<uint64_t, std::string> g_class_name_cache;

inline const std::string& uclass_name(UClass* c) {
    auto it = g_class_name_cache.find((uint64_t)c);
    if (it != g_class_name_cache.end()) return it->second;
    std::string n;
    try { n = c->GetName(); } catch (...) {}
    return g_class_name_cache[(uint64_t)c] = n;
}

// check class chain for a base class name
inline bool class_chain_has(UClass* c, const char* base) {
    UStruct* cur = c;
    for (int depth = 0; cur && ptr_sane(cur) && depth < 40; depth++) {
        if (uclass_name((UClass*)cur) == base) return true;
        cur = cur->SuperStruct;
    }
    return false;
}

inline EntCat classify_class(UClass* c) {
    if (!ptr_sane(c)) return EntCat::None;
    auto it = g_class_cat.find((uint64_t)c);
    if (it != g_class_cat.end()) return it->second;
    EntCat cat = EntCat::Other;
    try {
        if (class_chain_has(c, "SQSoldier")) cat = EntCat::Soldier;
        else if (class_chain_has(c, "SQVehicle")) cat = EntCat::Vehicle;
        else if (class_chain_has(c, "SQForwardBase")) cat = EntCat::FOB;
        else if (class_chain_has(c, "SQDeployable")) cat = EntCat::Deployable;
        else if (class_chain_has(c, "SQGrenadeProjectile")) cat = EntCat::Grenade;
    } catch (...) {}
    g_class_cat[(uint64_t)c] = cat;
    return cat;
}

inline EntCat classify_actor(uint64_t actor) {
    try {
        UObject* o = (UObject*)actor;
        if (!ptr_sane(o->ClassPrivate)) return EntCat::None;
        return classify_class(o->ClassPrivate);
    } catch (...) { return EntCat::None; }
}

// clean display name from BP class name: "BP_BTR80_C" -> "BTR80"
inline std::string pretty_class_name(UObject* obj) {
    std::string n;
    try { n = obj->ClassPrivate ? uclass_name(obj->ClassPrivate) : ""; } catch (...) {}
    if (n.rfind("BP_", 0) == 0) n = n.substr(3);
    if (n.size() > 2 && n.compare(n.size() - 2, 2, "_C") == 0) n = n.substr(0, n.size() - 2);
    return n;
}

// ==================== entities ====================
struct EntWeakPoint {
    char tag;          // E/T/A/R/?
    float health;
    FVector pos;
};

struct Entity {
    uint64_t actor = 0;
    EntCat cat = EntCat::None;
    std::string name;
    int team = -1;
    FVector pos;
    FVector center;          // bounds union center (box center), falls back to pos
    FQuat rot{};
    FVector extent;        // box half-size in entity local frame (UE units)
    bool has_extent = false;
    uint64_t mesh = 0;       // primary mesh (vehicles: VehicleMesh)
    std::vector<uint64_t> meshes;  // ALL mesh components (chams/bounds union)
    float health = -1.f;
    float max_health = -1.f;
    int seats_filled = 0;
    int seats_total = 0;
    bool destroyed = false;
    bool built = true;
    bool sieged = false;
    std::vector<EntWeakPoint> wps;
};

// ==================== per-frame label de-overlap ====================
class LabelPlacer {
    struct Placed { double x, y; };
    std::vector<Placed> placed_;
public:
    void clear() { placed_.clear(); }
    void draw(UCanvas* canvas, double x, double y, const std::string& text, FLinearColor color) {
        double ny = y;
        for (int iter = 0; iter < 24; iter++) {
            bool clash = false;
            for (auto& p : placed_) {
                if (fabs(p.x - x) < 80.0 && fabs(p.y - ny) < 13.0) {
                    ny = p.y + 13.0;
                    clash = true;
                    break;
                }
            }
            if (!clash) break;
        }
        placed_.push_back({ x, ny });
        RenderTextCentered(canvas, { x, ny }, text, color);
    }
};
static LabelPlacer g_labels;

static char wp_tag_from_name(const std::string& cn) {
    std::string s = cn;
    for (auto& ch : s) ch = (char)tolower((unsigned char)ch);
    if (s.find("engine") != std::string::npos) return 'E';
    if (s.find("track") != std::string::npos) return 'T';
    if (s.find("ammo") != std::string::npos) return 'A';
    if (s.find("turret") != std::string::npos || s.find("ring") != std::string::npos) return 'R';
    return '?';
}

// deployable mesh resolution for chams: root mesh -> attach children meshes ->
// ChildActorComponent's child actor meshes -> InstanceComponents meshes.
// Squad deployables are frequently ChildActor-based (mesh lives on child actor).
static uint64_t deploy_mesh_of(uint64_t comp) {
    if (!ptr_sane((void*)comp)) return 0;
    UClass* cc = ((UObject*)comp)->ClassPrivate;
    if (!ptr_sane(cc)) return 0;
    return class_chain_has(cc, "MeshComponent") ? comp : 0;
}

static uint64_t find_deploy_mesh(uint64_t actor) {
    try {
        uint64_t root = fld<uint64_t>(actor, off::RootComponent);
        if (uint64_t m = deploy_mesh_of(root)) return m;
        if (ptr_sane((void*)root)) {
            auto kids = fld<TArray<uint64_t>>(root, off::AttachChildren);
            if (ptr_sane(kids.data) && kids.count > 0 && kids.count < 64) {
                for (int i = 0; i < kids.count; i++) {
                    uint64_t kid = kids[i];
                    if (uint64_t m = deploy_mesh_of(kid)) return m;
                    if (!ptr_sane((void*)kid)) continue;
                    UClass* kc = ((UObject*)kid)->ClassPrivate;
                    if (ptr_sane(kc) && class_chain_has(kc, "ChildActorComponent")) {
                        uint64_t child = fld<uint64_t>(kid, off::ChildActorActor);
                        if (!ptr_sane((void*)child)) continue;
                        uint64_t cr = fld<uint64_t>(child, off::RootComponent);
                        if (uint64_t m = deploy_mesh_of(cr)) return m;
                        if (ptr_sane((void*)cr)) {
                            auto ck = fld<TArray<uint64_t>>(cr, off::AttachChildren);
                            if (ptr_sane(ck.data) && ck.count > 0 && ck.count < 64) {
                                for (int j = 0; j < ck.count; j++)
                                    if (uint64_t m = deploy_mesh_of(ck[j])) return m;
                            }
                        }
                    }
                }
            }
        }
        auto inst = fld<TArray<uint64_t>>(actor, off::InstanceComponents);
        if (ptr_sane(inst.data) && inst.count > 0 && inst.count < 64) {
            for (int i = 0; i < inst.count; i++)
                if (uint64_t m = deploy_mesh_of(inst[i])) return m;
        }
    } catch (...) {}
    return 0;
}

// gather ALL mesh components of an actor (component tree + nested child actors).
// Depth only counts ACTOR boundaries (ChildActor crossings, max 6) - the
// component tree inside one actor is walked without limit. TOW-style nesting:
// deployable wrapper -> child actor vehicle pawn -> child actor launcher.
static void gather_meshes_rec(uint64_t comp, std::vector<uint64_t>& out, int actor_depth) {
    if (!ptr_sane((void*)comp) || actor_depth > 6) return;
    try {
        if (deploy_mesh_of(comp)) {
            bool dup = false;
            for (auto m : out) if (m == comp) { dup = true; break; }
            if (!dup) out.push_back(comp);
        }
        auto kids = fld<TArray<uint64_t>>(comp, off::AttachChildren);
        if (!ptr_sane(kids.data) || kids.count <= 0 || kids.count > 96) return;
        for (int i = 0; i < kids.count; i++) {
            uint64_t kid = kids[i];
            if (!ptr_sane((void*)kid)) continue;
            UClass* kc = ((UObject*)kid)->ClassPrivate;
            if (ptr_sane(kc) && class_chain_has(kc, "ChildActorComponent")) {
                uint64_t child = fld<uint64_t>(kid, off::ChildActorActor);
                if (ptr_sane((void*)child))
                    gather_meshes_rec(fld<uint64_t>(child, off::RootComponent), out, actor_depth + 1);
            } else {
                gather_meshes_rec(kid, out, actor_depth);
            }
        }
    } catch (...) {}
}

static void gather_meshes(uint64_t actor, std::vector<uint64_t>& out) {
    try {
        gather_meshes_rec(fld<uint64_t>(actor, off::RootComponent), out, 0);
    } catch (...) {}
}

static void collect_entities(UWorld* world, std::vector<Entity>& out) {
    auto levels = world->Levels();
    if (!ptr_sane(levels.data)) return;
    for (int li = 0; li < levels.count; li++) {
        ULevel* level = levels[li];
        if (!ptr_sane(level)) continue;
        auto actors = level->Actors();
        if (!ptr_sane(actors.data) || actors.count <= 0 || actors.count > 100000) continue;
        for (int ai = 0; ai < actors.count; ai++) {
            uint64_t actor = (uint64_t)actors[ai];
            if (!ptr_sane((void*)actor)) continue;
            EntCat cat = classify_actor(actor);
            if (cat != EntCat::Vehicle && cat != EntCat::FOB && cat != EntCat::Deployable)
                continue;
            try {
                Entity e;
                e.actor = actor;
                e.cat = cat;
                e.pos = ((AActor*)actor)->GetActorLocation();
                if (e.pos.is_zero()) continue;

                if (cat == EntCat::Vehicle) {
                    e.team = (int)fld<uint8_t>(actor, off::SQPawnTeam);
                    e.name = pretty_class_name((UObject*)actor);
                    e.health = fld<float>(actor, off::SQVehicleHealth);
                    e.max_health = fld<float>(actor, off::SQVehicleMaxHealth);
                    e.destroyed = fld<bool>(actor, off::SQVehicleDestroyed);

                    e.center = e.pos;
                    uint64_t root = fld<uint64_t>(actor, off::RootComponent);
                    e.rot = fld<FTransform>(root, off::ComponentToWorld).rotation;
                    uint64_t vmesh = fld<uint64_t>(actor, off::SQVehicleMesh);
                    if (ptr_sane((void*)vmesh)) e.mesh = vmesh;

                    // ALL mesh components (tripod + launcher/turret/child actors)
                    gather_meshes(actor, e.meshes);
                    if (e.mesh) {
                        bool has = false;
                        for (auto m : e.meshes) if (m == e.mesh) { has = true; break; }
                        if (!has) e.meshes.push_back(e.mesh);
                    }

                    // bounds UNION in entity-local frame over every mesh
                    // (VehicleMesh alone covers only the tripod on emplaced weapons)
                    {
                        FQuat inv{ -e.rot.x, -e.rot.y, -e.rot.z, e.rot.w };
                        FVector lmin{ 1e18, 1e18, 1e18 }, lmax{ -1e18, -1e18, -1e18 };
                        int nb = 0;
                        for (uint64_t m : e.meshes) {
                            FBoxSphereBounds b;
                            try { b = fld<FBoxSphereBounds>(m, off::CachedWorldBounds); } catch (...) { continue; }
                            if (b.extent.x < 1.0 || b.extent.x > 2000.0 ||
                                b.extent.y < 1.0 || b.extent.y > 2000.0 ||
                                b.extent.z < 1.0 || b.extent.z > 2000.0) continue;
                            if (b.origin.distance_m(e.pos) > 100.0) continue;   // stale bounds
                            FVector lo = quat_rotate(inv, b.origin - e.pos);
                            if (lo.x - b.extent.x < lmin.x) lmin.x = lo.x - b.extent.x;
                            if (lo.y - b.extent.y < lmin.y) lmin.y = lo.y - b.extent.y;
                            if (lo.z - b.extent.z < lmin.z) lmin.z = lo.z - b.extent.z;
                            if (lo.x + b.extent.x > lmax.x) lmax.x = lo.x + b.extent.x;
                            if (lo.y + b.extent.y > lmax.y) lmax.y = lo.y + b.extent.y;
                            if (lo.z + b.extent.z > lmax.z) lmax.z = lo.z + b.extent.z;
                            nb++;
                        }
                        if (nb > 0) {
                            FVector lc = (lmin + lmax) * 0.5;
                            e.extent = (lmax - lmin) * 0.5;
                            e.center = e.pos + quat_rotate(e.rot, lc);
                            e.has_extent = true;
                        }
                    }

                    // seats occupancy
                    auto seats = fld<TArray<uint64_t>>(actor, off::SQVehicleSeats);
                    if (ptr_sane(seats.data) && seats.count > 0 && seats.count < 32) {
                        e.seats_total = seats.count;
                        for (int s = 0; s < seats.count; s++) {
                            uint64_t seat = seats[s];
                            if (!ptr_sane((void*)seat)) continue;
                            if (fld<uint64_t>(seat, off::SQSeatSeatedPlayer) ||
                                fld<uint64_t>(seat, off::SQSeatSeatedSoldier))
                                e.seats_filled++;
                        }
                    }

                    // weakpoints
                    auto comps = fld<TArray<uint64_t>>(actor, off::SQVehicleComponents);
                    if (ptr_sane(comps.data) && comps.count > 0 && comps.count < 64) {
                        for (int ci = 0; ci < comps.count; ci++) {
                            uint64_t comp = comps[ci];
                            if (!ptr_sane((void*)comp)) continue;
                            EntWeakPoint wp;
                            wp.tag = wp_tag_from_name(uclass_name(((UObject*)comp)->ClassPrivate));
                            wp.health = fld<float>(comp, off::SQVCompHealth);
                            // position: own CTW if sane & near vehicle, else compose from RelativeLocation
                            FVector cp = fld<FTransform>(comp, off::ComponentToWorld).translation;
                            if (cp.distance_m(e.pos) > 30.0) {
                                FVector rel = fld<FVector>(comp, off::RelativeLocation);
                                cp = e.pos + quat_rotate(e.rot, rel);
                            }
                            wp.pos = cp;
                            e.wps.push_back(wp);
                        }
                    }
                } else {
                    // deployable / FOB
                    e.team = fld<int32_t>(actor, off::SQDeployTeam);
                    e.name = pretty_class_name((UObject*)actor);
                    e.health = fld<float>(actor, off::SQDeployHealth);
                    e.max_health = fld<float>(actor, off::SQDeployMaxHealth);
                    e.built = (fld<uint8_t>(actor, off::SQDeployBuildState) == 0);
                    if (cat == EntCat::FOB) {
                        e.sieged = fld<bool>(actor, off::SQFobSieged);
                        std::string fn = fld<FString>(actor, off::SQFobName).to_string();
                        if (!fn.empty()) e.name = "FOB " + fn;
                    }
                    // mesh components for chams (all meshes in component tree)
                    e.mesh = find_deploy_mesh(actor);
                    gather_meshes(actor, e.meshes);
                    if (e.mesh) {
                        bool has = false;
                        for (auto m : e.meshes) if (m == e.mesh) { has = true; break; }
                        if (!has) e.meshes.push_back(e.mesh);
                    }
                }
                out.push_back(std::move(e));
            } catch (...) {}
        }
    }
}

// 3D wire box from extent + rotation (12 edges), near-plane clipped lines
static void draw_oriented_box(UCanvas* canvas, const CameraView& cam, const FVector& pos,
                              const FQuat& rot, const FVector& ext, FLinearColor color) {
    FVector corners[8];
    int idx = 0;
    for (int sx = -1; sx <= 1; sx += 2)
        for (int sy = -1; sy <= 1; sy += 2)
            for (int sz = -1; sz <= 1; sz += 2)
                corners[idx++] = pos + quat_rotate(rot, { ext.x * sx, ext.y * sy, ext.z * sz });
    static const int edges[12][2] = {
        {0,1},{1,3},{3,2},{2,0}, {4,5},{5,7},{7,6},{6,4}, {0,4},{1,5},{2,6},{3,7}
    };
    for (auto& e : edges) {
        FVector2D a, b;
        if (!cam.project_line(corners[e[0]], corners[e[1]], a, b)) continue;
        RenderLine(canvas, a, b, 1.f, color);
    }
}

static void draw_entities(UCanvas* canvas, const CameraView& cam, const std::vector<Entity>& ents,
                          int32_t local_team, uint64_t frame) {
    g_labels.clear();

    // sort by distance ascending: near entities get the text-label budget first
    std::vector<const Entity*> sorted;
    sorted.reserve(ents.size());
    for (const auto& e : ents) sorted.push_back(&e);
    std::sort(sorted.begin(), sorted.end(), [&](const Entity* a, const Entity* b) {
        return cam.location.distance_m(a->pos) < cam.location.distance_m(b->pos);
    });
    int text_budget = 30;   // max text labels per frame; overflow = dot marker only

    for (const Entity* ep : sorted) {
        const auto& e = *ep;
        double dist_m = cam.location.distance_m(e.pos);
        if (dist_m > vars::world_max_dist) continue;
        bool friendly = (local_team >= 0 && e.team == local_team);
        bool text_ok = text_budget > 0 && dist_m <= vars::world_label_dist;

        if (e.cat == EntCat::Vehicle) {
            if (!vars::vehicle_esp) continue;
            FLinearColor color = e.destroyed ? FLinearColor{ 0.35f, 0.35f, 0.35f, 1.f }
                               : friendly  ? FLinearColor{ 0.2f, 0.5f, 1.f, 1.f }
                                           : FLinearColor{ 1.f, 0.45f, 0.1f, 1.f };
            if (e.has_extent && dist_m < 800.0)
                draw_oriented_box(canvas, cam, e.center, e.rot, e.extent, color);

            FVector2D s;
            if (!cam.project_unclamped(e.center, s)) continue;

            if (!text_ok) {   // far/budget: small dot only
                RenderLine(canvas, { s.x - 3, s.y }, { s.x + 3, s.y }, 2.f, color);
                RenderLine(canvas, { s.x, s.y - 3 }, { s.x, s.y + 3 }, 2.f, color);
                continue;
            }
            text_budget--;

            // label: name, seats, dist (de-overlapped)
            char label[160];
            snprintf(label, sizeof(label), "%s%s %d/%d [%.0fm]",
                     e.destroyed ? "(DEAD) " : "", e.name.c_str(),
                     e.seats_filled, e.seats_total, dist_m);
            g_labels.draw(canvas, s.x, s.y - 10.0, label, color);

            // health bar under label
            if (!e.destroyed && e.max_health > 0.f) {
                float frac = e.health / e.max_health;
                if (frac < 0.f) frac = 0.f; if (frac > 1.f) frac = 1.f;
                double bw = 40.0;
                RenderLine(canvas, { s.x - bw, s.y + 4.0 }, { s.x + bw, s.y + 4.0 }, 3.f, { 0.f, 0.f, 0.f, 0.7f });
                RenderLine(canvas, { s.x - bw, s.y + 4.0 }, { s.x - bw + 2 * bw * frac, s.y + 4.0 }, 3.f,
                           { 1.f - frac, frac, 0.f, 1.f });
            }

            // weakpoints (near only - they clutter at range)
            if (vars::vehicle_wp && !e.destroyed && !friendly && dist_m < 400.0) {
                for (const auto& wp : e.wps) {
                    FVector2D ws;
                    if (!cam.project_unclamped(wp.pos, ws)) continue;
                    char t[2] = { wp.tag, 0 };
                    g_labels.draw(canvas, ws.x, ws.y, t, { 1.f, 0.1f, 0.1f, 1.f });
                }
            }
        } else {
            // deployable / FOB
            bool is_fob = (e.cat == EntCat::FOB);
            if (is_fob && !vars::fob_esp) continue;
            if (!is_fob && !vars::deploy_esp) continue;

            std::string lowname = e.name;
            for (auto& ch : lowname) ch = (char)tolower((unsigned char)ch);
            bool is_explosive = (lowname.find("mine") != std::string::npos ||
                                 lowname.find("ied") != std::string::npos);

            FLinearColor color = friendly ? FLinearColor{ 0.2f, 0.5f, 1.f, 1.f }
                                          : FLinearColor{ 0.9f, 0.2f, 0.9f, 1.f };
            if (is_fob) color = friendly ? FLinearColor{ 0.2f, 0.8f, 1.f, 1.f }
                                         : FLinearColor{ 1.f, 0.1f, 0.6f, 1.f };

            FVector2D s;
            if (!cam.project_unclamped(e.pos, s)) continue;

            // small diamond marker = exact world anchor (always drawn)
            RenderLine(canvas, { s.x - 3, s.y }, { s.x, s.y - 3 }, 1.f, color);
            RenderLine(canvas, { s.x, s.y - 3 }, { s.x + 3, s.y }, 1.f, color);
            RenderLine(canvas, { s.x + 3, s.y }, { s.x, s.y + 3 }, 1.f, color);
            RenderLine(canvas, { s.x, s.y + 3 }, { s.x - 3, s.y }, 1.f, color);

            if (!text_ok) continue;
            text_budget--;

            // enemy explosive close warning (blink)
            if (is_explosive && !friendly && vars::mine_warn) {
                char label[192];
                if (dist_m < 100.0 && (frame / 20) % 2 == 0) {
                    snprintf(label, sizeof(label), "!!! %s [%.0fm] !!!", e.name.c_str(), dist_m);
                    g_labels.draw(canvas, s.x, s.y - 16.0, label, { 1.f, 0.f, 0.f, 1.f });
                } else {
                    snprintf(label, sizeof(label), "%s [%.0fm]", e.name.c_str(), dist_m);
                    g_labels.draw(canvas, s.x, s.y - 16.0, label, color);
                }
            } else {
                char label[192];
                std::string flags;
                if (!e.built) flags += " (unbuilt)";
                if (e.sieged) flags += " [SIEGED]";
                snprintf(label, sizeof(label), "%s%s [%.0fm]", e.name.c_str(), flags.c_str(), dist_m);
                g_labels.draw(canvas, s.x, s.y - 16.0, label, color);
            }
        }
    }
}

// ==================== grenade warning ====================
static void draw_grenades(UCanvas* canvas, const CameraView& cam, UWorld* world) {
    if (!vars::grenade_warn) return;
    auto levels = world->Levels();
    if (!ptr_sane(levels.data)) return;
    for (int li = 0; li < levels.count; li++) {
        ULevel* level = levels[li];
        if (!ptr_sane(level)) continue;
        auto actors = level->Actors();
        if (!ptr_sane(actors.data) || actors.count <= 0) continue;
        for (int ai = 0; ai < actors.count; ai++) {
            uint64_t actor = (uint64_t)actors[ai];
            if (!ptr_sane((void*)actor)) continue;
            if (classify_actor(actor) != EntCat::Grenade) continue;
            try {
                FVector pos = ((AActor*)actor)->GetActorLocation();
                double dist_m = cam.location.distance_m(pos);
                if (dist_m > 150.0) continue;
                bool exploding = fld<bool>(actor, off::SQGrenadeExploding);
                float fuse = fld<float>(actor, off::SQGrenadeFuseTime);
                FVector2D s;
                if (!cam.project_unclamped(pos, s)) continue;
                char label[96];
                snprintf(label, sizeof(label), "GRENADE %.1fs (%.0fm)", fuse, dist_m);
                g_labels.draw(canvas, s.x, s.y, label,
                              exploding ? FLinearColor{ 1.f, 0.f, 0.f, 1.f }
                                        : FLinearColor{ 1.f, 0.5f, 0.f, 1.f });
            } catch (...) {}
        }
    }
}

// ==================== capture zones ====================
struct CapZone {
    uint64_t comp = 0;
    std::string flag;
    FVector pos;
};
static std::vector<CapZone> g_capzones;
static std::chrono::steady_clock::time_point g_capzone_scan{};

static void scan_capzones(UWorld* world) {
    auto now = std::chrono::steady_clock::now();
    if (std::chrono::duration_cast<std::chrono::seconds>(now - g_capzone_scan).count() < 10)
        return;
    g_capzone_scan = now;
    g_capzones.clear();
    auto levels = world->Levels();
    if (!ptr_sane(levels.data)) return;
    for (int li = 0; li < levels.count; li++) {
        ULevel* level = levels[li];
        if (!ptr_sane(level)) continue;
        auto actors = level->Actors();
        if (!ptr_sane(actors.data) || actors.count <= 0) continue;
        for (int ai = 0; ai < actors.count; ai++) {
            uint64_t actor = (uint64_t)actors[ai];
            if (!ptr_sane((void*)actor)) continue;
            try {
                auto comps = fld<TArray<uint64_t>>(actor, off::InstanceComponents);
                if (!ptr_sane(comps.data) || comps.count <= 0 || comps.count > 64) continue;
                for (int ci = 0; ci < comps.count; ci++) {
                    uint64_t comp = comps[ci];
                    if (!ptr_sane((void*)comp)) continue;
                    UClass* cc = ((UObject*)comp)->ClassPrivate;
                    if (!ptr_sane(cc)) continue;
                    if (!class_chain_has(cc, "SQCaptureZoneComponent")) continue;
                    CapZone z;
                    z.comp = comp;
                    z.flag = read_ftext(comp + off::SQCapFlagName);
                    if (z.flag.empty()) z.flag = "Flag";
                    FVector p = fld<FTransform>(comp, off::ComponentToWorld).translation;
                    if (p.is_zero()) p = ((AActor*)actor)->GetActorLocation();
                    z.pos = p;
                    g_capzones.push_back(z);
                }
            } catch (...) {}
        }
    }
}

static void draw_capzones(UCanvas* canvas, const CameraView& cam, int32_t local_team) {
    if (!vars::capzone_hud) return;
    for (const auto& z : g_capzones) {
        try {
            uint8_t owning = fld<uint8_t>(z.comp, off::SQCapOwningTeam);
            uint8_t capturing = fld<uint8_t>(z.comp, off::SQCapCapturingTeam);
            float pct = fld<float>(z.comp, off::SQCapPercent);
            double dist_m = cam.location.distance_m(z.pos);
            if (dist_m > vars::world_max_dist) continue;

            FLinearColor color = { 0.7f, 0.7f, 0.7f, 1.f };
            if ((int)owning == local_team && local_team >= 0) color = { 0.2f, 0.6f, 1.f, 1.f };
            else if (owning != 0) color = { 1.f, 0.25f, 0.25f, 1.f };
            if (capturing != 0 && (int)capturing != (int)owning)
                color = { 1.f, 0.8f, 0.1f, 1.f };   // contested

            FVector2D s;
            if (!cam.project_unclamped(z.pos, s)) continue;
            char label[160];
            snprintf(label, sizeof(label), "[%s] %.0f%% (%.0fm)", z.flag.c_str(), pct * 100.f, dist_m);
            g_labels.draw(canvas, s.x, s.y, label, color);
        } catch (...) {}
    }
}
