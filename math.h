
#pragma once

#define max(a,b) ((a)>(b)?(a):(b))
#define min(a,b) ((a)>(b)?(b):(a))

#define max3(a,b,c) max(a,max(b,c))
#define min3(a,b,c) min(a,min(b,c))


static inline double clampf(double v, double min, double max)
{
	if(v<min)
		return min;
	else if(v>max)
		return max;
	else
		return v;
}

#define VECTOR(type,size) type __attribute__ ((vector_size ((size)*sizeof(type))))

typedef VECTOR(int,2) Vec2i;
typedef VECTOR(int,4) Vec4i;

typedef VECTOR(float,2) Vec2f;
typedef VECTOR(float,4) Vec4f;

typedef VECTOR(unsigned char,2) Vec2b;
typedef VECTOR(unsigned char,4) Vec4b;

typedef VECTOR(short,2) Vec2s;
typedef VECTOR(short,4) Vec4s;

