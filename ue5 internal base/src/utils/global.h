#pragma once

namespace vars {
	FVector2D pos = { 300.0, 200.0 };

	bool is_open = true;
	int current_tab = 0;

	// aimbot
	bool aimbot = false;
	float aim_fov = 60.f;
	float aim_smooth = 4.f;
	bool aim_visible_only = true;
	float aim_max_step = 12.f;    // max degrees per frame (anti-snap)

	// visuals
	bool box = true;
	bool skeleton = true;
	bool name = true;
	bool distance = true;
	bool health_bar = true;
	bool show_bots = true;        // bots (no PlayerState) from level probe
	bool show_teammates = false;
	bool show_downed = false;     // incapacitated (dying/wounded) players
	float esp_max_dist = 800.f;   // meters

	// batch A: info
	bool weapon_info = true;
	bool role_info = true;
	bool stance_info = true;
	bool kd_info = true;
	bool tickets_hud = true;

	// batch B: world
	bool vehicle_esp = true;
	bool vehicle_wp = true;
	bool fob_esp = true;
	bool deploy_esp = true;
	bool mine_warn = true;
	bool capzone_hud = true;
	float world_max_dist = 1500.f;  // meters
	float world_label_dist = 700.f; // text labels beyond this = dot marker only

	// batch C
	bool chams = false;           // through-wall material swap (enemies)
	bool chams_vehicles = true;   // chams on enemy vehicles
	bool chams_deployables = true;// chams on deployables/FOBs (both teams)
	int chams_mat = 0;            // player material index
	int chams_mat_veh = 0;        // vehicle material index
	int chams_mat_dep = 0;        // deployable material index
	int chams_list = 0;           // 0=safe whitelist, 1=all scanned materials
	bool radar = true;
	float radar_range = 250.f;    // meters
	bool grenade_warn = true;
	int aim_bone = 0;             // 0=head 1=neck 2=chest
	bool aim_predict = true;
}
