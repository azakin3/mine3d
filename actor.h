
#pragma once

#include "math.h"
#include <stdbool.h>

typedef struct
{
	Vec4f pos;
	Vec4f size;
	Vec4f v;
	Vec2f rot;
	bool on_ground;
	double stance;
}Actor;

struct World;
typedef struct World World;

void actorTick(World* world, Actor* actor);