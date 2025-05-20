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
private :
	// particles and segments
	float time[2] = {0, 0};
	uint32_t nb_active_part = NB_PART; //< Number of Particles being simulated and displayed

	// Perfomance check
	Consometre conso; //< Used to measure and display performances of the simulation
	Consometre conso2; //< Used to measure and display performances of the simulation

	// Simulation multi-threading
	ThreadHandler threadHandler;

public :
	inline uint32_t get_active_part() {return nb_active_part;};
	Particle particle_array[NB_PART];

	float radii = 2;
	World world;

	// Orders to give to the simulator
	bool simulate = true;
	bool paused = false;
	bool step = false;
	bool quickstep = false;

	// ======== PARAMETERS ========
	void setDefaultParameters();
	// Those can be changed on the fly without causing any problem
	float masses;
	float dt;
	float spawn_speed[2];

	float grav_force;
	float grav_force_decay;
	float grav_center[2];
	float static_speed_block; //< Used in static_friction. Speed under which a Particle will stop.
	float fluid_friction_coef;

	// User forces
	enum class userForce {None = 0, Translation, Rotation, Vortex};
	userForce appliedForce;
	float user_point[2]; //< Used so the user can point a place in the world and interact with the simulation.
	float translation_force;
	float rotation_force;
	float range; // Radius of interaction for the user.
	bool deletion_order = false; //< If delete_range deletion should be called.

	uint8_t cs; //< collision Check Size. Size of the offset for the kernel (kernel size = 2*cs+1).

	// Collision coefs - To be added (when I have some sort of UI to change parameters during simulation)
	// For now these coefs stay in the functions where they are used.

	// ======== END PARAMETERS ========


