#pragma once
#include "Driver.h"
#include "Offsets.h"

#include <iostream>
#include <thread>
#include <map>
#include <vector>
#include <string>
#include <unordered_map>
#include <chrono>
#include <Xinput.h>
#include <dinput.h>
#pragma comment(lib, "Xinput.lib")
#pragma comment(lib, "dinput8.lib")
#pragma comment(lib, "dxguid.lib")

// Simple cache with 1-second TTL
template<typename T>
class CacheEntry {
public:
	T value;
	std::chrono::steady_clock::time_point timestamp;
	
	CacheEntry() : timestamp() {}
	CacheEntry(T val) : value(val), timestamp(std::chrono::steady_clock::now()) {}
	
	bool IsExpired(int ttlMs = 1000) const {
		auto now = std::chrono::steady_clock::now();
		auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - timestamp).count();
		return elapsed > ttlMs;
	}
};

template<typename T>
class AddressCache {
private:
	std::unordered_map<uintptr_t, CacheEntry<T>> cache;
	
public:
	T Get(uintptr_t address, int ttlMs = 1000) {
		auto it = cache.find(address);
		if (it != cache.end() && !it->second.IsExpired(ttlMs)) {
			return it->second.value;
		}
		return T(); // Return default if not cached or expired
	}
	
	void Set(uintptr_t address, T value) {
		cache[address] = CacheEntry<T>(value);
	}
	
	void Clear() {
		cache.clear();
	}
	
	void ClearExpired(int ttlMs = 1000) {
		auto now = std::chrono::steady_clock::now();
		for (auto it = cache.begin(); it != cache.end(); ) {
			if (it->second.IsExpired(ttlMs)) {
				it = cache.erase(it);
			} else {
				++it;
			}
		}
	}
};

#define Assert( _exp ) ((void)0)

#define FastSqrt(x)			(sqrt)(x)

#define M_PI		3.14159265358979323846	// matches value in gcc v2 math.h

#define M_PI_F		((float)(M_PI))	// Shouldn't collide with anything.

#define M_PHI		1.61803398874989484820 // golden ratio

// NJS: Inlined to prevent floats from being autopromoted to doubles, as with the old system.
#ifndef RAD2DEG
#define RAD2DEG( x  )  ( (float)(x) * (float)(180.f / M_PI_F) )
#endif

#ifndef DEG2RAD
#define DEG2RAD( x  )  ( (float)(x) * (float)(M_PI_F / 180.f) )
#endif

// decls for aligning data

#define DECL_ALIGN(x) __attribute__((aligned(x)))


#define ALIGN16 DECL_ALIGN(16)
#define VALVE_RAND_MAX 0x7fff
#define VectorExpand(v) (v).x, (v).y, (v).z

class VMatrix
{
public:

	VMatrix() {}
	VMatrix(
		float m00, float m01, float m02, float m03,
		float m10, float m11, float m12, float m13,
		float m20, float m21, float m22, float m23,
		float m30, float m31, float m32, float m33
	) {
		m[0][0] = m00;
		m[0][1] = m01;
		m[0][2] = m02;
		m[0][3] = m03;

		m[1][0] = m10;
		m[1][1] = m11;
		m[1][2] = m12;
		m[1][3] = m13;

		m[2][0] = m20;
		m[2][1] = m21;
		m[2][2] = m22;
		m[2][3] = m23;

		m[3][0] = m30;
		m[3][1] = m31;
		m[3][2] = m32;
		m[3][3] = m33;
	}



	// array access
	inline float* operator[](int i)
	{
		return m[i];
	}

	inline const float* operator[](int i) const
	{
		return m[i];
	}

	// Get a pointer to m[0][0]
	inline float* Base()
	{
		return &m[0][0];
	}

	inline const float* Base() const
	{
		return &m[0][0];
	}


public:
	// The matrix.
	float		m[4][4];
};

class Vector
{
public:
	float x, y, z;
	Vector(void);
	Vector(float X, float Y, float Z);
	void Init(float ix = 0.0f, float iy = 0.0f, float iz = 0.0f);
	bool IsValid() const;
	float operator[](int i) const;
	float& operator[](int i);
	inline void Zero();
	bool operator==(const Vector& v) const;
	bool operator!=(const Vector& v) const;
	inline Vector& operator+=(const Vector& v);
	inline Vector& operator-=(const Vector& v);
	inline Vector& operator*=(const Vector& v);
	inline Vector& operator*=(float s);
	inline Vector& operator/=(const Vector& v);
	inline Vector& operator/=(float s);
	inline Vector& operator+=(float fl);
	inline Vector& operator-=(float fl);
	inline float	Length() const;
	inline float LengthSqr(void) const
	{
		return (x * x + y * y + z * z);
	}
	bool IsZero(float tolerance = 0.01f) const
	{
		return (x > -tolerance && x < tolerance &&
			y > -tolerance && y < tolerance &&
			z > -tolerance && z < tolerance);
	}
	Vector	Normalize();
	void	NormalizeInPlace();
	inline float	DistTo(const Vector& vOther) const;
	inline float	DistToSqr(const Vector& vOther) const;
	float	Dot(const Vector& vOther) const;
	float	Length2D(void) const;
	float	Length2DSqr(void) const;
	Vector& operator=(const Vector& vOther);
	Vector	operator-(void) const;
	Vector	operator+(const Vector& v) const;
	Vector	operator-(const Vector& v) const;
	Vector	operator*(const Vector& v) const;
	Vector	operator/(const Vector& v) const;
	Vector	operator*(float fl) const;
	Vector	operator/(float fl) const;
	// Base address...
	float* Base();
	float const* Base() const;
};

//===============================================
inline void Vector::Init(float ix, float iy, float iz)
{
	x = ix; y = iy; z = iz;
}
//===============================================
inline Vector::Vector(float X, float Y, float Z)
{
	x = X; y = Y; z = Z;
}
//===============================================
inline Vector::Vector(void) { Zero(); }
//===============================================
inline void Vector::Zero()
{
	x = y = z = 0.0f;
}
//===============================================
inline void VectorClear(Vector& a)
{
	a.x = a.y = a.z = 0.0f;
}
//===============================================
inline Vector& Vector::operator=(const Vector& vOther)
{
	x = vOther.x; y = vOther.y; z = vOther.z;
	return *this;
}
//===============================================
inline float& Vector::operator[](int i)
{
	Assert((i >= 0) && (i < 3));
	return ((float*)this)[i];
}
//===============================================
inline float Vector::operator[](int i) const
{
	Assert((i >= 0) && (i < 3));
	return ((float*)this)[i];
}
//===============================================
inline bool Vector::operator==(const Vector& src) const
{
	return (src.x == x) && (src.y == y) && (src.z == z);
}
//===============================================
inline bool Vector::operator!=(const Vector& src) const
{
	return (src.x != x) || (src.y != y) || (src.z != z);
}
//===============================================
inline void VectorCopy(const Vector& src, Vector& dst)
{
	dst.x = src.x;
	dst.y = src.y;
	dst.z = src.z;
}
//===============================================
inline  Vector& Vector::operator+=(const Vector& v)
{
	x += v.x; y += v.y; z += v.z;
	return *this;
}
//===============================================
inline  Vector& Vector::operator-=(const Vector& v)
{
	x -= v.x; y -= v.y; z -= v.z;
	return *this;
}
//===============================================
inline  Vector& Vector::operator*=(float fl)
{
	x *= fl;
	y *= fl;
	z *= fl;

	return *this;
}
//===============================================
inline  Vector& Vector::operator*=(const Vector& v)
{
	x *= v.x;
	y *= v.y;
	z *= v.z;

	return *this;
}
//===============================================
inline Vector& Vector::operator+=(float fl)
{
	x += fl;
	y += fl;
	z += fl;

	return *this;
}
//===============================================
inline Vector& Vector::operator-=(float fl)
{
	x -= fl;
	y -= fl;
	z -= fl;

	return *this;
}
//===============================================
inline  Vector& Vector::operator/=(float fl)
{
	Assert(fl != 0.0f);
	float oofl = 1.0f / fl;
	x *= oofl;
	y *= oofl;
	z *= oofl;

	return *this;
}
//===============================================
inline  Vector& Vector::operator/=(const Vector& v)
{
	Assert(v.x != 0.0f && v.y != 0.0f && v.z != 0.0f);
	x /= v.x;
	y /= v.y;
	z /= v.z;

	return *this;
}
//===============================================
inline float Vector::Length(void) const
{


	float root = 0.0f;

	float sqsr = x * x + y * y + z * z;

	root = sqrt(sqsr);

	return root;
}
//===============================================
inline float Vector::Length2D(void) const
{
	float root = 0.0f;

	float sqst = x * x + y * y;

	root = sqrt(sqst);

	return root;
}
//===============================================
inline float Vector::Length2DSqr(void) const
{
	return (x * x + y * y);
}
//===============================================
inline Vector CrossProduct(const Vector& a, const Vector& b)
{
	return Vector(a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x);
}
//===============================================

