#pragma once
// Core engine primitives for Squad v10.5.1 (UE 5.7.4 build 627303, LWC double).
// All layouts verified against Dumper-7 dump:
//   C:\Dumper-7\5.7.4-627303+__Squad_v10.5.1-SquadGame
#include <wchar.h>
#include <cmath>
#include <locale>
#include <vector>
#include <string>
#include <Windows.h>
#include "fnamepool.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// ==================== Global offsets (RVA, [Dumper-7 Basic.hpp]) ====================
namespace rva
{
	constexpr uint64_t GObjects     = 0x0D0331B0;
	constexpr uint64_t AppendString = 0x013ED940;
	constexpr uint64_t GWorld       = 0x0D1C9EB8;
	constexpr uint64_t ProcessEvent = 0x01656580;  // verify: vtable[0x4C] of any UObject
	constexpr int32_t  ProcessEventIdx = 0x4C;
}

// ==================== Math (LWC double) ====================
class FVector {
public:
	double x = 0.0, y = 0.0, z = 0.0;

	FVector() = default;
	FVector(double x, double y, double z) : x(x), y(y), z(z) {}

	FVector operator+(const FVector& o) const { return { x + o.x, y + o.y, z + o.z }; }
	FVector operator-(const FVector& o) const { return { x - o.x, y - o.y, z - o.z }; }
	FVector operator*(double s) const { return { x * s, y * s, z * s }; }
	FVector& operator+=(const FVector& o) { x += o.x; y += o.y; z += o.z; return *this; }
	double dot(const FVector& o) const { return x * o.x + y * o.y + z * o.z; }
	double length() const { return sqrt(x * x + y * y + z * z); }
	double length_2d() const { return sqrt(x * x + y * y); }
	bool is_zero() const { return x == 0.0 && y == 0.0 && z == 0.0; }
	double distance_m(const FVector& o) const { return (*this - o).length() * 0.01; }
};
static_assert(sizeof(FVector) == 0x18, "FVector size");

class FVector2D {
public:
	double x = 0.0, y = 0.0;   // UE5.7: double (0x10) - UCanvas parms confirm

	FVector2D() = default;
	FVector2D(double x, double y) : x(x), y(y) {}
};
static_assert(sizeof(FVector2D) == 0x10, "FVector2D size");

class FRotator {
public:
	double pitch = 0.0, yaw = 0.0, roll = 0.0;

	FRotator() = default;
	FRotator(double p, double y, double r) : pitch(p), yaw(y), roll(r) {}

	double Distance2D(const FRotator& o) const {
		double dp = pitch - o.pitch, dy = yaw - o.yaw;
		return sqrt(dp * dp + dy * dy);
	}
};
static_assert(sizeof(FRotator) == 0x18, "FRotator size");

struct FQuat { double x, y, z, w; };
static_assert(sizeof(FQuat) == 0x20, "FQuat size");

struct FTransform
{
	FQuat   rotation;    // 0x00
	FVector translation; // 0x20
	char    pad0[8];
	FVector scale3d;     // 0x40
	char    pad1[8];
};
static_assert(sizeof(FTransform) == 0x60, "FTransform size");

class FLinearColor {
public:
	float r, g, b, a;
	FLinearColor() : r(0), g(0), b(0), a(0) {}
	FLinearColor(float r, float g, float b, float a) : r(r), g(g), b(b), a(a) {}
};
static_assert(sizeof(FLinearColor) == 0x10, "FLinearColor size");

struct FWeakObjectPtr
{
	int32_t ObjectIndex;
	int32_t ObjectSerialNumber;
};
static_assert(sizeof(FWeakObjectPtr) == 0x8, "FWeakObjectPtr size");

// Quaternion vector rotation (q * v * q^-1), verified in external build
inline FVector quat_rotate(const FQuat& q, const FVector& v)
{
	double ix = q.w * v.x + q.y * v.z - q.z * v.y;
	double iy = q.w * v.y + q.z * v.x - q.x * v.z;
	double iz = q.w * v.z + q.x * v.y - q.y * v.x;
	double iw = -q.x * v.x - q.y * v.y - q.z * v.z;
	return {
		ix * q.w + iw * -q.x + iy * -q.z - iz * -q.y,
		iy * q.w + iw * -q.y + iz * -q.x - ix * -q.z,
		iz * q.w + iw * -q.z + ix * -q.y - iy * -q.x
	};
}

// Bone local -> world via ComponentToWorld (scale -> rotate -> translate)
inline FVector transform_bone_to_world(const FTransform& ctw, const FTransform& bone)
{
	FVector scaled = { bone.translation.x * ctw.scale3d.x,
	                   bone.translation.y * ctw.scale3d.y,
	                   bone.translation.z * ctw.scale3d.z };
	return quat_rotate(ctw.rotation, scaled) + ctw.translation;
}

