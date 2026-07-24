#include "sdk_functions.h"
#include <cstdio>

extern TUObjectArray* ObjectArray;
extern void ilog(const char* fmt, ...);

namespace functions {
	UObject* K2_DrawText_FN = 0;
	UObject* K2_DrawLine_FN = 0;
	UObject* LineOfSightTo_FN = 0;
	UObject* GetMaterials_FN = 0;
	UObject* SetMaterial_FN = 0;

	void init() {
		K2_DrawText_FN = ObjectArray->FindObject("Function /Script/Engine.Canvas.K2_DrawText");
		ilog("[fn] K2_DrawText = %p\n", K2_DrawText_FN);

		K2_DrawLine_FN = ObjectArray->FindObject("Function /Script/Engine.Canvas.K2_DrawLine");
		ilog("[fn] K2_DrawLine = %p\n", K2_DrawLine_FN);

		LineOfSightTo_FN = ObjectArray->FindObject("Function /Script/Engine.Controller.LineOfSightTo");
		ilog("[fn] LineOfSightTo = %p\n", LineOfSightTo_FN);

		GetMaterials_FN = ObjectArray->FindObject("Function /Script/Engine.MeshComponent.GetMaterials");
		ilog("[fn] GetMaterials = %p\n", GetMaterials_FN);

		SetMaterial_FN = ObjectArray->FindObject("Function /Script/Engine.PrimitiveComponent.SetMaterial");
		ilog("[fn] SetMaterial = %p\n", SetMaterial_FN);
	}
}