public :
	/**
	* @brief Constructor. Initializes world, particle_array and seg_array.
	*/
	Particle_simulator();

	/**
	* @brief Destructor. Stops the simulation. Stop and joins the simulation threads if created .
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
	* @brief Check closeness between Particles in [p_start, p_end[ and every Particle.
	* Calls a collision function (like collision_pp_base) for actual collision handling.
	* @see collision_pp_base
	*/
	void collision_pp(uint32_t p_start, uint32_t p_end);

	/**
	* @brief Check closeness between Particles in [p_start, p_end[ and every Particle around it using the world's grid.
	* Calls a collision function (like collision_pp_base) for actual collision handling.
	* @see collision_pp_base
	* @see collision_pp_glue
	* @see collision_pp_pĥyacc
	* @warning The implementation shouldn't be thread-safe, but this never caused a problem and solving this could decrease performance. I'll see to it later.
	*/
	void collision_pp_grid(uint32_t p_start, uint32_t p_end);

	/**
	* @brief Applies collision between p1 and p2.
	* @details This is the base repelling collision.
	* @param p1 index of the first Particle.
	* @param p2 index of the second Particle (ensure p1!=p2).
	* @param dist distance between the Particles p1 and p2.
	* @param vec vector from p1's position to p2's.
	* @warning The implementation shouldn't be thread-safe, but this never caused a problem and solving this could decrease performance. I'll see to it later.
	*/
	inline void collision_pp_base(uint32_t p1, uint32_t p2, float dist, float vec[2]);

	/**
	* @brief Applies collision between p1 and p2.
	* @details Has a basic repelling collision added with an attractive force between Particles.
	* @see collision_pp_base
	*/
	inline void collision_pp_glue(uint32_t p1, uint32_t p2, float dist, float vec[2]);

	/**
	* @brief Applies collision between p1 and p2.
	* @details Applies a "physically accurate" elastic collision, as in energy and momentum are as conserved as possible considering floating point precision.
	* If the p1 and p2 are already inside each other applies an elastic repulsive force.
	* @see collision_pp_base
	*/
	inline void collision_pp_pĥyacc(uint32_t p1, uint32_t p2, float dist, float vec[2]);

	/**
	* @brief Check and apply collision between Particles in [p_start, p_end[ and every Segment in seg_array.
	*/
	void collision_pl(uint32_t p_start, uint32_t p_end);

	/**
	* @brief Goes through Particles in [p_start, p_end[ to check if any Segment is nearby using the world grid.
	* Calls the Particle to Segment collision detection check_collision_ps .
	* @details As it goes through Particles rather than Segment cells it is better to call this function when there are more Segment Cell than Particles.
	* @warning Only call this function if the world has a grid for Segments (i.e. if world.segments_in_grid = true).
	* @see check_collision_ps
	*/
	void comparison_ps_grid(uint32_t p_start, uint32_t p_end);

	/**
	* @brief Goes through Segments cells in [c_start, c_end[ to check if any Particle is nearby using the world grid.
	* @details As it goes through Segment cells rather than particles it is better to call this function when there are more Particles than Segment cells (which is often the case).
	* @param c_start Segment cell index to start (included).
	* @param c_end Segment cell index to end (excluded).
	* @param seg_num Segment on which this function shall work.
	* @warning Only call this function if the Segments have their cells (i.e. if world.segments_in_grid = false).
	* @see check_collision_ps
	*/
	void comparison_sp_grid(uint32_t c_start, uint32_t c_end, uint16_t seg_num);

	/**
	* @brief Calculates the closest point on the Segment s from the Particle p's next position.
	* Then calls a collision handling function like collision_ps_base.
	* @see collision_ps_base
	* @see collision_ps_rebound
	*/
	inline void check_collision_ps(uint32_t p, uint16_t s); // the order doesn't matter

	/**
	* @brief Handles a collision between the Particle p and the collision point.
	* @details Changes the Particle's speed so that it will be at most in tangential contact with the point.
	* @param p Index of the Particle in particle_array.
	* @param PC Vector from Particle to Collision point.
	*/
	inline void collision_ps_base(uint32_t p, float PC[2]);

	/**
	* @brief Handles a collision between the Particle p and the collision point.
	* @details Changes the Particle's speed so that it will bounce off the point. In other words, the speed normal to PC will be reversed.
	* This conserves energy, though momentum is changed.
	* @param p Index of the Particle in particle_array.
	* @param PC Vector from Particle to Collision point.
	*/
	inline void collision_ps_rebound(uint32_t p, float PC[2]);

	/**
	* @brief Keeps Particles in [p_start, p_end[ inside the world borders.
	* @details If the Particle is expected to be even slightly outside after moving, its speed is changed so that it will be at most in tangential contact with the border.
	*/
	void world_borders(uint32_t p_start, uint32_t p_end);

	/**
	* @brief Keeps Particles in [p_start, p_end[ inside the world borders.
	* @details If the Particle is expected to be even slightly outside after moving, its speed normal to the border is reversed.
	* This conserves energy, though momentum is changed.
	*/
	void world_borders_rebound(uint32_t p_start, uint32_t p_end);
	
	/**
	* @brief Applies world_borders_rebound on the left, top and bottom borders.
	* Teleports the Particles leaving by the right border to the left one.
	* @warning It works but doesn't complement well with collisions_base yet.
	* @see world_borders_rebound
	*/
	void world_loop(uint32_t p_start, uint32_t p_end);

	/**
	* @brief Deletes every Particle of which position is NaN.
	* @warning Not yet threadable.
	*/
	void delete_NaNs(uint32_t p_start, uint32_t p_end);
	
	// General forces
	void gravity(uint32_t p_start, uint32_t p_end);
	void point_gravity(uint32_t p_start, uint32_t p_end);
	void point_gravity_invSquared(uint32_t p_start, uint32_t p_end);
	void static_friction(uint32_t p_start, uint32_t p_end);
	void fluid_friction(uint32_t p_start, uint32_t p_end);
	void vibrate(uint32_t p_start, uint32_t p_end);

	// User forces
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

	float get_average_loop_time() { return conso.get_average_perf_now(); };
};