// ==================== UObject core [Dumper-7 CoreUObject_classes.hpp] ====================
class UObject
{
public:
	void*        vtable;       // 0x0000
	int32_t      ObjectFlags;  // 0x0008
	int32_t      InternalIndex;// 0x000C
	class UClass*    ClassPrivate;   // 0x0010
	struct FName     NamePrivate;    // 0x0018
	class UObject*   OuterPrivate;   // 0x0020

	void ProcessEvent(void* fn, void* parms);
	std::string GetName();
	std::string GetFullName();
	std::uintptr_t GetProcessEventAddr() {
		auto vt = *reinterpret_cast<void***>(this);
		return (uintptr_t)vt[rva::ProcessEventIdx];
	}
};

class UField : public UObject
{
public:
	class UField* Next;  // 0x0028
};

class UStruct : public UField
{
public:
	char             pad_0030[0x10];       // 0x0030 BaseChain
	class UStruct*   SuperStruct;          // 0x0040
	class UField*    Children;             // 0x0048
	void*            ChildProperties;      // 0x0050
	int32_t          Size;                 // 0x0058
};

class UClass : public UStruct
{
public:
	char pad[0x208 - sizeof(UStruct)];
};

// ==================== TUObjectArray [Dumper-7 Basic.hpp] ====================
// FUObjectItem = 0x18 bytes; object pointer at item+0x8 (8-byte pad first).
class TUObjectArray {
public:
	BYTE** Objects;            // 0x00 chunk table
	BYTE*  PreAllocatedObjects;// 0x08
	int32_t MaxElements;       // 0x10
	int32_t NumElements;       // 0x14
	int32_t MaxChunks;         // 0x18
	int32_t NumChunks;         // 0x1C

	UObject* GetObjectPtr(uint32_t id) const;
	UObject* FindObject(const char* name) const;             // exact GetFullName match
	UObject* FindObjectByString(const char* name);           // substring, first match
	std::vector<UObject*> FindObjectsByString(const char* name);
};

// Resolve FWeakObjectPtr -> UObject* via GObjects (index only; serial ignored)
class UObject* WeakObjResolve(const FWeakObjectPtr& weak);

// ==================== Camera / W2S (verified formula, external build) ====================
struct CameraView
{
	FVector  location;
	FRotator rotation;
	float    fov = 90.f;
	double   screen_w = 1920.0;
	double   screen_h = 1080.0;

	// Axes from FRotationTranslationMatrix:
	//   forward = (CP*CY, CP*SY, SP)
	//   right   = (SR*SP*CY - CR*SY, SR*SP*SY + CR*CY, -SR*CP)
	//   up      = (-CR*SP*CY - SR*SY, -CR*SP*SY + SR*CY, CR*CP)
	// Both axes scale by (screenW/2) / tan(fov/2).
	bool world_to_screen(const FVector& world, FVector2D& screen) const
	{
		double pr = rotation.pitch * (M_PI / 180.0);
		double yr = rotation.yaw   * (M_PI / 180.0);
		double rr = rotation.roll  * (M_PI / 180.0);
		double cp = cos(pr), sp = sin(pr);
		double cy = cos(yr), sy = sin(yr);
		double cr = cos(rr), sr = sin(rr);

		FVector forward = { cp * cy, cp * sy, sp };
		FVector right   = { sr * sp * cy - cr * sy, sr * sp * sy + cr * cy, -sr * cp };
		FVector up      = { -cr * sp * cy - sr * sy, -cr * sp * sy + sr * cy, cr * cp };

		FVector delta = world - location;
		double dot_forward = delta.dot(forward);
		if (dot_forward <= 0.1) return false;

		double scale = (screen_w * 0.5) / tan(fov * (M_PI / 360.0));
		screen.x = screen_w * 0.5 + (delta.dot(right) / dot_forward) * scale;
		screen.y = screen_h * 0.5 - (delta.dot(up)    / dot_forward) * scale;
		return (screen.x >= -50 && screen.x <= screen_w + 50 &&
		        screen.y >= -50 && screen.y <= screen_h + 50);
	}

	// No screen-margin culling; behind-camera points projected at near depth.
	bool project_unclamped(const FVector& world, FVector2D& screen, bool clamp_behind = false) const
	{
		double pr = rotation.pitch * (M_PI / 180.0);
		double yr = rotation.yaw   * (M_PI / 180.0);
		double rr = rotation.roll  * (M_PI / 180.0);
		double cp = cos(pr), sp = sin(pr);
		double cy = cos(yr), sy = sin(yr);
		double cr = cos(rr), sr = sin(rr);

		FVector forward = { cp * cy, cp * sy, sp };
		FVector right   = { sr * sp * cy - cr * sy, sr * sp * sy + cr * cy, -sr * cp };
		FVector up      = { -cr * sp * cy - sr * sy, -cr * sp * sy + sr * cy, cr * cp };

		FVector delta = world - location;
		double dot_forward = delta.dot(forward);
		if (dot_forward <= 0.1) {
			if (!clamp_behind) return false;
			dot_forward = 0.1;
		}
		double scale = (screen_w * 0.5) / tan(fov * (M_PI / 360.0));
		double px = screen_w * 0.5 + (delta.dot(right) / dot_forward) * scale;
		double py = screen_h * 0.5 - (delta.dot(up)    / dot_forward) * scale;
		if (px > 100000.0) px = 100000.0; else if (px < -100000.0) px = -100000.0;
		if (py > 100000.0) py = 100000.0; else if (py < -100000.0) py = -100000.0;
		screen.x = px; screen.y = py;
		return true;
	}

