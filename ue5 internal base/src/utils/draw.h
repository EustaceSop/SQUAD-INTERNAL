#pragma once
#include "../gui/ZeroGUI.h"

void RenderText(UCanvas* canvas, FVector2D Position, std::string text, FLinearColor color) {
    std::wstring ws(text.size(), L' ');
    ws.resize(std::mbstowcs(&ws[0], text.c_str(), text.size()));
    canvas->K2_DrawText(engine_font, ws.c_str(), Position, { 1.0, 1.0 }, color, 0.f, { 0.0f, 0.0f, 0.0f, 1.f }, { 0.0, 0.0 }, false, false, true, { 0.f, 0.f, 0.f, 1.f });
}

void RenderTextCentered(UCanvas* canvas, FVector2D Position, std::string text, FLinearColor color) {
    std::wstring ws(text.size(), L' ');
    ws.resize(std::mbstowcs(&ws[0], text.c_str(), text.size()));
    canvas->K2_DrawText(engine_font, ws.c_str(), Position, { 1.0, 1.0 }, color, 0.f, { 0.0f, 0.0f, 0.0f, 1.f }, { 0.0, 0.0 }, true, false, true, { 0.f, 0.f, 0.f, 1.f });
}

void RenderLine(UCanvas* canvas, FVector2D Pos1, FVector2D Pos2, float thickness, FLinearColor color) {
    canvas->K2_DrawLine(Pos1, Pos2, thickness, color);
}

void RenderBox(UCanvas* canvas, FVector2D TopLeft, FVector2D DownRight, FLinearColor color, float Thickness) {
    RenderLine(canvas, TopLeft, { DownRight.x, TopLeft.y }, Thickness, color);
    RenderLine(canvas, TopLeft, { TopLeft.x, DownRight.y }, Thickness, color);
    RenderLine(canvas, DownRight, { TopLeft.x, DownRight.y }, Thickness, color);
    RenderLine(canvas, DownRight, { DownRight.x, TopLeft.y }, Thickness, color);
}

void RenderHealthBar(UCanvas* canvas, FVector2D top, FVector2D bottom, float health_frac) {
    if (health_frac < 0.f) health_frac = 0.f;
    if (health_frac > 1.f) health_frac = 1.f;
    float x = top.x - 5.0;
    float h = bottom.y - top.y;
    // background
    RenderLine(canvas, { x, top.y }, { x, bottom.y }, 3.f, { 0.f, 0.f, 0.f, 0.7f });
    // fill
    float fill_top = bottom.y - h * health_frac;
    FLinearColor c = { 1.f - health_frac, health_frac, 0.f, 1.f };
    RenderLine(canvas, { x, fill_top }, { x, bottom.y }, 3.f, c);
}
