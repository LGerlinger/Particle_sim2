#pragma once

#include <cstdint>
#include <thread>
#include <pthread.h> // I couldn't find a way to synchronize multiple threads with modern c++. Surely there's a way
#include <semaphore.h>


#include "Consometer.hpp"
#include "Particle.hpp"
#include "Segment.hpp"
#include "World.hpp"


#define NB_PART 24000
#define MAX_THREAD_NUM 6
#define NULLPART (uint32_t)-1

class Particle_simulator {
public :
	float masses = 1;
	float radii = 2;
	float dt = 0.001;

private :
	// particles and segments
	float time[2] = {0, 0};
	uint32_t nb_active_part = NB_PART; //< Number of Particles being simulated and displayed
	uint32_t nb_active_seg = 6; //< Number of Segments being simulated and displayed
public :
	inline uint32_t get_active_part() {return nb_active_part;};
	inline uint32_t get_active_seg() {return nb_active_seg;};
	Particle particle_array[NB_PART];
	Segment seg_array[6];

	World world;

	// Simulation multi-threading
	std::thread* thread_list[MAX_THREAD_NUM];
	bool threads_created = false;
	pthread_mutex_t sync_mutex = PTHREAD_MUTEX_INITIALIZER;
	pthread_cond_t sync_condition = PTHREAD_COND_INITIALIZER;
	uint8_t sync_count;

	// Orders to give to the simulator
	bool simulate = true;
	bool paused = false;
	bool step = false;
	bool quickstep = false;

	// Perfomance check
	Consometre conso; //< Used to measure and display performances of the simulation
	Consometre conso2; //< Used to measure and display performances of the simulation

public :
	/**
	* @brief Constructor. Initializes world, particle_array and seg_array.
	*/
	Particle_simulator();

	/**
	* @brief Destructor. Stops the simulation. Stop and joins the simulation threads if created 
	*/
	~Particle_simulator();

	void start_simulation_threads();
	
	/**
	* @brief Sends a signal for each simulation thread to stop and waits for each one to return.
	* @details After the signal, each thread will still finish it simulation step. This function won't return before that. 
	*/
	void stop_simulation_threads();

	/**
	* @brief Contains the simulation loop. This function is meant to be given to a thread.
	* @details This is where threads are synchronised and the forces can be applied. The synchronisation contains a trick to pause the threads and step the simulation. 
	*/
	void simulation_thread(uint8_t th_id);

	/**
	* @brief Does a complete simulation step over all Particles, not meant to be threaded. 
	*/
	void simu_step();

	/**
	* @brief Updates speed according to acceleration and position according to speed (in that order).
	* @param p_start start index of Particles to update (included).
	* @param p_end end index of Particles to update (excluded).
	*/
	void solver(uint32_t p_start, uint32_t p_end);

	// Collisions
	/**
	* @brief Check and apply collision between Particles in [p_start, p_end[ and every Particle.
	*/
	void collision_pp(uint32_t p_start, uint32_t p_end);

	/**
	* @brief Check and apply collision between all Particles using the world's grid.
	* @warning This is basically collision_pp_grid_threaded (0, nb_active_part) , but somewhat worse. Thus this function is DEPRECATED.
	*/
	void collision_pp_grid();

	/**
	* @brief Check and apply collision between Particles in [p_start, p_end[ and every Particle using the world's grid.
	* @warning The implementation shouldn't be thread-safe, but this never caused a problem and solving this could decrease performance. I'll see to it later.
	*/
	void collision_pp_grid_threaded(uint32_t p_start, uint32_t p_end);

	/**
	* @brief Check and apply collision between Particles in [p_start, p_end[ and every Segment in seg_array.
	*/
	void collision_pl(uint32_t p_start, uint32_t p_end);

	/**
	* @brief Check and apply collision between Particles in [p_start, p_end[ and every Segment in seg_array using the world's grid..
	*/
	void collision_pl_grid(uint32_t p_start, uint32_t p_end);

	/**
	* @brief Check and apply collision (and a sticking contact) between Particles in [p_start, p_end[ and every Particle using the world's grid.
	* @see collision_pp_grid_threaded
	*/
	void collision_pp_glue(uint32_t p_start, uint32_t p_end);

	/**
	* @brief Keep Particles in [p_start, p_end[ inside the world borders.
	*/
	void world_borders(uint32_t p_start, uint32_t p_end);
	
	// General forces
	void gravity(uint32_t p_start, uint32_t p_end);
	float grav_center[2] = {1000, 500};
	void point_gravity(uint32_t p_start, uint32_t p_end);
	void static_friction(uint32_t p_start, uint32_t p_end);
	void fluid_friction(uint32_t p_start, uint32_t p_end);
	void vibrate(uint32_t p_start, uint32_t p_end);

	// User forces
	enum class userForce {None = 0, Translation, Translation_ranged, Rotation, Rotation_ranged, Vortex, Vortex_ranged};
	userForce appliedForce = userForce::None;
	float user_point[2]; //< Used so the user can point a place in the world and interact with the simulation
	float translation_force = 3000;
	float rotation_force = 1000;
	float range = 200;
	bool deletion_order = false; //< If delete_range deletion should be called
	void attraction(uint32_t p_start, uint32_t p_end);
	void attraction_ranged(uint32_t p_start, uint32_t p_end);
	void rotation(uint32_t p_start, uint32_t p_end);
	void rotation_ranged(uint32_t p_start, uint32_t p_end);
	void vortex(uint32_t p_start, uint32_t p_end);
	void vortex_ranged(uint32_t p_start, uint32_t p_end);


	// Creation / deletion of particles
	/**
	* @brief Delete all particles in a circle of center [x, y] in the radius range_.
	*/
	void delete_range(float x, float y, float range_);
	void delete_particle(uint32_t p);
	uint32_t create_particle(float x, float y);
};