float Vector::DistTo(const Vector& vOther) const
{
	Vector delta;

	delta.x = x - vOther.x;
	delta.y = y - vOther.y;
	delta.z = z - vOther.z;

	return delta.Length();
}
float Vector::DistToSqr(const Vector& vOther) const
{
	Vector delta;

	delta.x = x - vOther.x;
	delta.y = y - vOther.y;
	delta.z = z - vOther.z;

	return delta.LengthSqr();
}
//===============================================
inline Vector Vector::Normalize()
{
	Vector vector;
	float length = this->Length();

	if (length != 0)
	{
		vector.x = x / length;
		vector.y = y / length;
		vector.z = z / length;
	}
	else
	{
		vector.x = vector.y = 0.0f; vector.z = 1.0f;
	}

	return vector;
}
//===============================================
inline void Vector::NormalizeInPlace()
{
	Vector& v = *this;

	float iradius = 1.f / (this->Length() + 1.192092896e-07F); //FLT_EPSILON

	v.x *= iradius;
	v.y *= iradius;
	v.z *= iradius;
}
//===============================================
inline float VectorNormalize(Vector& v)
{
	Assert(v.IsValid());
	float l = v.Length();
	if (l != 0.0f)
	{
		v /= l;
	}
	else
	{
		// FIXME:
		// Just copying the existing implemenation; shouldn't res.z == 0?
		v.x = v.y = 0.0f; v.z = 1.0f;
	}
	return l;
}
//===============================================
inline float VectorNormalize(float* v)
{
	return VectorNormalize(*(reinterpret_cast<Vector*>(v)));
}
//===============================================
inline Vector Vector::operator+(const Vector& v) const
{
	Vector res;
	res.x = x + v.x;
	res.y = y + v.y;
	res.z = z + v.z;
	return res;
}

//===============================================
inline Vector Vector::operator-(const Vector& v) const
{
	Vector res;
	res.x = x - v.x;
	res.y = y - v.y;
	res.z = z - v.z;
	return res;
}
//===============================================
inline Vector Vector::operator*(float fl) const
{
	Vector res;
	res.x = x * fl;
	res.y = y * fl;
	res.z = z * fl;
	return res;
}
//===============================================
inline Vector Vector::operator*(const Vector& v) const
{
	Vector res;
	res.x = x * v.x;
	res.y = y * v.y;
	res.z = z * v.z;
	return res;
}
//===============================================
inline Vector Vector::operator/(float fl) const
{
	Vector res;
	res.x = x / fl;
	res.y = y / fl;
	res.z = z / fl;
	return res;
}
//===============================================
inline Vector Vector::operator/(const Vector& v) const
{
	Vector res;
	res.x = x / v.x;
	res.y = y / v.y;
	res.z = z / v.z;
	return res;
}
inline float Vector::Dot(const Vector& vOther) const
{
	const Vector& a = *this;

	return(a.x * vOther.x + a.y * vOther.y + a.z * vOther.z);
}

//-----------------------------------------------------------------------------
// length
//-----------------------------------------------------------------------------

inline float VectorLength(const Vector& v)
{

	return (float)FastSqrt(v.x * v.x + v.y * v.y + v.z * v.z);
}

//VECTOR SUBTRAC
inline void VectorSubtract(const Vector& a, const Vector& b, Vector& c)
{


	c.x = a.x - b.x;
	c.y = a.y - b.y;
	c.z = a.z - b.z;
}

//VECTORADD
inline void VectorAdd(const Vector& a, const Vector& b, Vector& c)
{


	c.x = a.x + b.x;
	c.y = a.y + b.y;
	c.z = a.z + b.z;
}

//-----------------------------------------------------------------------------
// Base address...
//-----------------------------------------------------------------------------
inline float* Vector::Base()
{
	return (float*)this;
}

inline float const* Vector::Base() const
{
	return (float const*)this;
}

inline void VectorMAInline(const float* start, float scale, const float* direction, float* dest)
{
	dest[0] = start[0] + direction[0] * scale;
	dest[1] = start[1] + direction[1] * scale;
	dest[2] = start[2] + direction[2] * scale;
}

inline void VectorMAInline(const Vector& start, float scale, const Vector& direction, Vector& dest)
{
	dest.x = start.x + direction.x * scale;
	dest.y = start.y + direction.y * scale;
	dest.z = start.z + direction.z * scale;
}

inline void VectorMA(const Vector& start, float scale, const Vector& direction, Vector& dest)
{
	VectorMAInline(start, scale, direction, dest);
}

inline void VectorMA(const float* start, float scale, const float* direction, float* dest)
{
	VectorMAInline(start, scale, direction, dest);
}


inline unsigned long& FloatBits(float& f)
{
	return *reinterpret_cast<unsigned long*>(&f);
}

inline bool IsFinite(float f)
{
	return ((FloatBits(f) & 0x7F800000) != 0x7F800000);
}

//=========================================================
// 2D Vector2D
//=========================================================

class Vector2D
{
public:
	// Members
	float x, y;

	// Construction/destruction
	Vector2D(void);
	Vector2D(float X, float Y);
	Vector2D(const float* pFloat);

	// Initialization
	void Init(float ix = 0.0f, float iy = 0.0f);

	// Got any nasty NAN's?
	bool IsValid() const;

	// array access...
	float operator[](int i) const;
	float& operator[](int i);

	// Base address...
	float* Base();
	float const* Base() const;

	// Initialization methods
	void Random(float minVal, float maxVal);

	// equality
	bool operator==(const Vector2D& v) const;
	bool operator!=(const Vector2D& v) const;

	// arithmetic operations
	Vector2D& operator+=(const Vector2D& v);
	Vector2D& operator-=(const Vector2D& v);
	Vector2D& operator*=(const Vector2D& v);
	Vector2D& operator*=(float s);
	Vector2D& operator/=(const Vector2D& v);
	Vector2D& operator/=(float s);

	// negate the Vector2D components
	void	Negate();

	// Get the Vector2D's magnitude.
	float	Length() const;

	// Get the Vector2D's magnitude squared.
	float	LengthSqr(void) const;

	// return true if this vector is (0,0) within tolerance
	bool IsZero(float tolerance = 0.01f) const
	{
		return (x > -tolerance && x < tolerance &&
			y > -tolerance && y < tolerance);
	}

	float	Normalize();

	// Normalize in place and return the old length.
	float	NormalizeInPlace();

	// Compare length.
	bool	IsLengthGreaterThan(float val) const;
	bool	IsLengthLessThan(float val) const;

	// Get the distance from this Vector2D to the other one.
	float	DistTo(const Vector2D& vOther) const;

	// Get the distance from this Vector2D to the other one squared.
	float	DistToSqr(const Vector2D& vOther) const;

	// Copy
	void	CopyToArray(float* rgfl) const;

	// Multiply, add, and assign to this (ie: *this = a + b * scalar). This
	// is about 12% faster than the actual Vector2D equation (because it's done per-component
	// rather than per-Vector2D).
	void	MulAdd(const Vector2D& a, const Vector2D& b, float scalar);

	// Dot product.
	float	Dot(const Vector2D& vOther) const;

	// assignment
	Vector2D& operator=(const Vector2D& vOther);

#ifndef VECTOR_NO_SLOW_OPERATIONS
	// copy constructors
	Vector2D(const Vector2D& vOther);

	// arithmetic operations
	Vector2D	operator-(void) const;

	Vector2D	operator+(const Vector2D& v) const;
	Vector2D	operator-(const Vector2D& v) const;
	Vector2D	operator*(const Vector2D& v) const;
	Vector2D	operator/(const Vector2D& v) const;
	Vector2D	operator*(float fl) const;
	Vector2D	operator/(float fl) const;

	// Cross product between two vectors.
	Vector2D	Cross(const Vector2D& vOther) const;

	// Returns a Vector2D with the min or max in X, Y, and Z.
	Vector2D	Min(const Vector2D& vOther) const;
	Vector2D	Max(const Vector2D& vOther) const;

#else

private:
	// No copy constructors allowed if we're in optimal mode
	Vector2D(const Vector2D& vOther);
#endif
};

//-----------------------------------------------------------------------------

const Vector2D vec2_origin(0, 0);
//const Vector2D vec2_invalid(3.40282347E+38F, 3.40282347E+38F);

//-----------------------------------------------------------------------------
// Vector2D related operations
//-----------------------------------------------------------------------------

// Vector2D clear
void Vector2DClear(Vector2D& a);

// Copy
void Vector2DCopy(const Vector2D& src, Vector2D& dst);

// Vector2D arithmetic
void Vector2DAdd(const Vector2D& a, const Vector2D& b, Vector2D& result);
void Vector2DSubtract(const Vector2D& a, const Vector2D& b, Vector2D& result);
void Vector2DMultiply(const Vector2D& a, float b, Vector2D& result);
void Vector2DMultiply(const Vector2D& a, const Vector2D& b, Vector2D& result);
void Vector2DDivide(const Vector2D& a, float b, Vector2D& result);
void Vector2DDivide(const Vector2D& a, const Vector2D& b, Vector2D& result);
void Vector2DMA(const Vector2D& start, float s, const Vector2D& dir, Vector2D& result);

// Store the min or max of each of x, y, and z into the result.
void Vector2DMin(const Vector2D& a, const Vector2D& b, Vector2D& result);
void Vector2DMax(const Vector2D& a, const Vector2D& b, Vector2D& result);

#define Vector2DExpand( v ) (v).x, (v).y

// Normalization
float Vector2DNormalize(Vector2D& v);

// Length
float Vector2DLength(const Vector2D& v);

// Dot Product
float DotProduct2D(const Vector2D& a, const Vector2D& b);

// Linearly interpolate between two vectors
void Vector2DLerp(const Vector2D& src1, const Vector2D& src2, float t, Vector2D& dest);


//-----------------------------------------------------------------------------
//
// Inlined Vector2D methods
//
//-----------------------------------------------------------------------------


//-----------------------------------------------------------------------------
// constructors
//-----------------------------------------------------------------------------

inline Vector2D::Vector2D(void)
{
#ifdef _DEBUG
	// Initialize to NAN to catch errors
	//x = y = float_NAN;
#endif
}

inline Vector2D::Vector2D(float X, float Y)
{
	x = X; y = Y;
	Assert(IsValid());
}

inline Vector2D::Vector2D(const float* pFloat)
{
	Assert(pFloat);
	x = pFloat[0]; y = pFloat[1];
	Assert(IsValid());
}


//-----------------------------------------------------------------------------
// copy constructor
//-----------------------------------------------------------------------------

