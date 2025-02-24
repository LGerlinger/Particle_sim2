#pragma once

#include <cstdint>
#include <mutex>
#include <thread>
#include <pthread.h> // I couldn't find a way to synchronize multiple threads with modern c++. Surely there's a way
#include <semaphore.h>


#include "Consometer.hpp"
#include "Particle.hpp"
#include "Segment.hpp"
#include "World.hpp"



#define NB_PART 12000
#define MAX_THREAD_NUM 6

class Particle_simulator {
public :
	float masses = 1;
	float radii = 4;
	float dt = 0.002;
	
	uint32_t nb_active_part = NB_PART;
	Particle particle_array[NB_PART];
	std::mutex particle_mutex;
	uint32_t nb_active_seg = 6;
	Segment seg_array[6];

	World world;

	// Simulation multi-threading
	std::thread* thread_list[MAX_THREAD_NUM];
	bool threads_created = false;
	bool simulate = true;
	bool paused = false;
	bool step = false;
	pthread_mutex_t sync_mutex = PTHREAD_MUTEX_INITIALIZER;
	pthread_cond_t sync_condition = PTHREAD_COND_INITIALIZER;
	uint8_t sync_count;

	Particle_simulator();
	~Particle_simulator();

	void start_simulation_threads();
	void stop_simulation_threads();
	void simulation_thread(uint8_t th_id);
	void simu_step();
	void solver(uint32_t p_start, uint32_t p_end);

	// Collisions
	void collision_pp(uint32_t p_start, uint32_t p_end);
	void collision_pp_grid();
	void collision_pp_grid_threaded(uint32_t p_start, uint32_t p_end);
	void collision_pl(uint32_t p_start, uint32_t p_end);
	
	// General forces
	void gravity(uint32_t p_start, uint32_t p_end);
	float grav_center[2] = {1000, 500};
	void point_gravity(uint32_t p_start, uint32_t p_end);
	void static_friction(uint32_t p_start, uint32_t p_end);
	void fluid_friction(uint32_t p_start, uint32_t p_end);

	// User forces
	enum class userForce {None = 0, Translation, Translation_ranged, Rotation, Rotation_ranged, Vortex, Vortex_ranged};
	userForce appliedForce = userForce::None;
	float user_point[2];
	float translation_force = 3000;
	float rotation_force = 1000;
	float range = 200;
	bool deletion_order = false;
	void attraction(uint32_t p_start, uint32_t p_end);
	void attraction_ranged(uint32_t p_start, uint32_t p_end);
	void rotation(uint32_t p_start, uint32_t p_end);
	void rotation_ranged(uint32_t p_start, uint32_t p_end);
	void vortex(uint32_t p_start, uint32_t p_end);
	void vortex_ranged(uint32_t p_start, uint32_t p_end);


	// Creation / deletion of particles
	void delete_particle(uint32_t p);
	void delete_range(float x, float y, float range_);

	// perfomance check
	Consometre conso;
};