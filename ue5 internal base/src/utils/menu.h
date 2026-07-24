#pragma once
#include "draw.h"
#include "global.h"

// implemented in dllmain.cpp (chams material list)
std::string chams_current_name(int cat);
std::string chams_list_name();
void chams_next(int cat);
void chams_toggle_list();


void update_keybind() {
    if (GetAsyncKeyState(VK_INSERT) & 1) {
        vars::is_open = !vars::is_open;
    }
}

void update_menu(UCanvas* canvas) {
    ZeroGUI::SetupCanvas(canvas);

    ZeroGUI::Input::Handle();

    if (ZeroGUI::Window(L"Squad Internal v10.5.1", &vars::pos, { 600.f, 700.f }, vars::is_open)) {
        if (ZeroGUI::ButtonTab(L"Aimbot", FVector2D{ 197.f, 30.f }, vars::current_tab == 0))
            vars::current_tab = 0;
        ZeroGUI::SameLine();
        if (ZeroGUI::ButtonTab(L"Visual", FVector2D{ 197.f, 30.f }, vars::current_tab == 1))
            vars::current_tab = 1;
        ZeroGUI::SameLine();
        if (ZeroGUI::ButtonTab(L"World", FVector2D{ 197.f, 30.f }, vars::current_tab == 2))
            vars::current_tab = 2;

        switch (vars::current_tab) {
        case 0:
            ZeroGUI::Checkbox(L"Enable Aimbot", &vars::aimbot);
            ZeroGUI::SliderFloat(L"Aim FOV", &vars::aim_fov, 5.f, 180.f);
            ZeroGUI::SliderFloat(L"Smooth", &vars::aim_smooth, 1.f, 20.f);
            ZeroGUI::SliderFloat(L"Max Step", &vars::aim_max_step, 1.f, 30.f);
            ZeroGUI::Checkbox(L"Visible Only", &vars::aim_visible_only);
            ZeroGUI::Checkbox(L"Prediction", &vars::aim_predict);
            if (ZeroGUI::Button(vars::aim_bone == 0 ? L"Bone: Head" : vars::aim_bone == 1 ? L"Bone: Neck" : L"Bone: Chest", { 180.f, 25.f }))
                vars::aim_bone = (vars::aim_bone + 1) % 3;
            break;
        case 1:
            ZeroGUI::Checkbox(L"Box", &vars::box);
            ZeroGUI::Checkbox(L"Skeleton", &vars::skeleton);
            ZeroGUI::Checkbox(L"Name", &vars::name);
            ZeroGUI::Checkbox(L"Distance", &vars::distance);
            ZeroGUI::Checkbox(L"Health Bar", &vars::health_bar);
            ZeroGUI::Checkbox(L"Weapon + Ammo", &vars::weapon_info);
            ZeroGUI::Checkbox(L"Role", &vars::role_info);
            ZeroGUI::Checkbox(L"Stance", &vars::stance_info);
            ZeroGUI::Checkbox(L"K/D", &vars::kd_info);
            ZeroGUI::Checkbox(L"Tickets HUD", &vars::tickets_hud);
            ZeroGUI::Checkbox(L"Show Bots", &vars::show_bots);
            ZeroGUI::Checkbox(L"Show Teammates", &vars::show_teammates);
            ZeroGUI::Checkbox(L"Show Downed", &vars::show_downed);
            ZeroGUI::SliderFloat(L"Max Distance", &vars::esp_max_dist, 50.f, 2000.f);
            break;
        case 2:
            ZeroGUI::Checkbox(L"Vehicle ESP", &vars::vehicle_esp);
            ZeroGUI::Checkbox(L"Vehicle Weakpoints", &vars::vehicle_wp);
            ZeroGUI::Checkbox(L"FOB ESP", &vars::fob_esp);
            ZeroGUI::Checkbox(L"Deployable ESP", &vars::deploy_esp);
            ZeroGUI::Checkbox(L"Mine/IED Warning", &vars::mine_warn);
            ZeroGUI::Checkbox(L"Capture Zone HUD", &vars::capzone_hud);
            ZeroGUI::Checkbox(L"Grenade Warning", &vars::grenade_warn);
            ZeroGUI::Checkbox(L"Chams (Enemies)", &vars::chams);
            {
                wchar_t l1[160], l2[160], l3[160], l4[64];
                swprintf(l1, 160, L"P Mat: %hs", chams_current_name(0).c_str());
                if (ZeroGUI::Button(l1, { 270.f, 25.f })) chams_next(0);
                ZeroGUI::SameLine();
                swprintf(l2, 160, L"V Mat: %hs", chams_current_name(1).c_str());
                if (ZeroGUI::Button(l2, { 270.f, 25.f })) chams_next(1);
                swprintf(l3, 160, L"D Mat: %hs", chams_current_name(2).c_str());
                if (ZeroGUI::Button(l3, { 270.f, 25.f })) chams_next(2);
                ZeroGUI::SameLine();
                swprintf(l4, 64, L"List: %hs", chams_list_name().c_str());
                if (ZeroGUI::Button(l4, { 120.f, 25.f })) chams_toggle_list();
            }
            ZeroGUI::Checkbox(L"Chams Vehicles", &vars::chams_vehicles);
            ZeroGUI::Checkbox(L"Chams Deployables", &vars::chams_deployables);
            ZeroGUI::Checkbox(L"Radar", &vars::radar);
            ZeroGUI::SliderFloat(L"Radar Range", &vars::radar_range, 50.f, 1000.f);
            ZeroGUI::SliderFloat(L"World Max Distance", &vars::world_max_dist, 100.f, 4000.f);
            ZeroGUI::SliderFloat(L"Label Distance", &vars::world_label_dist, 100.f, 2000.f);
            break;
        }
    }

    ZeroGUI::Draw_Cursor(vars::is_open);

    ZeroGUI::Render();
}