inline Vector2D::Vector2D(const Vector2D& vOther)
{
	Assert(vOther.IsValid());
	x = vOther.x; y = vOther.y;
}

//-----------------------------------------------------------------------------
// initialization
//-----------------------------------------------------------------------------

inline void Vector2D::Init(float ix, float iy)
{
	x = ix; y = iy;
	Assert(IsValid());
}

inline void Vector2D::Random(float minVal, float maxVal)
{
	x = minVal + ((float)rand() / VALVE_RAND_MAX) * (maxVal - minVal);
	y = minVal + ((float)rand() / VALVE_RAND_MAX) * (maxVal - minVal);
}

inline void Vector2DClear(Vector2D& a)
{
	a.x = a.y = 0.0f;
}

//-----------------------------------------------------------------------------
// assignment
//-----------------------------------------------------------------------------

inline Vector2D& Vector2D::operator=(const Vector2D& vOther)
{
	Assert(vOther.IsValid());
	x = vOther.x; y = vOther.y;
	return *this;
}

//-----------------------------------------------------------------------------
// Array access
//-----------------------------------------------------------------------------

inline float& Vector2D::operator[](int i)
{
	Assert((i >= 0) && (i < 2));
	return ((float*)this)[i];
}

inline float Vector2D::operator[](int i) const
{
	Assert((i >= 0) && (i < 2));
	return ((float*)this)[i];
}

//-----------------------------------------------------------------------------
// Base address...
//-----------------------------------------------------------------------------

inline float* Vector2D::Base()
{
	return (float*)this;
}

inline float const* Vector2D::Base() const
{
	return (float const*)this;
}

//-----------------------------------------------------------------------------
// IsValid?
//-----------------------------------------------------------------------------

inline bool Vector2D::IsValid() const
{
	return IsFinite(x) && IsFinite(y);
}

//-----------------------------------------------------------------------------
// comparison
//-----------------------------------------------------------------------------

inline bool Vector2D::operator==(const Vector2D& src) const
{
	Assert(src.IsValid() && IsValid());
	return (src.x == x) && (src.y == y);
}

inline bool Vector2D::operator!=(const Vector2D& src) const
{
	Assert(src.IsValid() && IsValid());
	return (src.x != x) || (src.y != y);
}


//-----------------------------------------------------------------------------
// Copy
//-----------------------------------------------------------------------------

inline void Vector2DCopy(const Vector2D& src, Vector2D& dst)
{
	Assert(src.IsValid());
	dst.x = src.x;
	dst.y = src.y;
}

inline void	Vector2D::CopyToArray(float* rgfl) const
{
	Assert(IsValid());
	Assert(rgfl);
	rgfl[0] = x; rgfl[1] = y;
}

//-----------------------------------------------------------------------------
// standard math operations
//-----------------------------------------------------------------------------

inline void Vector2D::Negate()
{
	Assert(IsValid());
	x = -x; y = -y;
}

inline Vector2D& Vector2D::operator+=(const Vector2D& v)
{
	Assert(IsValid() && v.IsValid());
	x += v.x; y += v.y;
	return *this;
}

inline Vector2D& Vector2D::operator-=(const Vector2D& v)
{
	Assert(IsValid() && v.IsValid());
	x -= v.x; y -= v.y;
	return *this;
}

inline Vector2D& Vector2D::operator*=(float fl)
{
	x *= fl;
	y *= fl;
	Assert(IsValid());
	return *this;
}

inline Vector2D& Vector2D::operator*=(const Vector2D& v)
{
	x *= v.x;
	y *= v.y;
	Assert(IsValid());
	return *this;
}

inline Vector2D& Vector2D::operator/=(float fl)
{
	Assert(fl != 0.0f);
	float oofl = 1.0f / fl;
	x *= oofl;
	y *= oofl;
	Assert(IsValid());
	return *this;
}

inline Vector2D& Vector2D::operator/=(const Vector2D& v)
{
	Assert(v.x != 0.0f && v.y != 0.0f);
	x /= v.x;
	y /= v.y;
	Assert(IsValid());
	return *this;
}

inline void Vector2DAdd(const Vector2D& a, const Vector2D& b, Vector2D& c)
{
	Assert(a.IsValid() && b.IsValid());
	c.x = a.x + b.x;
	c.y = a.y + b.y;
}

inline void Vector2DSubtract(const Vector2D& a, const Vector2D& b, Vector2D& c)
{
	Assert(a.IsValid() && b.IsValid());
	c.x = a.x - b.x;
	c.y = a.y - b.y;
}

inline void Vector2DMultiply(const Vector2D& a, float b, Vector2D& c)
{
	Assert(a.IsValid() && IsFinite(b));
	c.x = a.x * b;
	c.y = a.y * b;
}

inline void Vector2DMultiply(const Vector2D& a, const Vector2D& b, Vector2D& c)
{
	Assert(a.IsValid() && b.IsValid());
	c.x = a.x * b.x;
	c.y = a.y * b.y;
}


inline void Vector2DDivide(const Vector2D& a, float b, Vector2D& c)
{
	Assert(a.IsValid());
	Assert(b != 0.0f);
	float oob = 1.0f / b;
	c.x = a.x * oob;
	c.y = a.y * oob;
}

inline void Vector2DDivide(const Vector2D& a, const Vector2D& b, Vector2D& c)
{
	Assert(a.IsValid());
	Assert((b.x != 0.0f) && (b.y != 0.0f));
	c.x = a.x / b.x;
	c.y = a.y / b.y;
}

inline void Vector2DMA(const Vector2D& start, float s, const Vector2D& dir, Vector2D& result)
{
	Assert(start.IsValid() && IsFinite(s) && dir.IsValid());
	result.x = start.x + s * dir.x;
	result.y = start.y + s * dir.y;
}

// FIXME: Remove
// For backwards compatability
inline void	Vector2D::MulAdd(const Vector2D& a, const Vector2D& b, float scalar)
{
	x = a.x + b.x * scalar;
	y = a.y + b.y * scalar;
}

inline void Vector2DLerp(const Vector2D& src1, const Vector2D& src2, float t, Vector2D& dest)
{
	dest[0] = src1[0] + (src2[0] - src1[0]) * t;
	dest[1] = src1[1] + (src2[1] - src1[1]) * t;
}

//-----------------------------------------------------------------------------
// dot, cross
//-----------------------------------------------------------------------------
inline float DotProduct2D(const Vector2D& a, const Vector2D& b)
{
	Assert(a.IsValid() && b.IsValid());
	return(a.x * b.x + a.y * b.y);
}

// for backwards compatability
inline float Vector2D::Dot(const Vector2D& vOther) const
{
	return DotProduct2D(*this, vOther);
}


//-----------------------------------------------------------------------------
// length
//-----------------------------------------------------------------------------
inline float Vector2DLength(const Vector2D& v)
{
	Assert(v.IsValid());
	return (float)FastSqrt(v.x * v.x + v.y * v.y);
}

inline float Vector2D::LengthSqr(void) const
{
	Assert(IsValid());
	return (x * x + y * y);
}

inline float Vector2D::NormalizeInPlace()
{
	return Vector2DNormalize(*this);
}

inline bool Vector2D::IsLengthGreaterThan(float val) const
{
	return LengthSqr() > val * val;
}

inline bool Vector2D::IsLengthLessThan(float val) const
{
	return LengthSqr() < val * val;
}

inline float Vector2D::Length(void) const
{
	return Vector2DLength(*this);
}


inline void Vector2DMin(const Vector2D& a, const Vector2D& b, Vector2D& result)
{
	result.x = (a.x < b.x) ? a.x : b.x;
	result.y = (a.y < b.y) ? a.y : b.y;
}


inline void Vector2DMax(const Vector2D& a, const Vector2D& b, Vector2D& result)
{
	result.x = (a.x > b.x) ? a.x : b.x;
	result.y = (a.y > b.y) ? a.y : b.y;
}


//-----------------------------------------------------------------------------
// Normalization
//-----------------------------------------------------------------------------
inline float Vector2DNormalize(Vector2D& v)
{
	Assert(v.IsValid());
	float l = v.Length();
	if (l != 0.0f)
	{
		v /= l;
	}
	else
	{
		v.x = v.y = 0.0f;
	}
	return l;
}


//-----------------------------------------------------------------------------
// Get the distance from this Vector2D to the other one
//-----------------------------------------------------------------------------
inline float Vector2D::DistTo(const Vector2D& vOther) const
{
	Vector2D delta;
	Vector2DSubtract(*this, vOther, delta);
	return delta.Length();
}

inline float Vector2D::DistToSqr(const Vector2D& vOther) const
{
	Vector2D delta;
	Vector2DSubtract(*this, vOther, delta);
	return delta.LengthSqr();
}


//-----------------------------------------------------------------------------
// Computes the closest point to vecTarget no farther than flMaxDist from vecStart
//-----------------------------------------------------------------------------
inline void ComputeClosestPoint2D(const Vector2D& vecStart, float flMaxDist, const Vector2D& vecTarget, Vector2D* pResult)
{
	Vector2D vecDelta;
	Vector2DSubtract(vecTarget, vecStart, vecDelta);
	float flDistSqr = vecDelta.LengthSqr();
	if (flDistSqr <= flMaxDist * flMaxDist)
	{
		*pResult = vecTarget;
	}
	else
	{
		vecDelta /= FastSqrt(flDistSqr);
		Vector2DMA(vecStart, flMaxDist, vecDelta, *pResult);
	}
}



//-----------------------------------------------------------------------------
//
// Slow methods
//
//-----------------------------------------------------------------------------

#ifndef VECTOR_NO_SLOW_OPERATIONS
#endif
//-----------------------------------------------------------------------------
// Returns a Vector2D with the min or max in X, Y, and Z.
//-----------------------------------------------------------------------------

