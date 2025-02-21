#pragma once

#include <cstdint>
#include <mutex>
#include <thread>
#include <pthread.h> // I couldn't find a way to synchronize multiple threads with modern c++. Surely there's a way
#include <semaphore.h>

#include "Particle.hpp"
#include "Segment.hpp"
#include "World.hpp"



#define NB_PART 10002
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

	// Collisions
	void collision_pp(uint32_t pn);
	void collision_pp_grid();
	void collision_pp_grid_threaded(uint32_t pn);
	void collision_pl(uint32_t pn);
	
	// General forces
	inline void gravity(uint32_t pn);
	float grav_center[2] = {1000, 500};
	inline void point_gravity(uint32_t pn);
	inline void static_friction(uint32_t pn);
	inline void fluid_friction(uint32_t pn);

	// User forces
	enum class userForce {None = 0, Translation, Rotation, Vortex};
	userForce appliedForce = userForce::None;
	float user_point[2];
	float translation_force = 2000;
	float rotation_force = 1000;
	inline void attraction(uint32_t pn);
	inline void rotation(uint32_t pn);
	inline void vortex(uint32_t pn);

	inline void solver(uint32_t pn);
};