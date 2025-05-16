#pragma once

#include "Consometer.hpp"
#include "Particle.hpp"
#include "World.hpp"
#include "ThreadHandler.hpp"

#include <cstdint>

#define NB_PART 24000
#define MAX_THREAD_NUM 6
#define NULLPART (uint32_t)-1

class Particle_simulator {
public :
	float masses = 1;
	float radii = 4;
	float dt = 0.001;

private :
	// particles and segments
	float time[2] = {0, 0};
	uint32_t nb_active_part = NB_PART; //< Number of Particles being simulated and displayed
public :
	inline uint32_t get_active_part() {return nb_active_part;};
	Particle particle_array[NB_PART];

	World world;

	// Simulation multi-threading
	ThreadHandler threadHandler;

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

	/**
	* @brief Initializes the position and speed of the Particles.
	* @details The positions are randomly asigned in the world and the speed are null.
	*/
	void initialize_particles();

	/**
	* @brief Starts the 
	* @see Particle_simulator::simulation_thread
	*/
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
	* @brief simulation_thread without ThreadHandler::load_repartion for debugging purposes.
	*/
	void simulation_thread2(uint8_t th_id);

	private : 
		/**
		* @brief Calls pause_wait then create_destroy_wait.
		* @details What to do while waiting for synchronization.
		*/
		void loop_wait();
		/**
		* @brief Sleeps as long as the simulation is paused or not stepping.
		* @details One of the simulating thread calls this while other wait for his synchronization. This exists because really only one thread needs to call this.
		*/
		void pause_wait();
		/**
		* @brief Deletes some particles if deletion_order. Creates Particles if possible. Deletes Particle of which position is NaN. 
		* @details One of the simulating thread calls this while other wait for his synchronization. This exists because only one thread should call this at a time.
		*/
		void create_destroy_wait();
	public :

	/**
	* @brief Updates speed according to acceleration and position according to speed (in that order).
	* @param p_start start index of Particles to update (included).
	* @param p_end end index of Particles to update (excluded).
	*/
	void update_pos(uint32_t p_start, uint32_t p_end);

	// Collisions
	/**
	* @brief Check and apply collision between Particles in [p_start, p_end[ and every Particle.
	*/
	void collision_pp(uint32_t p_start, uint32_t p_end);

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
	* @brief Check and apply collision between Particles in [p_start, p_end[ and every Segment in seg_array using the world's grid.
	* @details Loops over every Particle to check for a collision with a Segment.
	*/
	void collision_pl_grid(uint32_t p_start, uint32_t p_end);

	/**
	* @brief Check and apply collision between the segment seg_num and the Particles known to be in contact thanks to the world's grid.
	* @details Loops over every cell of the Segment to check for a collision with a Particle.
	*/
	void collision_lp_grid(uint32_t c_start, uint32_t c_end, uint16_t seg_num); // Particule-segment collision over the cell [p_start, p_end[ of segment seg_num

	/**
	* @brief Check and apply collision (and a sticking contact) between Particles in [p_start, p_end[ and every Particle using the world's grid.
	* @see collision_pp_grid_threaded
	*/
	void collision_pp_glue(uint32_t p_start, uint32_t p_end);

	/**
	* @brief Keep Particles in [p_start, p_end[ inside the world borders.
	*/
	void world_borders(uint32_t p_start, uint32_t p_end);
	
	/**
	* @brief Used to loop the world around by teleporting Particles from a border to the other.
	* @warning It works but doesn't complement well with collisions yet.
	*/
	void world_loop(uint32_t p_start, uint32_t p_end);

	/**
	* @brief Deletes every Particle of which position is NaN.
	* @warning Not yet threadable.
	*/
	void delete_NaNs(uint32_t p_start, uint32_t p_end);
	
	// General forces
	void gravity(uint32_t p_start, uint32_t p_end);
	float grav_center[2] = {1000, 500};
	void point_gravity(uint32_t p_start, uint32_t p_end);
	void static_friction(uint32_t p_start, uint32_t p_end);
	void fluid_friction(uint32_t p_start, uint32_t p_end);
	void vibrate(uint32_t p_start, uint32_t p_end);

	// User forces
	enum class userForce {None = 0, Translation, Rotation, Vortex};
	userForce appliedForce = userForce::None;
	float user_point[2]; //< Used so the user can point a place in the world and interact with the simulation
	float translation_force = 3000;
	float rotation_force = 1000;
	float range = 200;
	bool deletion_order = false; //< If delete_range deletion should be called
	void attraction(uint32_t p_start, uint32_t p_end);
	void rotation(uint32_t p_start, uint32_t p_end);
	void vortex(uint32_t p_start, uint32_t p_end);


	// Creation / deletion of particles
	/**
	* @brief Delete all particles in a circle of center [x, y] in the radius range_.
	*/
	void delete_range(float x, float y, float range_);
	/**
	* @warning Not thread-safe. Only 1 thread should call this function at a time.
	*/
	void delete_particle(uint32_t p);
	/**
	* @warning Not thread-safe. Only 1 thread should call this function at a time.
	*/
	uint32_t create_particle(float x, float y, float vx=0, float vy=0);
};