inline Vector2D Vector2D::Min(const Vector2D& vOther) const
{
	return Vector2D(x < vOther.x ? x : vOther.x,
		y < vOther.y ? y : vOther.y);
}

inline Vector2D Vector2D::Max(const Vector2D& vOther) const
{
	return Vector2D(x > vOther.x ? x : vOther.x,
		y > vOther.y ? y : vOther.y);
}


//-----------------------------------------------------------------------------
// arithmetic operations
//-----------------------------------------------------------------------------

inline Vector2D Vector2D::operator-(void) const
{
	return Vector2D(-x, -y);
}

inline Vector2D Vector2D::operator+(const Vector2D& v) const
{
	Vector2D res;
	Vector2DAdd(*this, v, res);
	return res;
}

inline Vector2D Vector2D::operator-(const Vector2D& v) const
{
	Vector2D res;
	Vector2DSubtract(*this, v, res);
	return res;
}

inline Vector2D Vector2D::operator*(float fl) const
{
	Vector2D res;
	Vector2DMultiply(*this, fl, res);
	return res;
}

inline Vector2D Vector2D::operator*(const Vector2D& v) const
{
	Vector2D res;
	Vector2DMultiply(*this, v, res);
	return res;
}

inline Vector2D Vector2D::operator/(float fl) const
{
	Vector2D res;
	Vector2DDivide(*this, fl, res);
	return res;
}

inline Vector2D Vector2D::operator/(const Vector2D& v) const
{
	Vector2D res;
	Vector2DDivide(*this, v, res);
	return res;
}

inline Vector2D operator*(float fl, const Vector2D& v)
{
	return v * fl;
}

class QAngleByValue;
class QAngle
{
public:
	// Members
	float x, y, z;

	void Normalize() {
		while (x > 89.f) x -= 180.f;
		while (x < -89.f) x += 180.f;
		while (y > 180.f) y -= 360.f;
		while (y < -180.f) y += 360.f;
		z = 0.f;
	}

	// Construction/destruction
	QAngle(void);
	QAngle(float X, float Y, float Z);
	//      QAngle(RadianEuler const &angles);      // evil auto type promotion!!!

	// Allow pass-by-value
	operator QAngleByValue& () { return *((QAngleByValue*)(this)); }
	operator const QAngleByValue& () const { return *((const QAngleByValue*)(this)); }

	// Initialization
	void Init(float ix = 0.0f, float iy = 0.0f, float iz = 0.0f);
	void Random(float minVal, float maxVal);

	// Got any nasty NAN's?
	bool IsValid() const;

	// array access...
	float operator[](int i) const;
	float& operator[](int i);

	// Base address...
	float* Base();
	float const* Base() const;

	// equality
	bool operator==(const QAngle& v) const;
	bool operator!=(const QAngle& v) const;

	bool IsZero(float tolerance = 0.01f) const
	{
		return (x > -tolerance && x < tolerance &&
			y > -tolerance && y < tolerance &&
			z > -tolerance && z < tolerance);
	}

	// arithmetic operations
	QAngle& operator+=(const QAngle& v);
	QAngle& operator-=(const QAngle& v);
	QAngle& operator*=(float s);
	QAngle& operator/=(float s);

	// Get the vector's magnitude.
	float   Length() const;
	float   LengthSqr() const;

	// negate the QAngle components
	//void  Negate();

	// No assignment operators either...
	QAngle& operator=(const QAngle& src);

#ifndef VECTOR_NO_SLOW_OPERATIONS
	// copy constructors

	// arithmetic operations
	QAngle  operator-(void) const;

	QAngle  operator+(const QAngle& v) const;
	QAngle  operator-(const QAngle& v) const;
	QAngle  operator*(float fl) const;
	QAngle  operator/(float fl) const;
#else

private:
	// No copy constructors allowed if we're in optimal mode
	QAngle(const QAngle& vOther);

#endif
};

//-----------------------------------------------------------------------------
// constructors
//-----------------------------------------------------------------------------
inline QAngle::QAngle(void)
{
#ifdef _DEBUG
#ifdef VECTOR_PARANOIA
	// Initialize to NAN to catch errors
	x = y = z = VEC_T_NAN;
#endif
#endif
}

inline QAngle::QAngle(float X, float Y, float Z)
{
	x = X; y = Y; z = Z;

}

//-----------------------------------------------------------------------------
// initialization
//-----------------------------------------------------------------------------
inline void QAngle::Init(float ix, float iy, float iz)
{
	x = ix; y = iy; z = iz;

}

inline void QAngle::Random(float minVal, float maxVal)
{
	x = minVal + ((float)rand() / RAND_MAX) * (maxVal - minVal);
	y = minVal + ((float)rand() / RAND_MAX) * (maxVal - minVal);
	z = minVal + ((float)rand() / RAND_MAX) * (maxVal - minVal);

}

//-----------------------------------------------------------------------------
// assignment
//-----------------------------------------------------------------------------
inline QAngle& QAngle::operator=(const QAngle& vOther)
{

	x = vOther.x; y = vOther.y; z = vOther.z;
	return *this;
}

//-----------------------------------------------------------------------------
// comparison
//-----------------------------------------------------------------------------
inline bool QAngle::operator==(const QAngle& src) const
{


	return (src.x == x) && (src.y == y) && (src.z == z);
}

inline bool QAngle::operator!=(const QAngle& src) const
{


	return (src.x != x) || (src.y != y) || (src.z != z);
}

//-----------------------------------------------------------------------------
// standard math operations
//-----------------------------------------------------------------------------
inline QAngle& QAngle::operator+=(const QAngle& v)
{


	x += v.x; y += v.y; z += v.z;
	return *this;
}

inline QAngle& QAngle::operator-=(const QAngle& v)
{


	x -= v.x; y -= v.y; z -= v.z;
	return *this;
}

inline QAngle& QAngle::operator*=(float fl)
{
	x *= fl;
	y *= fl;
	z *= fl;

	return *this;
}

inline QAngle& QAngle::operator/=(float fl)
{
	Assert(fl != 0.0f);
	float oofl = 1.0f / fl;
	x *= oofl;
	y *= oofl;
	z *= oofl;

	return *this;
}

//-----------------------------------------------------------------------------
// Base address...
//-----------------------------------------------------------------------------
inline float* QAngle::Base()
{
	return (float*)this;
}

inline float const* QAngle::Base() const
{
	return (float const*)this;
}

//-----------------------------------------------------------------------------
// Array access
//-----------------------------------------------------------------------------
inline float& QAngle::operator[](int i)
{
	Assert((i >= 0) && (i < 3));
	return ((float*)this)[i];
}

inline float QAngle::operator[](int i) const
{
	Assert((i >= 0) && (i < 3));
	return ((float*)this)[i];
}

//-----------------------------------------------------------------------------
// length
//-----------------------------------------------------------------------------
inline float QAngle::Length() const
{

	return (float)FastSqrt(LengthSqr());
}


inline float QAngle::LengthSqr() const
{

	return x * x + y * y + z * z;
}


//-----------------------------------------------------------------------------
// arithmetic operations (SLOW!!)
//-----------------------------------------------------------------------------
#ifndef VECTOR_NO_SLOW_OPERATIONS

inline QAngle QAngle::operator-(void) const
{
	return QAngle(-x, -y, -z);
}

inline QAngle QAngle::operator+(const QAngle& v) const
{
	QAngle res;
	res.x = x + v.x;
	res.y = y + v.y;
	res.z = z + v.z;
	return res;
}

inline QAngle QAngle::operator-(const QAngle& v) const
{
	QAngle res;
	res.x = x - v.x;
	res.y = y - v.y;
	res.z = z - v.z;
	return res;
}

inline QAngle QAngle::operator*(float fl) const
{
	QAngle res;
	res.x = x * fl;
	res.y = y * fl;
	res.z = z * fl;
	return res;
}

inline QAngle QAngle::operator/(float fl) const
{
	QAngle res;
	res.x = x / fl;
	res.y = y / fl;
	res.z = z / fl;
	return res;
}

inline QAngle operator*(float fl, const QAngle& v)
{
	return v * fl;
}

#endif // VECTOR_NO_SLOW_OPERATIONS


//QANGLE SUBTRAC
inline void QAngleSubtract(const QAngle& a, const QAngle& b, QAngle& c)
{


	c.x = a.x - b.x;
	c.y = a.y - b.y;
	c.z = a.z - b.z;
}

//QANGLEADD
inline void QAngleAdd(const QAngle& a, const QAngle& b, QAngle& c)
{


	c.x = a.x + b.x;
	c.y = a.y + b.y;
	c.z = a.z + b.z;
}

struct Vector2
{
	float x, y;
	Vector2 operator-(Vector2 v) const { return { x - v.x, y - v.y }; }
	Vector2 operator+(Vector2 v) const { return { x + v.x, y + v.y }; }
	Vector2 operator*(float s) const { return { x * s, y * s }; }
	Vector2 operator/(float s) const { return { x / s, y / s }; }
	Vector2& operator/=(float s) { x /= s; y /= s; return *this; }
	Vector2& operator+=(Vector2 v) { x += v.x; y += v.y; return *this; }
	Vector2& operator-=(Vector2 v) { x -= v.x; y -= v.y; return *this; }
	bool operator!=(Vector2 v) const { return x != v.x || y != v.y; }
	void Normalize() { if (x > 89.0f) x -= 180.f; if (x < -89.0f) x += 180.f; if (y > 180.f) y -= 360.f; if (y < -180.f) y += 360.f; }
	float Length2D() const { return sqrt(x * x + y * y); }
	float Dist2D(Vector2 v) const { return (*this - v).Length2D(); }
};



struct Bones2D {
	Vector2 Head;
	Vector2 Neck;
	Vector2 Chest;
	Vector2 Hip;

