#pragma once

#include "Consometer.hpp"
#include "SaveLoader.hpp"
#include "Particle.hpp"
#include "World.hpp"
#include "ThreadHandler.hpp"

#include <cstdint>
#include <cstring>
#include <vector>

#define NULLPART (uint32_t)-1


class PSparam {
public :
	uint32_t n_threads = 6; //< Number of threads running the simulation.
	uint32_t max_part = 24000; //< Maximum number of Particles that can exist at a time.
	uint32_t n_part_start; //< Number of Particles that will be spawned at the start of the simulation.
	uint32_t pps; //< Number of Particles to create per second if the number of active particle is less than max_part.

	float dt; //< Delta time between to simulation frames.
	float radii; //< Radius of the Particles.
	float spawn_speed[2]; //< 2D speed at which Particles will be set when spawned or teleported.
	float temperature; //< 1D speed at which Particles will be set when spawned or teleported. During setting, a random angle is given to make the speed 2D.

	float grav_force; //< Force of the gravity whether vertical or centripetal.
	float grav_force_decay; //< How quickly the gravitational force decreases against distance in the function @see void Particle_simulator::point_gravity_invSquared(uint32_t, uint32_t)
	float grav_center[2]; //< Center of the gravitational attraction in the case of centripetal gravity.
	float static_speed_block; //< Used in static_friction. If speed < static_speed_block*dt, the Particle will stop.
	float fluid_friction_coef; //< Ratio of speed lost by Particles per simulation seconds (not by dt). Works like : speed = speed*(1-fluid_friction_coef*dt)

	// User forces
	float translation_force; //< Force applied equally on all Particles when user uses attraction. Can be used to repell when negative.
	float rotation_force; //< Force applied equally on all Particles when user uses rotation.
	float range; //< Radius of interaction for the user.

	uint8_t cs; //< collision Check Size. How far away (in cells) are collisions checked. Size of the offset for the kernel (kernel size = 2*cs+1).

	// Collision coefficients
	float pp_repulsion; //< Ratio of the radius each Particles in overlap will repell each other per dt. A ratio of 1/2 means there will a single point contact at the end of simulation step.
	float pp_repulsion_2lev; //< Ratio of the radius each Particles in less than 4 radius of each other will repell per dt. Can be used to make Particles stick (negative value) or avoid (positive value) each other.
	float pp_repulsion_phyacc; //< Force applied between Particles in overlap that aren't getting closer in the function @see void Particle_simulator::collision_pp_phyacc(uint32_t, uint32_t, float, float[2]).
	float pp_energy_conservation; //< Ratio of energy conserved during collision for the function @see void Particle_simulator::collision_pp_phyacc(uint32_t, uint32_t, float, float[2]).

	// Choosing applied fonctions
	bool apl_point_gravity; //< Apply centripetal force towards grav_center ?
	bool apl_point_gravity_invSquared; //< Apply centripetal force decreasing with the inverse of the square of the distance (i.e. f~1/r²) towards grav_center ?
	bool apl_gravity; //< Apply uniform downward force ?
	bool apl_vibrate; //< Make Particles vibrate ?
	bool apl_fluid_friction; //< Apply a fluid friction on the Particles (f~-v) ?
	bool apl_static_friction; //< Apply static friction on the Particles ? Then if a Particle's speed goes under static_speed_block, it will instantly loose all speed.
	
	bool apl_pp_collision; //< Apply Particle to Particle collision ?
	bool apl_ps_collision; //< Apply Particle to Segment collision ?
	bool apl_world_border; //< Apply Particle to world border collision ? If yes the Particles will always stay inbound of the world.
	bool apl_zone; //< Apply Particle to Zone interaction ? If yes, the zone's function will be called when a Particle enters it.

	uint8_t pp_collision_fun; //< Particle to Particle collision function. @see Particle_simulator::pp_collision_t .
	uint8_t ps_collision_fun; //< Particle to Segment  collision function. @see Particle_simulator::ps_collision_t .
	uint8_t world_border_fun; //< Particle to world border collision function. @see Particle_simulator::world_border_t .

	static PSparam Default;

	bool operator==(const PSparam& other) {return std::memcmp(this, &other, sizeof(PSparam)) == 0;};
};


