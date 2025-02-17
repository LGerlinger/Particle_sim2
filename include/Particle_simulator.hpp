#pragma once

#include "Particle.hpp"
#include "Segment.hpp"
#include "World.hpp"
#include <cstdint>

#define NB_PART 2000

class Particle_simulator {
public :
	float masses = 1;
	float radii = 5;
	float dt = 0.001;
	
	uint32_t nb_active_part = NB_PART;
	Particle particle_array[NB_PART];
	uint32_t nb_active_seg = 4;
	Segment seg_array[4];

	World world;



	Particle_simulator();
	~Particle_simulator() = default;

	void simu_step();

	// Collisions
	void collision_pp();
	void collision_pp_grid();
	void collision_pl();
	
	// General forces
	void gravity();
	float grav_center[2] = {500, 500};
	void point_gravity();
	void static_friction();
	void fluid_friction();

	void solver();
};