	Vector2 leftElbow, rightElbow;
	Vector2 leftHand, rightHand;
	Vector2 leftKnee, rightKnee;
	Vector2 leftHeel, rightHeel;
};

class Matrix3x4
{
public:
	union
	{
		struct
		{
			float _11, _12, _13, _14;
			float _21, _22, _23, _24;
			float _31, _32, _33, _34;
		};

		float m[3][4];
		float mm[12];
	};

	inline Vector& GetAxis(int i)
	{
		return *(Vector*)&m[i][0];
	}
};

struct Matrix {
	float matrix[16];
};

// Global cache instances - declared after Matrix is defined
extern AddressCache<Matrix> MatrixCache;
extern AddressCache<Vector> VectorCache;
extern AddressCache<int> IntCache;
extern AddressCache<uintptr_t> UintptrCache;

enum BoxStyle {
	Full = 0,
	Cornered = 1
};

struct Setting
{
	bool MenuOpen = true;

	bool Health = true;
	bool Shield = true;
	bool Name = true;
	bool Team = false;

	bool Skelton = true;

	bool ItemDist = true;
	bool ItemName = false;
	bool Box = true;
	bool Distance = true; // Add this line
	float MaxDistance = 150.0f; // Max range in meters
	BoxStyle BoxType = BoxStyle::Full; // Default to Full Box
	
	// Glow settings
	bool Glow = false;
	bool LootChams = false;
	
	// Aimbot settings
	bool Aimbot = false;
	float AimSmooth = 5.0f;
	float AimFOV = 150.0f;
	bool AimVisibleOnly = true;
	bool RecoilComp = true;  // Compensate for aimpunch (weapon recoil)
	bool SwayComp = true;    // Compensate for weapon sway
	
	// Controller settings
	bool ControllerEnabled = false;  // Enable controller support
	float ControllerSensitivity = 1.0f;  // Controller aim sensitivity
}Settings;

float ToMeters(float x)
{
	return x / 39.62f;
}

// Normalize angle to -180 to 180
float NormalizeAngle(float angle)
{
	while (angle > 180.0f) angle -= 360.0f;
	while (angle < -180.0f) angle += 360.0f;
	return angle;
}

// Normalize QAngle
inline void NormalizeQAngle(QAngle& angles) {
	while (angles.x > 89.f) angles.x -= 180.f;
	while (angles.x < -89.f) angles.x += 180.f;
	while (angles.y > 180.f) angles.y -= 360.f;
	while (angles.y < -180.f) angles.y += 360.f;
	angles.z = 0.f;
}

// Get FOV between two QAngles
inline float GetFov(const QAngle& viewAngle, const QAngle& aimAngle) {
	QAngle delta = aimAngle - viewAngle;
	NormalizeQAngle(delta);
	return sqrt(delta.x * delta.x + delta.y * delta.y);
}

// Normalize angles with pitch clamp (for FPS games)
void NormalizeAngles(Vector& angle)
{
	while (angle.x > 89.0f)
		angle.x -= 180.0f;

	while (angle.x < -89.0f)
		angle.x += 180.0f;

	while (angle.y > 180.0f)
		angle.y -= 360.0f;

	while (angle.y < -180.0f)
		angle.y += 360.0f;
}

// Aimbot angle calculation (Source engine style)
// Uses atan2f for correct yaw quadrant handling (avoids div-by-zero and wrong quadrant from atanf)
Vector CalcAngle(Vector src, Vector dst)
{
	Vector delta = dst - src;
	float hyp = sqrt(delta.x * delta.x + delta.y * delta.y);
	
	Vector angles;
	angles.x = atan2f(-delta.z, hyp) * (180.0f / M_PI); // Pitch: negative = up
	angles.y = atan2f(delta.y, delta.x) * (180.0f / M_PI); // Yaw: atan2f handles all quadrants correctly
	angles.z = 0.0f;
	
	// Normalize angles
	angles.x = NormalizeAngle(angles.x);
	angles.y = NormalizeAngle(angles.y);
		
	return angles;
}

// Calculate shortest angle difference (handles wrapping)
float AngleDiff(float a, float b)
{
	float diff = NormalizeAngle(a - b);
	if (diff > 180.0f) diff -= 360.0f;
	if (diff < -180.0f) diff += 360.0f;
	return diff;
}

// Highlight functions (Apex uses highlight system, not traditional glow)
void EnableHighlight(uintptr_t entityPtr)
{
	// Write highlight settings to enable glow effect
	Kernel.write<int>(entityPtr + OFFSET_HIGHLIGHT_GENERIC_CONTEXTS, 2); // Enable highlight
	Kernel.write<int>(entityPtr + OFFSET_HIGHLIGHT_FOCUSED, 1); // Set as focused
}

void DisableHighlight(uintptr_t entityPtr)
{
	// Disable highlight
	Kernel.write<int>(entityPtr + OFFSET_HIGHLIGHT_GENERIC_CONTEXTS, 0);
	Kernel.write<int>(entityPtr + OFFSET_HIGHLIGHT_FOCUSED, 0);
}

// Controller support functions
bool IsControllerConnected()
{
	XINPUT_STATE state;
	DWORD result = XInputGetState(0, &state);
	return result == ERROR_SUCCESS;
}

bool IsControllerAimButtonPressed()
{
	XINPUT_STATE state;
	DWORD result = XInputGetState(0, &state);
	if (result != ERROR_SUCCESS) return false;
	
	// Left trigger (LT) for aimbot activation
	return state.Gamepad.bLeftTrigger > XINPUT_GAMEPAD_TRIGGER_THRESHOLD;
}

Vector2 GetControllerRightStick()
{
	Vector2 stick = { 0, 0 };
	XINPUT_STATE state;
	DWORD result = XInputGetState(0, &state);
	if (result == ERROR_SUCCESS)
	{
		// Normalize stick values from -32767 to 32767 to -1.0 to 1.0
		float deadzone = XINPUT_GAMEPAD_RIGHT_THUMB_DEADZONE;
		float x = state.Gamepad.sThumbRX;
		float y = state.Gamepad.sThumbRY;
		
		// Apply deadzone
		if (abs(x) < deadzone) x = 0;
		if (abs(y) < deadzone) y = 0;
		
		stick.x = x / 32767.0f;
		stick.y = y / 32767.0f;
	}
	return stick;
}

// DirectInput support for PS5 DualSense controller
static LPDIRECTINPUT8 g_pDI = nullptr;
static LPDIRECTINPUTDEVICE8 g_pJoystick = nullptr;
static DIJOYSTATE2 g_jsState;
static bool g_DIInitialized = false;

bool InitializeDirectInput()
{
	if (g_DIInitialized) return true;

	HRESULT hr = DirectInput8Create(GetModuleHandle(NULL), DIRECTINPUT_VERSION, IID_IDirectInput8, (void**)&g_pDI, NULL);
	if (FAILED(hr)) return false;

	// Enumerate devices to find PS5 controller
	hr = g_pDI->EnumDevices(DI8DEVCLASS_GAMECTRL, [](LPCDIDEVICEINSTANCE lpddi, LPVOID pvRef) -> BOOL CALLBACK {
		LPDIRECTINPUT8 pDI = (LPDIRECTINPUT8)pvRef;
		
		// Check for PS5 DualSense controller
		if (strstr(lpddi->tszProductName, "DualSense") || strstr(lpddi->tszProductName, "Wireless Controller"))
		{
			HRESULT hr = pDI->CreateDevice(lpddi->guidInstance, &g_pJoystick, NULL);
			if (SUCCEEDED(hr))
			{
				// Set data format
				hr = g_pJoystick->SetDataFormat(&c_dfDIJoystick2);
				if (SUCCEEDED(hr))
				{
					// Set cooperative level
					hr = g_pJoystick->SetCooperativeLevel(NULL, DISCL_BACKGROUND | DISCL_NONEXCLUSIVE);
					if (SUCCEEDED(hr))
					{
						g_DIInitialized = true;
						return DIENUM_STOP;
					}
				}
			}
		}
		return DIENUM_CONTINUE;
	}, g_pDI, DIEDFL_ALLDEVICES);

	return g_DIInitialized;
}

bool IsPS5ControllerConnected()
{
	if (!g_DIInitialized && !InitializeDirectInput()) return false;
	return g_pJoystick != nullptr;
}

bool IsPS5AimButtonPressed()
{
	if (!g_DIInitialized) InitializeDirectInput();
	if (!g_pJoystick) return false;

	HRESULT hr = g_pJoystick->Poll();
	if (FAILED(hr))
	{
		hr = g_pJoystick->Acquire();
		if (FAILED(hr)) return false;
		hr = g_pJoystick->Poll();
		if (FAILED(hr)) return false;
	}

	hr = g_pJoystick->GetDeviceState(sizeof(DIJOYSTATE2), &g_jsState);
	if (FAILED(hr)) return false;

	// L2 button (button 7 on PS5) for aimbot activation
	return (g_jsState.rgbButtons[7] & 0x80) != 0;
}

Vector2 GetPS5RightStick()
{
	Vector2 stick = { 0, 0 };
	if (!g_DIInitialized) InitializeDirectInput();
	if (!g_pJoystick) return stick;

	HRESULT hr = g_pJoystick->Poll();
	if (FAILED(hr))
	{
		hr = g_pJoystick->Acquire();
		if (FAILED(hr)) return stick;
		hr = g_pJoystick->Poll();
		if (FAILED(hr)) return stick;
	}

	hr = g_pJoystick->GetDeviceState(sizeof(DIJOYSTATE2), &g_jsState);
	if (FAILED(hr)) return stick;

	// Right stick is typically lZ (axis 4) and lRz (axis 5) in DIJOYSTATE2
	// Normalize from 0-65535 to -1.0 to 1.0
	float deadzone = 5000.0f;
	
	float x = g_jsState.lZ - 32767;
	float y = g_jsState.lRz - 32767;
	
	// Apply deadzone
	if (abs(x) < deadzone) x = 0;
	if (abs(y) < deadzone) y = 0;
	
	stick.x = x / 32767.0f;
	stick.y = y / 32767.0f;
	
	return stick;
}