class Particle_simulator {
private :
	// particles and segments
	double time[2] = {0, 0};
	uint32_t nb_max_part;
	uint32_t nb_active_part; //< Number of Particles being simulated and displayed
	std::vector<Particle> particle_array;
	uint32_t used_n_threads = 1; //< Number of threads used for simulation. Copied-out of parameters to keep it private.

	// Perfomance check
	Consometre conso; //< Used to measure and display performances of the simulation
	Consometre conso2; //< Used to measure and display performances of the simulation

	// Simulation multi-threading
	ThreadHandler threadHandler;
	SaveLoader* partLoader = nullptr;

	SLinfoPos SLI;
	bool finished_loading = false;

	// Collision function enum & pointers
public :
	enum class pp_collision_t : uint8_t{BASE = 0, TLEV, PHYACC, COHERENT};
	enum class ps_collision_t : uint8_t{BASE = 0, REBOUND};
	enum class world_border_t : uint8_t{BASE = 0, REBOUND};
private :
	using pp_collision_sign = void (Particle_simulator::*)(uint32_t p1, uint32_t p2, float dist, float vec[2]);
	void (Particle_simulator::*pp_collision_ptr)(uint32_t p_start, uint32_t p_end) = nullptr;
	void (Particle_simulator::*ps_collision_internal_ptr)(uint32_t p, float PC[2]) = nullptr;
	void (Particle_simulator::*world_borders_ptr)(uint32_t p_start, uint32_t p_end) = nullptr;

public :
	inline const Particle* get_particle_data() {return particle_array.data();};
	inline uint32_t get_max_part() {return particle_array.capacity();};
	inline uint32_t get_active_part() {return nb_active_part;};
	inline Particle& operator[](uint32_t index) {return particle_array[index];};
	inline double get_time() {return time[0];};

	World& world;

	// Orders to give to the simulator
	bool simulate = false;
	bool paused = false;
	bool step = false;
	bool quickstep = false;

	// Simulator parameters
	void setParameters(PSparam& parameters);
	PSparam params; //< Parameters of the simulation. They can be changed on the fly without causing any problem.

	// User forces
	enum class userForce {None = 0, Translation, Rotation, Vortex};
	userForce appliedForce = userForce::None;
	float user_point[2]; //< Used so the user can point a place in the world and interact with the simulation.
	bool deletion_order = false; //< If delete_range deletion should be called.
	bool reinitialize_order = false; //< If initialize_particles should be called.

public :
	/**
	* @brief Constructor. Initializes world, particle_array and seg_array.
	*/
	Particle_simulator(World& world_, PSparam& parameters = PSparam::Default, SLinfoPos SLmode_ = SLinfoPos::Lazy());


	/**
	* @brief Destructor. Stops the simulation. Stop and joins the simulation threads if created.
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
	* @details Little disclaimer : writing this function as a template increases the executable size very slightly (about 0.2kB for 3 template implementation) but is also necessary to change the collision handling at runtime without sacrificing efficiency.  
	* @see collision_pp_base
	* @see collision_pp_2lev
	* @see collision_pp_phyacc
	* @warning The implementation shouldn't be thread-safe, but this never caused a problem and solving this could decrease performance. I'll see to it later.
	*/
	template<pp_collision_sign collision_handler>
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
	void collision_pp_base(uint32_t p1, uint32_t p2, float dist, float vec[2]);

	/**
	* @brief Applies collision between p1 and p2.
	* @details Has the basic repelling collision applied from 0 to 2 radius, added with an second level linearly decreasing force applied from 0 to 4 radius between Particles.
	* This function generally needs cs to be at least 2.
	* @see collision_pp_base
	*/
	void collision_pp_2lev(uint32_t p1, uint32_t p2, float dist, float vec[2]);

	/**
	* @brief Applies collision between p1 and p2.
	* @details Applies a "physically accurate" elastic collision, as in energy and momentum are as conserved as possible considering floating point precision.
	* If the p1 and p2 are already inside each other applies an elastic repulsive force.
	* @see collision_pp_base
	*/
	void collision_pp_phyacc(uint32_t p1, uint32_t p2, float dist, float vec[2]);

	void collision_pp_coherent(uint32_t p1, uint32_t p2, float dist, float vec[2]);

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
	inline void check_collision_ps(uint32_t p, uint16_t s); // the order ps or sp doesn't matter

	/**
	* @brief Handles a collision between the Particle p and the collision point.
	* @details Changes the Particle's speed so that it will be at most in tangential contact with the point.
	* @param p Index of the Particle in particle_array.
	* @param PC Vector from Particle to Collision point.
	*/
	void collision_ps_base(uint32_t p, float PC[2]);