	// Line segment projection with NEAR-PLANE 3D CLIPPING.
	// If one endpoint is behind the camera, the segment is clipped at the near
	// plane and only the visible part is projected - no fake far-point streaks.
	// Returns false when the whole segment is behind the near plane.
	bool project_line(const FVector& p1, const FVector& p2, FVector2D& s1, FVector2D& s2) const
	{
		double pr = rotation.pitch * (M_PI / 180.0);
		double yr = rotation.yaw   * (M_PI / 180.0);
		double rr = rotation.roll  * (M_PI / 180.0);
		double cp = cos(pr), sp = sin(pr);
		double cy = cos(yr), sy = sin(yr);
		double cr = cos(rr), sr = sin(rr);

		FVector forward = { cp * cy, cp * sy, sp };
		FVector right   = { sr * sp * cy - cr * sy, sr * sp * sy + cr * cy, -sr * cp };
		FVector up      = { -cr * sp * cy - sr * sy, -cr * sp * sy + sr * cy, cr * cp };

		const double near_z = 10.0;   // 10 cm
		FVector d1 = p1 - location;
		FVector d2 = p2 - location;
		double z1 = d1.dot(forward);
		double z2 = d2.dot(forward);
		if (z1 < near_z && z2 < near_z) return false;

		FVector a = p1, b = p2;
		FVector da = d1, db = d2;
		double za = z1, zb = z2;
		if (z1 < near_z) {
			double t = (near_z - z1) / (z2 - z1);
			a = p1 + (p2 - p1) * t;
			da = a - location;
			za = near_z;
		} else if (z2 < near_z) {
			double t = (near_z - z2) / (z1 - z2);
			b = p2 + (p1 - p2) * t;
			db = b - location;
			zb = near_z;
		}

		double scale = (screen_w * 0.5) / tan(fov * (M_PI / 360.0));
		s1.x = screen_w * 0.5 + (da.dot(right) / za) * scale;
		s1.y = screen_h * 0.5 - (da.dot(up)    / za) * scale;
		s2.x = screen_w * 0.5 + (db.dot(right) / zb) * scale;
		s2.y = screen_h * 0.5 - (db.dot(up)    / zb) * scale;
		return true;
	}
};

// Manual FindLookAtRotation (UE convention: pitch positive = up, matches w2s above)
inline FRotator look_at_rotation(const FVector& from, const FVector& to)
{
	FVector d = to - from;
	double len2d = sqrt(d.x * d.x + d.y * d.y);
	FRotator r;
	r.yaw   = atan2(d.y, d.x) * (180.0 / M_PI);
	r.pitch = atan2(d.z, len2d) * (180.0 / M_PI);
	r.roll  = 0.0;
	return r;
}

// ==================== angle helpers (UE FRotator semantics) ====================
// wrap to (-180, 180]
inline double clamp_angle(double a) {
	while (a > 180.0) a -= 360.0;
	while (a < -180.0) a += 360.0;
	return a;
}

inline double clamp_pitch(double p) {
	return p > 89.9 ? 89.9 : (p < -89.9 ? -89.9 : p);
}

// UE GetNormalized: pitch pole-flip (pitch>90 => pitch=180-pitch, yaw+=180)
inline FRotator normalize_rot(const FRotator& r) {
	FRotator n;
	n.pitch = clamp_angle(r.pitch);
	n.yaw   = clamp_angle(r.yaw);
	n.roll  = 0.0;
	if (n.pitch > 90.0)  { n.pitch = 180.0 - n.pitch; n.yaw = clamp_angle(n.yaw + 180.0); }
	if (n.pitch < -90.0) { n.pitch = -180.0 - n.pitch; n.yaw = clamp_angle(n.yaw + 180.0); }
	n.pitch = clamp_pitch(n.pitch);
	return n;
}

inline FVector rotator_forward(const FRotator& r)
{
	double pr = r.pitch * (M_PI / 180.0), yr = r.yaw * (M_PI / 180.0);
	double cp = cos(pr), sp = sin(pr), cy = cos(yr), sy = sin(yr);
	return { cp * cy, cp * sy, sp };
}