std::vector<DWORD_PTR> EntityLoot = std::vector<DWORD_PTR>();
std::vector<DWORD_PTR> PlayerEntityList = std::vector<DWORD_PTR>();

#include <mutex>
class Manager;
extern std::mutex ManagerMutex;
extern std::mutex EntityListMutex;   // Protects PlayerEntityList
extern std::mutex LootListMutex;     // Protects EntityLoot
extern Manager* GlobalSharedManager;

uintptr_t NameList;

uintptr_t LocalPlayer;

class HitBoxManager
{
private:

public:

};

namespace LootItems {
	enum item_rarity { HEIRLOOM, LEGENDARY, EPIC, RARE, COMMON };

	struct CProps {
		std::string itemName;
		item_rarity rarity;
	};

	inline std::multimap<uint32_t, LootItems::CProps> itemLists = {
	{ 1, { "Kraber", item_rarity::HEIRLOOM }},
	{ 2, { "Mastiff", item_rarity::COMMON }},
	{ 6, { "Mastiff Gold", item_rarity::LEGENDARY }},
	{ 8, { "LStar", item_rarity::COMMON }},
	{ 14, { "Havoc", item_rarity::COMMON }},
	{ 20, { "Devotion", item_rarity::COMMON }},
	{ 26, { "Triple Take", item_rarity::COMMON }},
	{ 32, { "Flatline", item_rarity::COMMON }},
	{ 37, { "Hemlock", item_rarity::COMMON }},
	{ 44, { "G7 Scout", item_rarity::HEIRLOOM }},
	{ 49, { "Alternator", item_rarity::COMMON }},
	{ 55, { "R99", item_rarity::COMMON }},
	{ 61, { "Prowler", item_rarity::COMMON }},
	{ 57, { "Volt", item_rarity::LEGENDARY }},
	{ 72, { "Longbow", item_rarity::COMMON }},
	{ 77, { "Charge Rifle", item_rarity::COMMON }},
	{ 67, { "Volt", item_rarity::COMMON }},
	{ 82, { "Spitfire", item_rarity::COMMON }},
	{ 87, { "R301", item_rarity::COMMON }},
	{ 92, { "Spitfire", item_rarity::COMMON }},
	{ 98, { "R301", item_rarity::COMMON }},
	{ 104, { "Eva-8", item_rarity::COMMON }},
	{ 117, { "Wingman", item_rarity::COMMON }},
	{ 123, { "P2020", item_rarity::COMMON }},
	{ 136, { "RE45", item_rarity::COMMON }},
	{ 145, { "Sentinel", item_rarity::COMMON }},
	{ 152, { "Bocek", item_rarity::COMMON }},
	{ 159, { "Repeater", item_rarity::COMMON }},
	{ 169, { "Light Ammo", item_rarity::COMMON }},
	{ 170, { "Energy Ammo", item_rarity::COMMON }},
	{ 171, { "Shotgun Ammo", item_rarity::COMMON }},
	{ 172, { "Heavy Ammo", item_rarity::COMMON }},
	{ 173, { "Sniper Ammo", item_rarity::COMMON }},
	{ 129, { "Arrow", item_rarity::COMMON }},
	{ 176, { "Rampage", item_rarity::COMMON }},
	{ 183, { "Car SMG", item_rarity::HEIRLOOM }},
	{ 181, { "Nemesis", item_rarity::COMMON }},
	{ 221, { "Pheonix Kit", item_rarity::EPIC }},
	{ 222, { "Med Kit", item_rarity::RARE }},
	{ 223, { "Syringe", item_rarity::COMMON }},
	{ 224, { "Shield Battery", item_rarity::RARE }},
	{ 225, { "Shield Cell", item_rarity::COMMON }},
	{ 252, { "Body Armor[L1]", item_rarity::COMMON }},
	{ 253, { "Body Armor[L2]", item_rarity::RARE }},
	{ 254, { "Body Armor[L3]", item_rarity::EPIC }},
	{ 246, { "Body Armor[L4]", item_rarity::HEIRLOOM }},
	{ 369, { "Evo Cache", item_rarity::LEGENDARY}},
	{ 237, { "Heat Shield", item_rarity::HEIRLOOM }},
	{ 266, { "Knockdown Shield[L1]", item_rarity::COMMON }},
	{ 267, { "Knockdown Shield[L2]", item_rarity::RARE }},
	{ 268, { "Knockdown Shield[L3]", item_rarity::EPIC }},
	{ 269, { "Knockdown Shield[L4]", item_rarity::LEGENDARY }},
	{ 270, { "Backpack[L1]", item_rarity::COMMON }},
	{ 271, { "Backpack[L2]", item_rarity::RARE }},
	{ 272, { "Backpack[L3]", item_rarity::EPIC }},
	{ 273, { "Backpack[L4]", item_rarity::LEGENDARY }},
	{ 275, { "Thermite Grenade", item_rarity::COMMON }},
	{ 276, { "Frag Grenade", item_rarity::COMMON }},
	{ 277, { "Arc Star", item_rarity::COMMON }},
	{ 278, { "1x HCOG 'Classic'", item_rarity::COMMON }},
	{ 279, { "2x HCOG 'Bruiser'", item_rarity::RARE }},
	{ 280, { "1x Holo", item_rarity::COMMON }},
	{ 281, { "1x-2x Variable Holo", item_rarity::RARE }},
	{ 282, { "1x Digital Threat", item_rarity::LEGENDARY }},
	{ 283, { "3x HCOG 'Ranger'", item_rarity::EPIC }},
	{ 284, { "2x-4x Variable AOG", item_rarity::EPIC }},
	{ 267, { "6x Sniper", item_rarity::RARE }},
	{ 268, { "4x-8x Variable Sniper", item_rarity::EPIC }},
	{ 269, { "4x-10x Digital Sniper Threat", item_rarity::LEGENDARY }},
	{ 288, { "Barrel Stabilizer[L1]", item_rarity::COMMON }},
	{ 289, { "Barrel Stabilizer[L2]", item_rarity::RARE }},
	{ 290, { "Barrel Stabilizer[L3]", item_rarity::EPIC }},
	{ 277, { "Light Magazine[L1]", item_rarity::COMMON }},
	{ 278, { "Light Magazine[L2]", item_rarity::RARE }},
	{ 279, { "Light Magazine[L3]", item_rarity::EPIC }},
	{ 280, { "Light Magazine[L4]", item_rarity::LEGENDARY }},
	{ 281, { "Heavy Magazine[L1]", item_rarity::COMMON }},
	{ 282, { "Heavy Magazine[L2]", item_rarity::RARE }},
	{ 283, { "Heavy Magazine[L3]", item_rarity::EPIC }},
	{ 284, { "Heavy Magazine[L4]", item_rarity::LEGENDARY }},
	{ 304, { "Energy Magazine[L1]", item_rarity::COMMON }},
	{ 305, { "Energy Magazine[L2]", item_rarity::RARE }},
	{ 306, { "Energy Magazine[L3]", item_rarity::EPIC }},
	{ 307, { "Energy Magazine[L4]", item_rarity::LEGENDARY }},
	{ 308, { "Sniper Magazine[L1]", item_rarity::COMMON }},
	{ 309, { "Sniper Magazine[L2]", item_rarity::RARE }},
	{ 310, { "Sniper Magazine[L3]", item_rarity::EPIC }},
	{ 311, { "Sniper Magazine[L4]", item_rarity::LEGENDARY }},
	{ 312, { "Shotgun Bolt[L1]", item_rarity::COMMON }},
	{ 313, { "Shotgun Bolt[L2]", item_rarity::RARE }},
	{ 314, { "Shotgun Bolt[L3]", item_rarity::EPIC }},
	{ 315, { "Shotgun Bolt[L4]", item_rarity::LEGENDARY } },
	{ 292, { "Laser Sight[L1]", item_rarity::COMMON }},
	{ 293, { "Laser Sight[L2]", item_rarity::RARE }},
	{ 294, { "Laser Sight[L3]", item_rarity::EPIC }},
	{ 295, { "Laser Sight[L4]", item_rarity::LEGENDARY } },
	{ 316, { "Standard Stock[L1]", item_rarity::COMMON }},
	{ 317, { "Standard Stock[L2]", item_rarity::RARE }},
	{ 318, { "Standard Stock[L3]", item_rarity::EPIC }},
	{ 319, { "Sniper Stock[L1]", item_rarity::COMMON }},
	{ 320, { "Sniper Stock[L2]", item_rarity::RARE }},
	{ 321, { "Sniper Stock[L3]", item_rarity::EPIC }},
	{ 322, { "Sniper Stock[L4]", item_rarity::LEGENDARY } },
	{ 234, { "Turbocharger", item_rarity::LEGENDARY }},
	{ 238, { "Hammer Point", item_rarity::EPIC }},
	{ 237, { "Skull Piercer", item_rarity::LEGENDARY } },
	{ 245, { "Deadeye's Tempo", item_rarity::EPIC } },
	{ 246, { "Quickdraw Holster", item_rarity::EPIC } },
	{ 247, { "Shatter Caps", item_rarity::EPIC }},
	{ 248, { "Kinetic Feeder", item_rarity::EPIC } },
	{ 249, { "Boosted Loader", item_rarity::LEGENDARY }},
	{ 245, { "Mobile Respawn Beacon", item_rarity::RARE }},
	{ 246, { "Vault Key", item_rarity::HEIRLOOM }},
	{ 248, { "Treasure Pack", item_rarity::HEIRLOOM }},
	{ 249, { "Survival Doubler", item_rarity::HEIRLOOM }},
	{ 250, { "Survival Quadrupler", item_rarity::HEIRLOOM }},
	{ 251, { "Heat Shield", item_rarity::RARE }},
	{ 252, { "Mobile Respawn Beacon 2", item_rarity::RARE }},
	{ 253, { "Treasure Pack", item_rarity::HEIRLOOM }},
	{ 254, { "Vault Key", item_rarity::HEIRLOOM }},
	{ 256, { "Knife", item_rarity::HEIRLOOM }},
	{ 397, { "Patrol Kit", item_rarity::LEGENDARY }},
	{ 345, { "Evac Tower", item_rarity::RARE } },
	{ 220, { "Ultimate Accelerant", item_rarity::EPIC } },
	{ 393, { "Increased Power", item_rarity::EPIC } },
	{ 392, { "Excess Care", item_rarity::EPIC } },
	{ 391, { "Inexhaustible Batteries", item_rarity::EPIC } },
	{ 394, { "Mega-Armor", item_rarity::EPIC } },
	{ 390, { "Infinite Ammo", item_rarity::EPIC } }
	};
}