	/**
	* @brief Handles a collision between the Particle p and the collision point.
	* @details Changes the Particle's speed so that it will bounce off the point. In other words, the speed normal to PC will be reversed.
	* This conserves energy, though momentum is changed.
	* @param p Index of the Particle in particle_array.
	* @param PC Vector from Particle to Collision point.
	*/
	void collision_ps_rebound(uint32_t p, float PC[2]);

	/**
	* @brief Keeps Particles in [p_start, p_end[ inside the world borders.
	* @details If the Particle is expected to be even slightly outside after moving, its speed is changed so that it will be at most in tangential contact with the border.
	*/
	void world_borders_base(uint32_t p_start, uint32_t p_end);

	/**
	* @brief Keeps Particles in [p_start, p_end[ inside the world borders.
	* @details If the Particle is expected to be even slightly outside after moving, its speed normal to the border is reversed.
	* This conserves energy, though momentum is changed.
	*/
	void world_borders_rebound(uint32_t p_start, uint32_t p_end);

	/**
	* @brief Checks Particles in [p_start, p_end[ if they are in a one of the world's zone.
	* @details If there is a contact between the Particle and the Zone, calls the Particle to Zone interaction function @see void zone_functions(uint32_t, Zone&) .
	* As it goes through Particles rather than Zone cells it is better to call this function when there are more Zone Cells than Particles.
	* This is a sister function of @see void comparison_zp(uint32_t, uint32_t, uint16_t), framed differently for better performance in the aforementioned case.
	*/
	void comparison_pz(uint32_t p_start, uint32_t p_end);
	/**
	* @brief Checks the cells of the zone zone_num  for the presence of a Particle.
	* @details If there is a contact between the Particle and the Zone, calls the Particle to Zone interaction function @see void zone_functions(uint32_t, Zone&) .
	* The Zone's Cells checked are in the rectangle described by the entire width of the Zone, and in [c_start, c_end[ along the length.
	* As it goes through Zone cells rather than Particles it is better to call this function when there are more Particles than Zone Cells.
	* This is a sister function of @see void comparison_pz(uint32_t, uint32_t), framed differently for better performance in the aforementioned case.
	*/
	void comparison_zp(uint32_t c_start, uint32_t c_end, uint16_t zone_num);
	/**
	* @brief Handles the interaction between the Particle p and the Zone zone.
	* @details This will call specific functions depending on the Zone .fun member.
	*/
	void zone_functions(uint32_t p, Zone& zone);
	
	/**
	* @brief Applies world_borders_rebound on the left, top and bottom borders.
	* Teleports the Particles leaving by the right border to the left one.
	* @warning It works but doesn't complement well with collisions_base yet.
	* @see world_borders_rebound
	*/
	// void world_loop(uint32_t p_start, uint32_t p_end);

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
	* @brief Adds and initialzes n_particles in the Particle array.
	* @details It will add at most n_particles, but won't make @see nb_active_part greater than @see nb_max_part so as not to make @see particle_array overflow.
	* @return The new number of Particles if a Particle has been added. NULLPART otherwise.
	* @warning Not thread-safe. Only 1 thread should call this function at a time.
	*/
	uint32_t create_particles(uint32_t n_particles);

	/**
	* @brief Sets the speed and position of the Particle part.
	* @param part Index of the Particle to initialize.
	* @param pos_rect Rectangle in which the Particle will be randomly positioned.
	*/
	void particle_init(uint32_t part, Rectangle pos_rect);

	float get_average_loop_time() { return conso.get_average_perf_now(); };

	bool isLoading() {return SLI.isLoadPos();};
	/**
	* @brief Loads the next Particle positions/speeds from the loading file. If @see reinitialize_order, then positions are set back to their initial states.
	* @return True if there is no more positions to read from the loading file (i.e. if finished loading). False otherwise.
	* @see bool isDonePosLoading()
	*/
	bool load_next_positions();

	inline bool isDonePosLoading() {return finished_loading;};
	/**
	* @brief Resets position loading to the start.
	*/
	void resetPosLoading();

	/**
	@return true if Particle are being loaded without speed (due to compression), false otherwise.
	*/
	bool HasNoSpeed() { return SLI.isCompOutSpeed() && SLI.load_pos; };
};