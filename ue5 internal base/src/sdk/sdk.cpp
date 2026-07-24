#include "sdk.h"
#include "sdk_functions.h"

void UCanvas::K2_DrawLine(FVector2D a, FVector2D b, float thickness, FLinearColor color)
{
	if (!functions::K2_DrawLine_FN) return;
	Canvas_K2_DrawLine_Parms parms{};
	parms.ScreenPositionA = a;
	parms.ScreenPositionB = b;
	parms.Thickness = thickness;
	parms.RenderColor = color;
	try { ProcessEvent(functions::K2_DrawLine_FN, &parms); } catch (...) {}
}

void UCanvas::K2_DrawText(UObject* font, const wchar_t* text, FVector2D pos, FVector2D scale,
                          FLinearColor color, float kerning, FLinearColor shadow_color,
                          FVector2D shadow_offset, bool center_x, bool center_y, bool outlined,
                          FLinearColor outline_color)
{
	if (!functions::K2_DrawText_FN || !font || !text) return;
	Canvas_K2_DrawText_Parms parms{};
	parms.RenderFont = font;
	parms.RenderText = FString(text);
	parms.ScreenPosition = pos;
	parms.Scale = scale;
	parms.RenderColor = color;
	parms.Kerning = kerning;
	parms.ShadowColor = shadow_color;
	parms.ShadowOffset = shadow_offset;
	parms.bCentreX = center_x;
	parms.bCentreY = center_y;
	parms.bOutlined = outlined;
	parms.OutlineColor = outline_color;
	try { ProcessEvent(functions::K2_DrawText_FN, &parms); } catch (...) {}
}

bool AController::LineOfSightTo(AActor* Other, FVector ViewPoint, bool bAlternateChecks)
{
	if (!functions::LineOfSightTo_FN || !Other) return false;
	Controller_LineOfSightTo_Parms parms{};
	parms.Other = Other;
	parms.ViewPoint = ViewPoint;
	parms.bAlternateChecks = bAlternateChecks;
	try { ProcessEvent(functions::LineOfSightTo_FN, &parms); } catch (...) { return false; }
	return parms.ReturnValue;
}

// UMeshComponent::GetMaterials -> TArray<UMaterialInterface*> @0x0 [SDK]
TArray<UObject*> USkeletalMeshComponent::GetMaterials()
{
	TArray<UObject*> ret{};
	if (!functions::GetMaterials_FN) return ret;
	try { ProcessEvent(functions::GetMaterials_FN, &ret); } catch (...) {}
	return ret;
}

// UPrimitiveComponent::SetMaterial(int32 @0x0, UMaterialInterface* @0x8) [SDK]
void USkeletalMeshComponent::SetMaterial(int32_t slot, UObject* material)
{
	if (!functions::SetMaterial_FN || !material) return;
	struct { int32_t slot; char pad[4]; UObject* material; } parms{};
	parms.slot = slot;
	parms.material = material;
	try { ProcessEvent(functions::SetMaterial_FN, &parms); } catch (...) {}
}