DWORD64 GetEntityById(int Ent, DWORD64 Base)
{
	DWORD64 EntityList = Base + OFFSET_ENTITYLIST;
	DWORD64 BaseEntity = Kernel.read<DWORD64>(EntityList);
	if (!BaseEntity)
		return NULL;
	return   Kernel.read<DWORD64>(EntityList + (Ent << 5));
}

Vector GetEntityBonePosition(uintptr_t Ent, uint32_t BoneId)
{
	Vector Out{};
	Vector Origin = Kernel.read<Vector>(Ent + OFFSET_ORIGIN);
	unsigned long long pBoneArray = Kernel.read<DWORD_PTR>(Ent + OFFSET_BONES);

	Out.x = Kernel.read<float>(pBoneArray + 0xCC + (BoneId * 0x30)) + Origin.x;
	Out.y = Kernel.read<float>(pBoneArray + 0xDC + (BoneId * 0x30)) + Origin.y;
	Out.z = Kernel.read<float>(pBoneArray + 0xEC + (BoneId * 0x30)) + Origin.z;

	return Out;
}

namespace mem
{
	std::size_t BoundedStringLength(const char* value, std::size_t maxLength)
	{
		std::size_t length = 0;
		while (length < maxLength && value[length] != '\0')
			++length;
		return length;
	}

	std::string ReadString(std::uintptr_t address, std::size_t maxLength)
	{
		if (maxLength == 0)
			return {};

		// Read in chunks of 16 bytes for better performance
		constexpr size_t chunkSize = 16;
		std::string result;
		result.reserve(maxLength);
		
		for (std::size_t i = 0; i < maxLength; i += chunkSize) {
			size_t readSize = min(chunkSize, maxLength - i);
			
			// Read as uint64_t chunks for speed
			for (size_t j = 0; j < readSize; j++) {
				char c = Kernel.read<char>(address + i + j);
				if (c == '\0') return result;
				result += c;
			}
		}
		return result;
	}
}

// Assuming 'targetDTB' is stored in your entity/process object
std::string GetSignifier(uintptr_t ent)
{
	uintptr_t sigAddr = Kernel.read<uintptr_t>(ent + OFFSET_SIGNIFIER);
	return mem::ReadString(sigAddr, 64);
}

std::string GetName(uintptr_t pEntityBase) {
	uintptr_t NameIndex = Kernel.read<uintptr_t>(pEntityBase + 0x38);
	uintptr_t NameOffset = Kernel.read<uintptr_t>(Kernel.GetBaseAddress() + OFFSET_NAMELIST + ((NameIndex - 1) * 24));
	return mem::ReadString(NameOffset, 64);
}

bool IsAlive(uint64_t entity)
{
	int isAlive = Kernel.read<int>(entity + OFFSET_LIFE_STATE);
	return isAlive == 0;
}

bool IsValidEnemy(uintptr_t player, uintptr_t LocalPlayer)
{
	// 1. Always check for null pointers first to prevent crashes
	if (player == 0 || LocalPlayer == 0) return false;

	// 2. Read values once
	int health = Kernel.read<int>(player + OFFSET_HEALTH);
	int teamID = Kernel.read<int>(player + OFFSET_TEAM);
	int localTeamID = Kernel.read<int>(LocalPlayer + OFFSET_TEAM);

	// 3. Health check: If health is 0, they are dead. 
	// If it's > 100, ensure your game's max health is actually 100.
	if (health <= 0 || health > 100) return false;

	// 4. Team check: If teams are equal, it's a teammate
	if (teamID == localTeamID) return false;

	// 5. Team ID Range check (optional, only if 0-32 is strict)
	if (teamID < 0 || teamID > 32) return false;

	// 6. Alive check (Call this last to save performance if previous checks fail)
	if (!IsAlive(player)) return false;

	return true;
}

struct Vector _WorldToScreen(const struct Vector pos, struct Matrix matrix) {
	struct Vector out;
	float _x = matrix.matrix[0] * pos.x + matrix.matrix[1] * pos.y + matrix.matrix[2] * pos.z + matrix.matrix[3];
	float _y = matrix.matrix[4] * pos.x + matrix.matrix[5] * pos.y + matrix.matrix[6] * pos.z + matrix.matrix[7];
	out.z = matrix.matrix[12] * pos.x + matrix.matrix[13] * pos.y + matrix.matrix[14] * pos.z + matrix.matrix[15];

	_x *= 1.f / out.z;
	_y *= 1.f / out.z;

	int width = 1920;
	int height = 1080;

	out.x = width * .5f;
	out.y = height * .5f;

	out.x += 0.5f * _x * width + 0.5f;
	out.y -= 0.5f * _y * height + 0.5f;

	return out;
}

Vector GetBoneScreenPos(uintptr_t bones, uintptr_t hitboxArray, int hitboxId, Vector absOrigin, Matrix m) {
	// Read the bone index for this specific hitbox ID
	// Note: The offset 0x20 is standard for many SDKs, but verify against your specific version
	uint16_t boneIndex = Kernel.read<uint16_t>(hitboxArray + (hitboxId * 0x20));

	if (boneIndex > 255) return Vector();

	// Read the matrix from the bone array
	Matrix3x4 boneMatrix = Kernel.read<Matrix3x4>(bones + (boneIndex * sizeof(Matrix3x4)));

	// Calculate final world position
	Vector boneWorldPos = {
		boneMatrix._14 + absOrigin.x,
		boneMatrix._24 + absOrigin.y,
		boneMatrix._34 + absOrigin.z
	};

	return _WorldToScreen(boneWorldPos, m);
}

uintptr_t GetEntityBoneArray(uintptr_t ent)
{
	return Kernel.read<uintptr_t>(ent + OFFSET_BONES);
}

struct matrix3x4_t
{
	matrix3x4_t() {}
	matrix3x4_t(
		float m00, float m01, float m02, float m03,
		float m10, float m11, float m12, float m13,
		float m20, float m21, float m22, float m23)
	{
		m_flMatVal[0][0] = m00;	m_flMatVal[0][1] = m01; m_flMatVal[0][2] = m02; m_flMatVal[0][3] = m03;
		m_flMatVal[1][0] = m10;	m_flMatVal[1][1] = m11; m_flMatVal[1][2] = m12; m_flMatVal[1][3] = m13;
		m_flMatVal[2][0] = m20;	m_flMatVal[2][1] = m21; m_flMatVal[2][2] = m22; m_flMatVal[2][3] = m23;
	}

	float* operator[](int i) { Assert((i >= 0) && (i < 3)); return m_flMatVal[i]; }
	const float* operator[](int i) const { Assert((i >= 0) && (i < 3)); return m_flMatVal[i]; }
	float* Base() { return &m_flMatVal[0][0]; }
	const float* Base() const { return &m_flMatVal[0][0]; }

	float m_flMatVal[3][4];
};

Vector GetBonePositionByHitbox(uintptr_t entity, int id, Vector origin)
{
	// Cache StudioHdr details per entity ptr to avoid repeating 4 driver reads per bone check
	static std::unordered_map<uintptr_t, std::pair<uint64_t, int>> hitboxMetaCache;

	// Clear every 30s - entity addresses get recycled when players leave/join
	static auto lastHitboxCacheClear = std::chrono::steady_clock::now();
	{
		auto now = std::chrono::steady_clock::now();
		if (std::chrono::duration_cast<std::chrono::seconds>(now - lastHitboxCacheClear).count() > 30) {
			hitboxMetaCache.clear();
			lastHitboxCacheClear = now;
		}
	}
	
	uint64_t HitBoxsArray = 0;
	int HitboxIndex = 0;

	auto it = hitboxMetaCache.find(entity);
	if (it != hitboxMetaCache.end()) {
		HitBoxsArray = it->second.first;
		HitboxIndex = it->second.second;
	} else {
		uint64_t Model = Kernel.read<uint64_t>(entity + OFFSET_studioHdr);
		if (!Model) return Vector();
		uint64_t StudioHdr = Kernel.read<uint64_t>(Model + 0x8);
		if (!StudioHdr) return Vector();

		uint16_t HitboxCache = Kernel.read<uint16_t>(StudioHdr + 0x34);
		HitBoxsArray = StudioHdr + ((uint16_t)(HitboxCache & 0xFFFE) << (4 * (HitboxCache & 1)));

		uint16_t IndexCache = Kernel.read<uint16_t>(HitBoxsArray + 0x4);
		HitboxIndex = ((uint16_t)(IndexCache & 0xFFFE) << (4 * (IndexCache & 1)));
		
		hitboxMetaCache[entity] = { HitBoxsArray, HitboxIndex };
	}

	uint16_t Bone = Kernel.read<uint16_t>(HitBoxsArray + HitboxIndex + (id * 0x20));

	if (Bone < 0 || Bone > 255)
		return Vector();

	//hitpos
	uint64_t BoneArray = GetEntityBoneArray(entity);

	matrix3x4_t Matrix = Kernel.read<matrix3x4_t>(BoneArray + Bone * sizeof(matrix3x4_t));

	return Vector(Matrix.m_flMatVal[0][3] + origin.x, Matrix.m_flMatVal[1][3] + origin.y, Matrix.m_flMatVal[2][3] + origin.z);
}

class MainPlayer {
private:

public:
	MainPlayer() {}
	MainPlayer(DWORD_PTR _ptr) {
		this->ptr = _ptr;
		
		// Use cached reads where possible
		this->Localplayer = UintptrCache.Get(Kernel.GetBaseAddress() + OFFSET_LOCAL_ENT);
		if (!this->Localplayer) {
			this->Localplayer = Kernel.read<INT>(Kernel.GetBaseAddress() + OFFSET_LOCAL_ENT);
			UintptrCache.Set(Kernel.GetBaseAddress() + OFFSET_LOCAL_ENT, this->Localplayer);
		}
		
		this->Team = IntCache.Get(ptr + OFFSET_TEAM);
		if (this->Team == 0) {
			this->Team = Kernel.read<INT>(ptr + OFFSET_TEAM);
			IntCache.Set(ptr + OFFSET_TEAM, this->Team);
		}
		
		this->Health = IntCache.Get(ptr + OFFSET_HEALTH);
		if (this->Health == 0) {
			this->Health = Kernel.read<INT>(ptr + OFFSET_HEALTH);
			IntCache.Set(ptr + OFFSET_HEALTH, this->Health);
		}
		
		this->ShieldHealth = IntCache.Get(ptr + OFFSET_SHIELD);
		if (this->ShieldHealth == 0) {
			this->ShieldHealth = Kernel.read<INT>(ptr + OFFSET_SHIELD);
			IntCache.Set(ptr + OFFSET_SHIELD, this->ShieldHealth);
		}
		
		this->ShieldHealthMax = IntCache.Get(ptr + OFFSET_SHIELDMAX);
		if (this->ShieldHealthMax == 0) {
			this->ShieldHealthMax = Kernel.read<INT>(ptr + OFFSET_SHIELDMAX);
			IntCache.Set(ptr + OFFSET_SHIELDMAX, this->ShieldHealthMax);
		}
		
		this->origin = VectorCache.Get(ptr + OFFSET_ORIGIN);
		if (this->origin.x == 0 && this->origin.y == 0 && this->origin.z == 0) {
			this->origin = Kernel.read<Vector>(ptr + OFFSET_ORIGIN);
			VectorCache.Set(ptr + OFFSET_ORIGIN, this->origin);
		}

		unsigned long long pBoneArray = Kernel.read<DWORD_PTR>(ptr + OFFSET_BONES);

		Head.x = Kernel.read<float>(pBoneArray + 0xCC + (8 * 0x30)) + origin.x;
		Head.y = Kernel.read<float>(pBoneArray + 0xDC + (8 * 0x30)) + origin.y;
		Head.z = Kernel.read<float>(pBoneArray + 0xEC + (8 * 0x30)) + origin.z;
	}

	DWORD_PTR ptr = 0x0;
	INT Health = 0;
	INT ShieldHealth = 0;
	INT ShieldHealthMax = 0;
	INT Team = 0;
	uintptr_t Localplayer;
	Vector origin = Vector();
	Vector Head = Vector();

	bool IsValid() const {
		// Use a small epsilon check for floats instead of == 0
		return (ptr != 0 && Health > 0 && Team != 0);
	}
};

class Loot {
private:

public:
	Loot() {}
	Loot(DWORD_PTR _ptr) {
		this->ptr = _ptr;
		this->origin = VectorCache.Get(ptr + OFFSET_ORIGIN);
		if (this->origin.x == 0 && this->origin.y == 0 && this->origin.z == 0) {
			this->origin = Kernel.read<Vector>(ptr + OFFSET_ORIGIN);
			VectorCache.Set(ptr + OFFSET_ORIGIN, this->origin);
		}
		this->nameid = Kernel.read<DWORD>(ptr + OFFSET_ITEMID);
	}
	DWORD_PTR ptr = 0x0;
	Vector origin = Vector();
	DWORD nameid;
	FLOAT Distance = 0;

	BOOL IsValid() {
		if ((this->origin.x != NULL) &&
			(this->ptr != NULL)) return true;
		return false;
	}
};

class WeaponXEntity {
public:
	float projectile_speed = 0;
	float projectile_scale = 0;
	float zoom_fov = 0;
	int ammo = 0;

	void update(uint64_t LocalPlayer) {
		extern uint64_t GBase;
		uint64_t entitylist = GBase + OFFSET_ENTITYLIST;
		
		uint64_t wephandle = Kernel.read<uint64_t>(LocalPlayer + OFFSET_CURRENT_WEAPON);
		wephandle &= 0xffff;

		uint64_t wep_entity = Kernel.read<uint64_t>(entitylist + (wephandle << 5));

		if (wep_entity) {
			projectile_speed = Kernel.read<float>(wep_entity + OFFSET_BULLET_SPEED);
			projectile_scale = Kernel.read<float>(wep_entity + OFFSET_BULLET_SCALE);
			zoom_fov = Kernel.read<float>(wep_entity + OFFSET_ZOOM_FOV);
			ammo = Kernel.read<int>(wep_entity + OFFSET_AMMO);
		}
	}

	float get_projectile_speed() { return projectile_speed; }
	float get_projectile_gravity() { return projectile_scale; }
	float get_zoom_fov() { return zoom_fov; }
};

struct PredictCtx {
	Vector StartPos;
	Vector TargetPos;
	Vector TargetVel;
	float BulletSpeed;
	float BulletGravity;

	Vector2D AimAngles;
};

Vector ExtrapolatePos(const PredictCtx& Ctx, float Time) {
	return Ctx.TargetPos + (Ctx.TargetVel * Time);
}

bool OptimalPitch(const PredictCtx& Ctx, const Vector2D& Dir2D, float* OutPitch) {
	float Vel = Ctx.BulletSpeed, Grav = Ctx.BulletGravity, DirX = Dir2D.x, DirY = Dir2D.y;
	float Root = Vel * Vel * Vel * Vel - Grav * (Grav * DirX * DirX + 2.f * DirY * Vel * Vel);
	
	static int debugCounter = 0;
	if (debugCounter++ % 60 == 0) {
		printf("[DEBUG PREDICT] OptimalPitch: Vel=%.2f, Grav=%.2f, DirX=%.2f, DirY=%.2f\n", Vel, Grav, DirX, DirY);
		printf("[DEBUG PREDICT] Root value: %.2f\n", Root);
	}
	
	if (Root >= 0.f) { 
		*OutPitch = atanf((Vel * Vel - sqrt(Root)) / (Grav * DirX)); 
		return true; 
	}
	return false;
}

bool SolveTrajectory(PredictCtx& Ctx, const Vector& ExtrPos, float* TravelTime) {
	Vector Dir = ExtrPos - Ctx.StartPos;
	Vector2D Dir2D = { sqrtf(Dir.x * Dir.x + Dir.y * Dir.y), Dir.z };

	float CurPitch;
	if (!OptimalPitch(Ctx, Dir2D, &CurPitch)) {
		return false;
	}

	*TravelTime = Dir2D.x / (cosf(CurPitch) * Ctx.BulletSpeed);
	Ctx.AimAngles.y = atan2f(Dir.y, Dir.x);
	Ctx.AimAngles.x = CurPitch;
	return true;
}

bool BulletPredict(PredictCtx& Ctx) {
	float MAX_TIME = 1.f, TIME_STEP = (1.f / 256.f);
	for (float CurrentTime = 0.f; CurrentTime <= MAX_TIME; CurrentTime += TIME_STEP) {
		float TravelTime;
		Vector ExtrPos = ExtrapolatePos(Ctx, CurrentTime);
		if (!SolveTrajectory(Ctx, ExtrPos, &TravelTime)) {
			return false;
		}

		if (TravelTime < CurrentTime) {
			Ctx.AimAngles = { -RAD2DEG(Ctx.AimAngles.x), RAD2DEG(Ctx.AimAngles.y) };
			return true;
		}
	}
	return false;
}

class Manager {
private:
public:
	std::vector<Loot> Loots;
	std::vector<MainPlayer> Players;

	Manager(int i) {
		if (i == 0) return;

		// Take a local snapshot under lock to avoid data races with scanner threads
		std::vector<DWORD_PTR> lootSnapshot;
		{
			std::lock_guard<std::mutex> lock(LootListMutex);
			lootSnapshot = EntityLoot;
		}
		for (size_t i = 0; i < lootSnapshot.size(); i++) {
			Loot loot(lootSnapshot[i]);
			if (loot.ptr == 0) continue;
			if (loot.IsValid() && loot.ptr != LocalPlayer) {
				this->Loots.push_back(loot);
			}
		}

		std::vector<DWORD_PTR> playerSnapshot;
		{
			std::lock_guard<std::mutex> lock(EntityListMutex);
			playerSnapshot = PlayerEntityList;
		}
		for (size_t i = 0; i < playerSnapshot.size(); i++) {
			MainPlayer player(playerSnapshot[i]);
			if (player.ptr == 0) continue;
			if (player.IsValid() && player.ptr != LocalPlayer) {
				this->Players.push_back(player);
			}
		}
	}
};

// Cache definitions
AddressCache<Matrix> MatrixCache;
AddressCache<Vector> VectorCache;
AddressCache<int> IntCache;
AddressCache<uintptr_t> UintptrCache;