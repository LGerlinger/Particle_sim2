#include "Particle_simulator.hpp"
#include "Segment.hpp"
#include "utilities.hpp"

#include <cmath>
#include <cstdlib>
#include <iostream>
#include <thread>

PSparam PSparam::Default {
	Default.n_threads = 6,
	Default.max_part = 24000,
	Default.n_part_start = PSparam::Default.max_part,
	Default.pps = 1000,

	Default.dt = 0.001,
	Default.radii = 2,
	Default.spawn_speed[0] = 0,
	Default.spawn_speed[1] = 0,
	Default.temperature = 0,

	Default.grav_force = 1000,
	Default.grav_force_decay = 0.01f,
	Default.grav_center[0] = WorldParam::Default.size[0]/2,
	Default.grav_center[1] = WorldParam::Default.size[1]/2,
	Default.static_speed_block = 1,
	Default.fluid_friction_coef = 0.3,

	// User forces
	Default.translation_force = 3000,
	Default.rotation_force = 1000,
	Default.range = 200,

	Default.cs = 1,

	Default.pp_repulsion = 0.45f,
	Default.pp_repulsion_2lev = 1000,
	Default.pp_repulsion_phyacc = 5000,
	Default.pp_energy_conservation = 1,

	Default.apl_point_gravity = false,
	Default.apl_point_gravity_invSquared = false,
	Default.apl_gravity = true,
	Default.apl_vibrate = false,
	Default.apl_fluid_friction = false,
	Default.apl_static_friction = false,
	
	Default.apl_pp_collision = true,
	Default.apl_ps_collision = true,
	Default.apl_world_border = true,
	Default.apl_zone = true,

	Default.pp_collision_fun = (uint8_t)Particle_simulator::pp_collision_t::BASE,
	Default.ps_collision_fun = (uint8_t)Particle_simulator::ps_collision_t::BASE,
	Default.world_border_fun = (uint8_t)Particle_simulator::world_border_t::BASE,
};




Particle_simulator::Particle_simulator(World& world_, PSparam& parameters, SLinfoPos SLI_) : world(world_), SLI(SLI_) {
	std::cout << "Particle_simulator::Particle_simulator()" << std::endl;
	if (world.getCellSize(0) <= 2*parameters.radii || world.getCellSize(1) <= 2*parameters.radii) { // Capping radii at half a cellsize, so a Particle can't be bigger than a Cell
		parameters.radii = std::min(world.getCellSize(0), world.getCellSize(1))/2;
	}
	parameters.pp_energy_conservation = std::max(parameters.pp_energy_conservation, 0.5f); // Below 0.5 this causes some calculations to NaN the Particles.
	setParameters(parameters);
	nb_max_part = parameters.max_part;
	used_n_threads = parameters.n_threads;
	particle_array.reserve(nb_max_part);
	particle_array.resize(nb_max_part);
	initialize_particles();

	world.chg_seg_store_sys(nb_active_part);

	if (SLI.isSavePos() || SLI.isLoadPos()) {
		partLoader = new SaveLoader;
		if (SLI.isSavePos()) {
			partLoader->prepareSavePos(30, FileHandler::GB, SLI, nb_max_part, world, params.dt);
		}
		if (SLI.isLoadPos()) {
			conso.start_perf_check("loading time", 10000);
			partLoader->prepareLoadPos(nb_max_part, SLI);
		}
	}

	std::cout << "\t" << i2s(world.n_cell_seg) << " cells with a segment" << std::endl;
	std::cout << "\t" << i2s(nb_max_part) << " particles" << std::endl;
	std::cout << "\tsize Particle array : " << i2s(nb_max_part*sizeof(Particle)) << " bytes" << std::endl;
	// std::cout << "End Particle_simulator::Particle_simulator()" << std::endl;
}


Particle_simulator::~Particle_simulator() {
	std::cout << "Particle_simulator::~Particle_simulator()" << std::endl;
	simulate = false;
	stop_simulation_threads();

	if (partLoader) delete partLoader;
}

void Particle_simulator::setParameters(PSparam& InParameters) {
	params = InParameters;

	switch (InParameters.pp_collision_fun) {
		case (uint8_t)Particle_simulator::pp_collision_t::TLEV :
			pp_collision_ptr = &Particle_simulator::collision_pp_grid<&Particle_simulator::collision_pp_2lev>;
			break;
		case (uint8_t)Particle_simulator::pp_collision_t::PHYACC :
			pp_collision_ptr = &Particle_simulator::collision_pp_grid<&Particle_simulator::collision_pp_phyacc>;
			break;
		case (uint8_t)Particle_simulator::pp_collision_t::COHERENT :
			pp_collision_ptr = &Particle_simulator::collision_pp_grid<&Particle_simulator::collision_pp_coherent>;
			break;
		default :
			pp_collision_ptr = &Particle_simulator::collision_pp_grid<&Particle_simulator::collision_pp_base>;
			break;
	}

	switch (InParameters.ps_collision_fun) {
		case (uint8_t)Particle_simulator::ps_collision_t::REBOUND :
			ps_collision_internal_ptr = &Particle_simulator::collision_ps_rebound;
			break;
		default :
			ps_collision_internal_ptr = &Particle_simulator::collision_ps_base;
			break;
	}

	switch (InParameters.world_border_fun) {
		case (uint8_t)Particle_simulator::world_border_t::REBOUND :
			world_borders_ptr = &Particle_simulator::world_borders_rebound;
			break;
		// case (uint8_t)Particle_simulator::world_border_t::LOOP :
		// 	world_borders_ptr = &Particle_simulator::world_loop;
		// 	break;
		default :
			world_borders_ptr = &Particle_simulator::world_borders_base;
			break;
	}
}

void Particle_simulator::initialize_particles() {
	uint16_t x=0,y=0;
	nb_active_part = std::min(params.n_part_start, params.max_part);
	for (uint32_t i=0; i<nb_active_part; i++) {
		// Spawning in an ordered rectangle formation
		// particle_array[i].position[0] = x* world.getCellSize(0) + params.radii;
		// particle_array[i].position[1] = y* world.getCellSize(1) + params.radii;
		// if (++x==world.getGridSize(0)) {
		// 	x = 0;
		// 	y++;
		// }

		// random spawning
		// particle_init(i, world.getSpawnRect());
		particle_init(i, Rectangle(2*params.radii, 2*params.radii, world.getSize(0) -4*params.radii, world.getSize(1) -4*params.radii));
		
		// For collision testing purposes
		// particle_array[i].position[0] = i ? 30 : 10;
		// particle_array[i].position[1] = 100;
		// particle_array[i].speed[0] = i ? -500 : 0;
		// particle_array[i].speed[1] = i ? 0 : 0;
	}
}


void Particle_simulator::start_simulation_threads() {
	std::cout << "Particle_simulator::start_simulation_threads()" << std::endl;
	if (!simulate) {
		threadHandler.set_nb_fun(16+world.seg_array.size());
		simulate = true;
		// threadHandler.give_new_thread(new std::thread(&Particle_simulator::simulation_thread2, this, 0));
		for (uint8_t i=0; i<std::min((uint32_t)used_n_threads, nb_max_part); i++) {
			threadHandler.give_new_thread(new std::thread(&Particle_simulator::simulation_thread, this, i));
		}
	}
	else {
		std::cout << "Simulation already running" << std::endl;
	}
}

void Particle_simulator::stop_simulation_threads() {
	// std::cout << "Particle_simulator::stop_simulation_threads()" << std::endl;
	simulate = false;
	threadHandler.wait_for_threads_end();
}


void Particle_simulator::simulation_thread(uint8_t th_id) {
	// std::cout << "Particle_simulator::simulation_thread(" << th_id << ")" << std::endl;
	uint32_t nppt; // number of particles per thread
	uint32_t sub_nppt;
	uint8_t num_fun;

	threadHandler.synchronize_last(used_n_threads, 1, this, &Particle_simulator::pause_wait); // This is here only to make so the simulation can start paused before looping once.

	if (!th_id) {
		conso.start_perf_check("average sim loop", 20000);
		conso2.start_perf_check("Collision particles", 20000);
	}
	while (simulate) {
		num_fun = 0;
		// Synchronize & do stuff that shouldn't be done by multiple threads, like changing the number of Particles
		threadHandler.synchronize_last(used_n_threads, 1, this, &Particle_simulator::create_destroy_wait);
		nppt = nb_active_part/used_n_threads; // number of particles per thread +- 1
		sub_nppt = std::max(nppt/5, (uint32_t)10);
		
		// Filling the grid
		if (params.apl_pp_collision || params.apl_ps_collision) {
			auto bound_update_grid_particle_contenance = std::bind(&World::update_grid_particle_contenance, &world, particle_array.data(), std::placeholders::_1, std::placeholders::_2, params.dt);
			threadHandler.load_repartition(bound_update_grid_particle_contenance, num_fun++, nb_active_part, sub_nppt);
		}
			

		// Simulation -- applying forces and collisions
		switch (appliedForce) {
			case userForce::None:
				break;
			case userForce::Translation:
				threadHandler.load_repartition(this, &Particle_simulator::attraction, num_fun++, nb_active_part, sub_nppt);
				break;
			case userForce::Rotation:
				threadHandler.load_repartition(this, &Particle_simulator::rotation, num_fun++, nb_active_part, sub_nppt);
				break;
			case userForce::Vortex:
				threadHandler.load_repartition(this, &Particle_simulator::vortex, num_fun++, nb_active_part, sub_nppt);
				break;
		}

		if (params.apl_point_gravity)
			threadHandler.load_repartition(this, &Particle_simulator::point_gravity, num_fun++, nb_active_part, sub_nppt);
		if (params.apl_point_gravity_invSquared)
			threadHandler.load_repartition(this, &Particle_simulator::point_gravity_invSquared, num_fun++, nb_active_part, sub_nppt);
		if (params.apl_gravity)
			threadHandler.load_repartition(this, &Particle_simulator::gravity, num_fun++, nb_active_part, sub_nppt);
		if (params.apl_vibrate)
			threadHandler.load_repartition(this, &Particle_simulator::vibrate, num_fun++, nb_active_part, sub_nppt);
		if (params.apl_fluid_friction)
			threadHandler.load_repartition(this, &Particle_simulator::fluid_friction, num_fun++, nb_active_part, sub_nppt);


		if (!th_id) conso2.Start();
		if (params.apl_pp_collision)
			threadHandler.load_repartition(this, pp_collision_ptr, num_fun++, nb_active_part, sub_nppt);
		if (!th_id) conso2.Tick_fine(true);

		if (params.apl_ps_collision) {
			if (world.sig()) {
				threadHandler.load_repartition(this, &Particle_simulator::comparison_ps_grid, num_fun++, nb_active_part, sub_nppt);
			} else {
				for (uint16_t i=0; i<world.seg_array.size(); i++) {
					auto bound_collision_pl_grid = std::bind(&Particle_simulator::comparison_sp_grid, this, std::placeholders::_1, std::placeholders::_2, i);
					auto work_set = world.seg_array[i].cells.size();
					threadHandler.load_repartition(bound_collision_pl_grid, num_fun++, work_set, work_set/(5*used_n_threads));
				}
			}
		}
		
		if (params.apl_world_border)
			threadHandler.load_repartition(this, world_borders_ptr, num_fun++, nb_active_part, sub_nppt);

		if (params.apl_static_friction)
			threadHandler.load_repartition(this, &Particle_simulator::static_friction, num_fun++, nb_active_part, sub_nppt);

		if (params.apl_zone) {
			if (nb_active_part < world.getNZoneCoveredCells()) {
				threadHandler.load_repartition(this, &Particle_simulator::comparison_pz, num_fun++, nb_active_part, sub_nppt);
			} else {
				for (uint16_t i=0; i<world.getNbOfZones(); i++) {
					auto bound_comparison_zp = std::bind(&Particle_simulator::comparison_zp, this, std::placeholders::_1, std::placeholders::_2, i);
					auto work_set = world.getZone(i).getLength(); // work set along the length of the zone
					threadHandler.load_repartition(bound_comparison_zp, num_fun++, work_set, work_set/(5*used_n_threads));
				}
			}
		}

		// updating position
		threadHandler.synchronize_last(used_n_threads, 1, this, &Particle_simulator::pause_wait);
		threadHandler.load_repartition(this, &Particle_simulator::update_pos, num_fun++, nb_active_part, sub_nppt);

		// Removing the particles from the grid, thus preparing for the next loop
		if (params.apl_pp_collision || params.apl_ps_collision) {
			if (world.emptying_blindly()) threadHandler.load_repartition(&world, &World::empty_grid_particle_blind, num_fun++, world.getGridSize(1), world.getGridSize(1)/(2*used_n_threads));
			else {
				threadHandler.load_repartition(&world, &World::empty_grid_particle_pbased, num_fun++, nb_active_part, sub_nppt);
			}
		}

	}
}

void Particle_simulator::simulation_thread2(uint8_t th_id) {
	// std::cout << "Particle_simulator::simulation_thread()" << std::endl;
	uint32_t nppt; // number of particles per thread
	uint32_t p_start, p_end; // indices of the [first, last[ Particle of the thread.
	if (!th_id) {
		conso.start_perf_check("average sim loop", 20000);
		conso2.start_perf_check("update world grid", 20000);
	}
	while (simulate) {
		// Synchronisation
		threadHandler.synchronize_last(used_n_threads, 1, this, &Particle_simulator::loop_wait);

		// Simulation -- applying forces and collisions
		nppt = nb_active_part/used_n_threads; // number of particles per thread +- 1
		p_start = nppt *th_id       + std::min(th_id, (uint8_t)(nb_active_part%used_n_threads));
		p_end =   nppt *(th_id+1)   + std::min(th_id, (uint8_t)(nb_active_part%used_n_threads)) + (th_id < (uint8_t)(nb_active_part%used_n_threads));

		// if (params.apl_pp_collision || params.apl_ps_collision) {
			if (world.emptying_blindly()) {
				uint16_t c_start = world.getGridSize(1)/used_n_threads *th_id + std::min(th_id, (uint8_t)(world.getGridSize(1)%used_n_threads));
				uint16_t c_end =   world.getGridSize(1)/used_n_threads *(th_id+1)   + std::min(th_id, (uint8_t)(world.getGridSize(1)%used_n_threads)) + (th_id < (uint8_t)(world.getGridSize(1)%used_n_threads));
				world.empty_grid_particle_blind(c_start, c_end);
			
			} else world.empty_grid_particle_pbased(p_start, p_end);
		// }

		// if (params.apl_pp_collision || params.apl_ps_collision)
			world.update_grid_particle_contenance(particle_array.data(), p_start, p_end, params.dt);

		switch (appliedForce) {
			case userForce::None:
				break;
			case userForce::Translation:
				attraction(p_start, p_end);
				break;
			case userForce::Rotation:
				rotation(p_start, p_end);
				break;
			case userForce::Vortex:
				vortex(p_start, p_end);
				break;
		}

		// if (params.apl_point_gravity)
		// 	point_gravity(p_start, p_end);
		// if (params.apl_point_gravity_invSquared)
		// 	point_gravity_invSquared(p_start, p_end);
		// if (params.apl_gravity)
			gravity(p_start, p_end);
		// if (params.apl_fluid_friction)
		// 	fluid_friction(p_start, p_end);
		// if (params.apl_static_friction)
		// 	static_friction(p_start, p_end);
		// if (params.apl_vibrate)
		// 	vibrate(p_start, p_end);

		// updating speed

		// if (params.apl_pp_collision)
			(this->*pp_collision_ptr)(p_start, p_end);

		// if (params.apl_ps_collision) {
			if (world.sig()) comparison_ps_grid(p_start, p_end);
			else {
				for (uint16_t i=0; i<world.seg_array.size(); i++) {
					uint32_t nspt = world.seg_array[i].cells.size() / used_n_threads;
					uint32_t c_start= i   *nspt;
					uint32_t c_end = (i+1)*nspt;
					comparison_sp_grid(c_start, c_end, i);
				}
			}
		// }
		// if (params.apl_world_border)
			world_borders_base(p_start, p_end);

		// updating position
		update_pos(p_start, p_end);
	}
}


void Particle_simulator::loop_wait() {
	pause_wait();
	create_destroy_wait();
}

/**
* @details I had to split loop_wait in 2 so I could change where I pause the simulation relative to emptying the world grid.
*/
void Particle_simulator::pause_wait() {
	// std::cout << "pause_wait" << std::endl;
	if (SLI.isSavePos()) partLoader->savePos(particle_array.data(), nb_active_part, time[0]);
	conso.Tick_fine(true);
	while (simulate && paused && !step && !quickstep) {
		std::this_thread::sleep_for(std::chrono::milliseconds(50));
	}
	step = false;
	if (quickstep) std::this_thread::sleep_for(std::chrono::milliseconds(50));
	conso.Start();
}

/**
* @details I had to split loop_wait in 2 so I could change where I pause the simulation relative to emptying the world grid.
*/
void Particle_simulator::create_destroy_wait() {
	threadHandler.prep_new_work_loop();
	time[0] += params.dt;
	world.chg_seg_store_sys(nb_active_part);
	if (deletion_order) {
		delete_range(user_point[0], user_point[1], params.range);
	}
	if (!(rand()%4096)) {
		delete_NaNs(0, nb_active_part);
	}
	if (reinitialize_order) {
		initialize_particles();
		time[0] = 0;
		time[1] = 0;
		reinitialize_order = false;
	}
	float to_create = params.pps*params.dt;
	create_particles((uint32_t)to_create + ((float)rand()/RAND_MAX < to_create - (uint32_t)to_create));
}


void Particle_simulator::update_pos(uint32_t p_start, uint32_t p_end) {
	// std::cout << "Particle_simulator::update_pos()" << std::endl;
	for (uint32_t p=p_start; p<p_end; p++) {
		particle_array[p].position[0] += particle_array[p].speed[0] * params.dt;
		particle_array[p].position[1] += particle_array[p].speed[1] * params.dt;
	}
}


void Particle_simulator::collision_pp(uint32_t p_start, uint32_t p_end) {
	// std::cout << "Particle_simulator::collision_pp()" << std::endl;
	float vec[2];
	float dist;
	for (uint8_t i=0; i<2; i++) {
		for (uint32_t p1=p_start; p1<p_end; p1++) {

			for (uint32_t p2=p1+1; p2<nb_active_part; p2++) {
				vec[0] = particle_array[p2].position[0] + particle_array[p2].speed[0]*params.dt - particle_array[p1].position[0] - particle_array[p1].speed[0]*params.dt;
				vec[1] = particle_array[p2].position[1] + particle_array[p2].speed[1]*params.dt - particle_array[p1].position[1] - particle_array[p1].speed[1]*params.dt;
				dist = sqrt(vec[0]*vec[0] + vec[1]*vec[1]);
				
				collision_pp_base(p1, p2, dist, vec);
			}

		}
	}
}

template<Particle_simulator::pp_collision_sign collision_handler>
void Particle_simulator::collision_pp_grid(uint32_t p_start, uint32_t p_end) {
	// std::cout << "collision_pp_grid(" << p_start << ", " << p_end << ")" << std::endl;
	uint16_t dPos[2];
	float next_pos[2];
	float vec[2];
	float dist;
	uint32_t p2;
	uint16_t x, y;
	uint16_t min_x, max_x, min_y, max_y;
	for (uint32_t p1=p_start; p1<p_end; p1++) {
		next_pos[0] = particle_array[p1].position[0] + particle_array[p1].speed[0]*params.dt;
		next_pos[1] = particle_array[p1].position[1] + particle_array[p1].speed[1]*params.dt;
		x = next_pos[0] / world.getCellSize(0);
		y = next_pos[1] / world.getCellSize(1);
		// std::cout << "coord = " << x << ", " << y << std::endl;
		min_x = std::max(x-params.cs, 0);
		min_y = std::max(y-params.cs, 0);
		max_x = std::min((uint16_t)(x+params.cs+1), world.getGridSize(0));
		max_y = std::min((uint16_t)(y+params.cs+1), world.getGridSize(1));
		// std::cout << "x [" << min_x << ", " << max_x << "]   y [" << min_y << ", " << max_y << "]" << std::endl;

		for (dPos[1]=min_y; dPos[1]<max_y; dPos[1]++) {
			for (dPos[0]=min_x; dPos[0]<max_x; dPos[0]++) {
				// print_vect("dPos", dPos);
				Cell& cell = world.getCell(dPos[0], dPos[1]);
				for (uint8_t part2=0; part2<cell.nb_parts; part2++) {
					p2 = cell.parts[part2];
					if (p1 != p2) {

						vec[0] = particle_array[p2].position[0] + particle_array[p2].speed[0]*params.dt - next_pos[0];
						vec[1] = particle_array[p2].position[1] + particle_array[p2].speed[1]*params.dt - next_pos[1];
						dist = sqrt(vec[0]*vec[0] + vec[1]*vec[1]);
						// std::cout << "Comparing " << p1 << " with " << p2 << " : dits=" << dist;
						// print_vect("vec", vec);
						
						// appel fonction
						(this->*collision_handler)(p1, p2, dist, vec);

						// collision_pp_base(p1, p2, dist, vec);
						// collision_pp_2lev(p1, p2, dist, vec);
						// collision_pp_phyacc(p1, p2, dist, vec);
					}
				}
				
			}
		}
		// std::cout << std::endl;

	}
}


void Particle_simulator::collision_pp_base(uint32_t p1, uint32_t p2, float dist, float vec[2]) {
	// std::cout << "collision_pp_base\n";
	// std::cout << "radii=" << params.radii << ", repulsion = " << params.pp_repulsion << ",  dt=" << params.dt << "\n";
	if (dist < 2*params.radii) {
		float temp_coef = (2*params.radii - dist) * params.pp_repulsion /(params.dt * dist);
		vec[0] *= temp_coef;
		vec[1] *= temp_coef;

		particle_array[p1].speed[0] -= vec[0];
		particle_array[p1].speed[1] -= vec[1];
		particle_array[p2].speed[0] += vec[0];
		particle_array[p2].speed[1] += vec[1];
	}
}

void Particle_simulator::collision_pp_2lev(uint32_t p1, uint32_t p2, float dist, float vec[2]) {
	// std::cout << "collision_pp_2lev" << std::endl;
	float temp_coef;
	if (dist < 2*params.radii) {
		temp_coef = (2*params.radii - dist) * params.pp_repulsion /(params.dt * dist);
	} else
		temp_coef = 0;
	temp_coef += (dist < 4*params.radii) * (4*params.radii - dist) * params.pp_repulsion_2lev * params.dt / dist;
	vec[0] *= temp_coef;
	vec[1] *= temp_coef;

	particle_array[p1].speed[0] -= vec[0];
	particle_array[p1].speed[1] -= vec[1];
	particle_array[p2].speed[0] += vec[0];
	particle_array[p2].speed[1] += vec[1];
}

void Particle_simulator::collision_pp_phyacc(uint32_t p1, uint32_t p2, float dist, float rho[2]) {
	// std::cout << "collision_pp_phyacc" << std::endl;
	if (dist < 2*params.radii) {
		// Normalize the radial vector
		rho[0] /= dist;
		rho[1] /= dist;
		// normal tangent vector
		float tau[2] = {-rho[1], rho[0]};

		// this only serv to verify Energy and momentum are conserved. So it's commented out during actual use 
		// float Energy[2];
		// Energy[0] = (particle_array[p1].speed[0]*particle_array[p1].speed[0] + particle_array[p1].speed[1]*particle_array[p1].speed[1] +
		//              particle_array[p2].speed[0]*particle_array[p2].speed[0] + particle_array[p2].speed[1]*particle_array[p2].speed[1])/2;
		// float momentum[2][2];
		// momentum[0][0] = particle_array[p1].speed[0] + particle_array[p2].speed[0];
		// momentum[0][1] = particle_array[p1].speed[1] + particle_array[p2].speed[1];
		
		// Calculate speeds along rho and tau (changing base basically)
		// We focus on radial speed to make the collision problem 1-dimensional
		float v1[2] = // v1.rho * rho + v1.tau * tau
			{particle_array[p1].speed[0]*rho[0] + particle_array[p1].speed[1]*rho[1],
			 particle_array[p1].speed[0]*tau[0] + particle_array[p1].speed[1]*tau[1]};
		float v2[2] = // v2.rho * rho + v2.tau * tau
			{particle_array[p2].speed[0]*rho[0] + particle_array[p2].speed[1]*rho[1],
			 particle_array[p2].speed[0]*tau[0] + particle_array[p2].speed[1]*tau[1]};


		// std::cout << "v1p=" << v1[0] << ",  v2p=" << v2[0] << "   diff=" << v2[0]-v1[0] << std::endl;
		if (v2[0] - v1[0] < 0) { // if p1 and p2 are getting closer

			// Calculating Energy (E) and momentum (P)
			float E = (v1[0]*v1[0] + v2[0]*v2[0]) /2 * params.pp_energy_conservation; // E = (m1*v1*v1 + m2*v2*v2)/2 * energy_conservation_coefficient
			float P = v1[0] + v2[0]; // P = m1*v1 + m2*v2

			// Conservation of Enegy gives a second degree equation
			float A = 1; // A = m2*m2 * (1/m1 + 1/m2)/2
			float B = -P * 1; // B = -P*m2/m1
			float C = P*P/2 - E; // C = P*P/(2*m1) - E
			float delta = B*B-4*A*C; // delta > 0 <=> v1.p!=v2.p so I don't care

			// Choose a new vp so it is different from the first one
			float newVp2;
			newVp2 = (-B+sqrt(delta))/(2*A); // vp' = (-B+-sqrt(delta)) / 2A
			if (std::abs(newVp2 - v2[0]) < 0.01) { // we chose the same vp2
				// std::cout << "\tneed for change of sign" << std::endl;
				newVp2 = (-B-sqrt(delta))/(2*A);
			}
			float newVp1 = P-newVp2; // v1p' = (P -m2*v2'.p)/m1
			// print_vect("newVp", newVp);
	
			// v1 = v1p'*rho + v1t*tau
			particle_array[p1].speed[0] = newVp1*rho[0] + v1[1]*tau[0];
			particle_array[p1].speed[1] = newVp1*rho[1] + v1[1]*tau[1];
			// v2 = v2p'*rho + v2t*tau
			particle_array[p2].speed[0] = newVp2*rho[0] + v2[1]*tau[0];
			particle_array[p2].speed[1] = newVp2*rho[1] + v2[1]*tau[1];


			// Energy[1] = (particle_array[p1].speed[0]*particle_array[p1].speed[0] + particle_array[p1].speed[1]*particle_array[p1].speed[1] +
			// 						 particle_array[p2].speed[0]*particle_array[p2].speed[0] + particle_array[p2].speed[1]*particle_array[p2].speed[1])/2;
	
			// momentum[1][0] = particle_array[p1].speed[0] + particle_array[p2].speed[0];
			// momentum[1][1] = particle_array[p1].speed[1] + particle_array[p2].speed[1];
	
			// std::cout << "Avant-après" << std::endl;
			// print_vect("E", Energy);
			// print_vect("P-av", momentum[0]);
			// print_vect("P-ap", momentum[1]);
			// std::cout << std::endl;
		}

		else { // p1 and p2 are relatively either immobile or getting further
			float temp_coef = (2*params.radii - dist) * params.pp_repulsion_phyacc *params.dt;
			rho[0] *= temp_coef;
			rho[1] *= temp_coef;

			particle_array[p1].speed[0] -= rho[0];
			particle_array[p1].speed[1] -= rho[1];
			particle_array[p2].speed[0] += rho[0];
			particle_array[p2].speed[1] += rho[1];
		}
	}
}

float coherence = 100.f;
void Particle_simulator::collision_pp_coherent(uint32_t p1, uint32_t p2, float dist, float vec[2]) {
	// std::cout << "collision_pp_2lev" << std::endl;
	if (dist == 0) return;
	float temp_coef;
	if (dist < 2*params.radii) temp_coef = (2*params.radii - dist) * params.pp_repulsion;
	else temp_coef = 0;

	float dist_apl = (dist < 4*params.radii) * (4*params.radii - dist);
	temp_coef += dist_apl * params.pp_repulsion_2lev;
	temp_coef *= params.dt;
	vec[0] *= temp_coef;
	vec[1] *= temp_coef;

	float speed_dif[2] = {
		particle_array[p2].speed[0] - particle_array[p1].speed[0],
		particle_array[p2].speed[1] - particle_array[p1].speed[1]
	};
	float coherence_multiplier = coherence * params.dt * dist_apl;
	speed_dif[0] *= coherence_multiplier;
	speed_dif[1] *= coherence_multiplier;

	particle_array[p1].speed[0] -= vec[0] - speed_dif[0];
	particle_array[p1].speed[1] -= vec[1] - speed_dif[1];
	particle_array[p2].speed[0] += vec[0] - speed_dif[0];
	particle_array[p2].speed[1] += vec[1] - speed_dif[1];
}



void Particle_simulator::collision_pl(uint32_t p_start, uint32_t p_end) {
	// std::cout << "Particle_simulator::collision_pl()" << std::endl;
	for (uint32_t s=0; s<world.seg_array.size(); s++) {
		for (uint32_t p=p_start; p<p_end; p++) {
			check_collision_ps(p, s);
		}
	}
}

void Particle_simulator::comparison_ps_grid(uint32_t p_start, uint32_t p_end) {
	// std::cout << "Particle_simulator::comparison_ps_grid()" << std::endl;
	uint16_t x, y;
	for (uint32_t p=p_start; p<p_end; p++) {
		if (world.getCellCoord_fromPos(particle_array[p].position[0], particle_array[p].position[1], &x, &y)) {
			Cell_seg& cell = world.getCell_seg(x, y);
			for (uint8_t seg=0; seg<cell.nb_segs; seg++) {
				check_collision_ps(p, cell.segs[seg]);
			}
		}
	}
}

void Particle_simulator::comparison_sp_grid(uint32_t c_start, uint32_t c_end, uint16_t seg_num) {
	// std::cout << "Particle_simulator::comparison_sp_grid(" << c_start << ", " << c_end << ", " << seg_num << ")" << std::endl;
	Segment& segment = world.seg_array[seg_num];
	for (uint32_t c=c_start; c<c_end; c++) {
		Cell& cell = world.getCell(segment.cells[c][0], segment.cells[c][1]);
		for (uint8_t p_c=0; p_c<cell.nb_parts; p_c++) {
			check_collision_ps(cell.parts[p_c], seg_num);
		}
	}
}

void Particle_simulator::check_collision_ps(uint32_t p, uint16_t s) {
	Segment& seg = world.seg_array[s];
	float part_next_pos[2] = {
		part_next_pos[0] = particle_array[p].position[0] + particle_array[p].speed[0]*params.dt,
		part_next_pos[1] = particle_array[p].position[1] + particle_array[p].speed[1]*params.dt
	};

	float AP[2] = {
		AP[0] = part_next_pos[0] - seg.pos[0][0],
		AP[1] = part_next_pos[1] - seg.pos[0][1]
	};
	
	// AC = (AP.AB)/||AB||² * AB = [(AP.AB)/||AB|| * AB/||AB||]
	float scal = AP[0]*seg.vect_norm[0] + AP[1]*seg.vect_norm[1];
	float vec[2] = {
		vec[0] = scal * seg.vect_norm[0],
		vec[1] = scal * seg.vect_norm[1]
	};

	// Calculating OC : OC = OA + AC
	vec[0] = vec[0] + seg.pos[0][0];
	vec[1] = vec[1] + seg.pos[0][1];

	if (seg.min[0] <= vec[0] && vec[0] <= seg.max[0] && seg.min[1] <= vec[1] && vec[1] <= seg.max[1]) { // C is in [AB]
		// Calculating PC : PC = PO + OC = OC - OP
		vec[0] = vec[0] - part_next_pos[0];
		vec[1] = vec[1] - part_next_pos[1];
		(this->*ps_collision_internal_ptr)(p, vec);
		// collision_ps_base(p, vec);
		// collision_ps_rebound(p, vec);
	}
	else { // C is not in [AB] but the particle might still be near the edges A or B
		for (uint8_t i=0; i<2; i++) {
			// The collision point could then be an end of the Segment
			vec[0] = seg.pos[i][0] - part_next_pos[0];
			vec[1] = seg.pos[i][1] - part_next_pos[1];
			(this->*ps_collision_internal_ptr)(p, vec);
			// collision_ps_base(p, vec);
			// collision_ps_rebound(p, vec);
		}
	}
}

void Particle_simulator::collision_ps_base(uint32_t p, float PC[2]) {
	float scal = sqrt(PC[0]*PC[0] + PC[1]*PC[1]); // ||PC||

	if (scal < params.radii) { // Collision
		scal = (params.radii - scal) / (params.dt*scal);
		particle_array[p].speed[0] -= PC[0] * scal;
		particle_array[p].speed[1] -= PC[1] * scal;
	}
}

void Particle_simulator::collision_ps_rebound(uint32_t p, float PC[2]) {
	float scal = sqrt(PC[0]*PC[0] + PC[1]*PC[1]); // ||PC||

	if (scal < params.radii) { // Collision
		scal = (particle_array[p].speed[0]*PC[0] + particle_array[p].speed[1]*PC[1]) / (scal*scal); // vp = v.PC/||PC||
		scal = std::max(0.f, scal);

		particle_array[p].speed[0] -= 2* PC[0] *scal;
		particle_array[p].speed[1] -= 2* PC[1] *scal;
	}
}




void Particle_simulator::world_borders_base(uint32_t p_start, uint32_t p_end) {
	float vec[2];
	for (uint32_t p1=p_start; p1<p_end; p1++) {
		for (uint8_t i=0; i<2; i++) {
			vec[i] = particle_array[p1].position[i] + particle_array[p1].speed[i]*params.dt;
			if (vec[i] < params.radii) {
				particle_array[p1].speed[i] += (params.radii - vec[i]) /params.dt;
			} else if (world.getSize(i) - params.radii < vec[i]) {
				particle_array[p1].speed[i] += ((world.getSize(i) - params.radii) - vec[i]) /params.dt;
			}
		}
	}
}


void Particle_simulator::world_borders_rebound(uint32_t p_start, uint32_t p_end) {
	float vec[2];
	for (uint32_t p1=p_start; p1<p_end; p1++) {
		for (uint8_t i=0; i<2; i++) {
			vec[i] = particle_array[p1].position[i] + particle_array[p1].speed[i]*params.dt;
			if (vec[i] < params.radii && particle_array[p1].speed[i] < 0) {
				particle_array[p1].position[i] = params.radii;
				particle_array[p1].speed[i] = -particle_array[p1].speed[i];
			} else if (world.getSize(i) - params.radii < vec[i] && particle_array[p1].speed[i] > 0) {
				particle_array[p1].position[i] = world.getSize(i) - params.radii;
				particle_array[p1].speed[i] = -particle_array[p1].speed[i];
			}
		}
	}
}

/* void Particle_simulator::world_loop(uint32_t p_start, uint32_t p_end) {
	float vec[2];
	for (uint32_t p1=p_start; p1<p_end; p1++) {
		// teleporting from right to left
		if (world.getSize(0) < particle_array[p1].position[0]) {
			particle_array[p1].position[0] -= world.getSize(0);
			particle_array[p1].position[1] = ((float)rand()/RAND_MAX) * world.getSize(1);
			gen_spawn_speed(particle_array[p1].speed);
			gen_particle(1);
			particle_array[p1].speed[0] = std::abs(particle_array[p1].speed[0]);
		}

		// left, top and bottom borders
		vec[0] = particle_array[p1].position[0] + particle_array[p1].speed[0]*params.dt;
		if (vec[0] < params.radii && particle_array[p1].speed[0] < 0) { // left
			particle_array[p1].position[0] = params.radii;
			particle_array[p1].speed[0] = -particle_array[p1].speed[0];
		}
		vec[1] = particle_array[p1].position[1] + particle_array[p1].speed[1]*params.dt;
		if (vec[1] < params.radii && particle_array[p1].speed[1] < 0) { // top
			particle_array[p1].position[1] = params.radii;
			particle_array[p1].speed[1] = -particle_array[p1].speed[1];
		} else if (world.getSize(1) - params.radii < vec[1] && particle_array[p1].speed[1] > 0) { // bottom
			particle_array[p1].position[1] = world.getSize(1) - params.radii;
			particle_array[p1].speed[1] = -particle_array[p1].speed[1];
		}
	}
}
*/


void Particle_simulator::comparison_pz(uint32_t p_start, uint32_t p_end) {
	// std::cout << "comparison_pz(" << p_start << ", " << p_end << ")" << std::endl;
	for (uint32_t p=p_start; p<p_end; p++) {
		for (uint16_t z=0; z<world.getNbOfZones(); z++) {
			// check if the Particle p is in the Zone z
			Zone& zone = world.getZone(z);
			if (zone.check_in(particle_array[p].position)) {
				zone_functions(p, zone);
			}
		}
	}
}

void Particle_simulator::comparison_zp(uint32_t c_start, uint32_t c_end, uint16_t zone_num) {
	// std::cout << "comparison_zp(" << c_start << ", " << c_end << ", " << zone_num << ")" << std::endl;
	Zone& zone = world.getZone(zone_num);
	// iterating through [c_start, c_end[ along the length of the zone (which may be along x or y).
	// the other axis iterate through the entire width of the zone.
	uint16_t bounds[2][2];
	bounds[0][!zone.lc()] = zone.getWidthLowBound();
	bounds[1][!zone.lc()] = zone.getWidthUpBound();
	bounds[0][ zone.lc()] = zone.getLengthLowBound() + c_start;
	bounds[1][ zone.lc()] = zone.getLengthLowBound() + c_end;
	
	for (uint16_t cy=bounds[0][1]; cy<bounds[1][1]; cy++) {
		for (uint16_t cx=bounds[0][0]; cx<bounds[1][0]; cx++) {
			Cell& cell = world.getCell(cx, cy);
			for (uint8_t p_c=0; p_c<cell.nb_parts; p_c++) {
				zone_functions(cell.parts[p_c], zone);
			}
		}
	}
}

void Particle_simulator::zone_functions(uint32_t p, Zone& zone) {
	// I could use a switch(zone_fun) for the various functions associated to different zones.
	// But right now I'm only using one function so an if will be enough.
	if (zone.fun==0) {
		particle_init(p, world.getSpawnRect());
	}
}


void Particle_simulator::delete_NaNs(uint32_t p_start, uint32_t p_end) {
	// uint32_t found = 0;
	for (uint32_t p1=p_start; p1<p_end; p1++) {
		if (std::isnan(particle_array[p1].position[0]) || std::isnan(particle_array[p1].position[1]) ||
				std::abs(particle_array[p1].position[0]) == INFINITY || std::abs(particle_array[p1].position[1]) == INFINITY) {
			delete_particle(p1--);
			p_end--; // p_end-- as I only use it on 1 thread for now. I'll need to think about it :/
			// found++;
		}
	}
	// std::cout << "found " << found << " NaNs out of " << (found + p_end-p_start) << std::endl;
}


void Particle_simulator::gravity(uint32_t p_start, uint32_t p_end) {
	// std::cout << "Particle_simulator::gravity()" << std::endl;
	for (uint32_t p=p_start; p<p_end; p++) {
		particle_array[p].speed[1] += params.grav_force*params.dt;
	}
}

void Particle_simulator::point_gravity(uint32_t p_start, uint32_t p_end) {
	// std::cout << "Particle_simulator::point_gravity()" << std::endl;
	float vec[2];
	float multiplier;

	for (uint32_t p=p_start; p<p_end; p++) {
		vec[0] = params.grav_center[0] - particle_array[p].position[0];
		vec[1] = params.grav_center[1] - particle_array[p].position[1];
		multiplier = sqrt(vec[0]*vec[0] + vec[1]*vec[1]);
		multiplier = params.dt* params.grav_force / multiplier;
	
		particle_array[p].speed[0] += vec[0] * multiplier;
		particle_array[p].speed[1] += vec[1] * multiplier;
	}
}

void Particle_simulator::point_gravity_invSquared(uint32_t p_start, uint32_t p_end) {
	// std::cout << "Particle_simulator::point_gravity()" << std::endl;
	float vec[2], norm;
	float multiplier;

	for (uint32_t p=p_start; p<p_end; p++) {
		vec[0] = params.grav_center[0] - particle_array[p].position[0];
		vec[1] = params.grav_center[1] - particle_array[p].position[1];
		norm = sqrt(vec[0]*vec[0] + vec[1]*vec[1]) * params.grav_force_decay;
		multiplier = atan(norm) / norm;
		multiplier = params.dt* params.grav_force *multiplier *multiplier;
	
		particle_array[p].speed[0] += vec[0] * multiplier;
		particle_array[p].speed[1] += vec[1] * multiplier;
	}
}


void Particle_simulator::static_friction(uint32_t p_start, uint32_t p_end) {
	// std::cout << "Particle_simulator::static_friction()" << std::endl;
	float vec[2];
	float norm;

	for (uint32_t p=p_start; p<p_end; p++) {
		vec[0] = particle_array[p].speed[0];
		vec[1] = particle_array[p].speed[1];
		norm = sqrt(vec[0]*vec[0] + vec[1]*vec[1]);
		if (norm < params.static_speed_block*params.dt) {
			particle_array[p].speed[0] = 0;
			particle_array[p].speed[1] = 0;
		}
	}
}


void Particle_simulator::fluid_friction(uint32_t p_start, uint32_t p_end) {
	// std::cout << "Particle_simulator::fluid_friction()" << std::endl;
	float multiplier;
	for (uint32_t p=p_start; p<p_end; p++) {
		multiplier = 1 - params.fluid_friction_coef*params.dt;
		particle_array[p].speed[0] *= multiplier;
		particle_array[p].speed[1] *= multiplier;
	}
}

void Particle_simulator::vibrate(uint32_t p_start, uint32_t p_end) { // This function needs to be done better. I don't like the result
	int32_t max = 20000;
	for (uint32_t p=p_start; p<p_end; p++) {
		particle_array[p].speed[0] += sin((300+p/10)*(time[0] +p)) * max *params.dt;
		particle_array[p].speed[1] += (2*(int8_t)(p%2)-1)*cos((300+p/10)*(time[0] +p)) * max *params.dt;
	}
}



void Particle_simulator::attraction(uint32_t p_start, uint32_t p_end) {
	// std::cout << "Particle_simulator::attraction" << std::endl;
	float vec[2], norm;
	for (uint32_t p=p_start; p<p_end; p++) {
		vec[0] = user_point[0] - particle_array[p].position[0];
		vec[1] = user_point[1] - particle_array[p].position[1];
		norm = vec[0]*vec[0] + vec[1]*vec[1];

		if (norm < params.range*params.range) {
			norm = params.translation_force / sqrt(norm) * params.dt;
			particle_array[p].speed[0] += vec[0] * norm;
			particle_array[p].speed[1] += vec[1] * norm;
		}
	}
}

void Particle_simulator::rotation(uint32_t p_start, uint32_t p_end) {
	float vec[2], norm;
	for (uint32_t p=p_start; p<p_end; p++) {
		vec[0] = -user_point[1] + particle_array[p].position[1];
		vec[1] =  user_point[0] - particle_array[p].position[0];
		norm = vec[0]*vec[0] + vec[1]*vec[1];

		if (norm < params.range*params.range) {
			norm = params.rotation_force / sqrt(norm) *params.dt;
			particle_array[p].speed[0] += vec[0] * norm;
			particle_array[p].speed[1] += vec[1] * norm;
		}
	}
}

void Particle_simulator::vortex(uint32_t p_start, uint32_t p_end) {
	// std::cout << "Particle_simulator::vortex_ranged()" << std::endl;
	float vec[2], norm;
	for (uint32_t p=p_start; p<p_end; p++) {
		vec[0] = user_point[0] - particle_array[p].position[0];
		vec[1] = user_point[1] - particle_array[p].position[1];
		float norm = vec[0]*vec[0] + vec[1]*vec[1];

		if (norm < params.range*params.range) {
			norm = params.dt / sqrt(norm);
			particle_array[p].speed[0] += (vec[0]*params.translation_force - vec[1]*params.rotation_force) *norm;
			particle_array[p].speed[1] += (vec[1]*params.translation_force + vec[0]*params.rotation_force) *norm;
		}
	}
}


void Particle_simulator::delete_particle(uint32_t p) {
	// std::cout << "Particle_simulator::delete_particle()" << std::endl;
	Particle swap;

	swap = particle_array[nb_active_part-1]; // Their order doesn't matter
	particle_array[nb_active_part-1] = particle_array[p];
	particle_array[p] = swap;

	nb_active_part--;
}

/**
* @details This could be optimized by using the world grid.
* But that would mean I couldn't delete Particles outside of the world and I don't feel like implementing it when this function is only used by the user (so not often).
*/
void Particle_simulator::delete_range(float x, float y, float range_) {
	// std::cout << "Particle_simulator::delete_range()" << std::endl;
	float vec[2], norm;
	for (uint32_t p=0; p<nb_active_part; p++) {
		vec[0] = x - particle_array[p].position[0];
		vec[1] = y - particle_array[p].position[1];
		norm = sqrt(vec[0]*vec[0] + vec[1]*vec[1]);

		if (norm < params.range) {
			delete_particle(p);
			p--;
		}
	}
}


uint32_t Particle_simulator::create_particles(uint32_t n_particles) {
	// std::cout << "Particle_simulator::create_particles(), nb_actif=" << nb_active_part << std::endl;
	if (nb_active_part < nb_max_part) {
		uint32_t before = nb_active_part;
		nb_active_part = std::min(nb_active_part+n_particles, nb_max_part);

		for (uint32_t p=before; p<nb_active_part; p++) {
			particle_init(p, world.getSpawnRect());
		}
		
		return nb_active_part;

	} else {
		return NULLPART;
	}
}

void Particle_simulator::particle_init(uint32_t part, Rectangle pos_rect) {
	// std::cout << "\tparticle_init(" << part << ") rect=[" << pos_rect.pos(0) << ", " << pos_rect.pos(1) << "],  [" << pos_rect.size(0) << ", " << pos_rect.size(1) << "]" << std::endl;
	particle_array[part].position[0] = pos_rect.pos(0) + rand()*pos_rect.size(0)/RAND_MAX;
	particle_array[part].position[1] = pos_rect.pos(1) + rand()*pos_rect.size(1)/RAND_MAX;

	float theta = (float)rand()/RAND_MAX *2*M_PI;
	particle_array[part].speed[0] = params.temperature*cos(theta) + params.spawn_speed[0];
	particle_array[part].speed[1] = params.temperature*sin(theta) + params.spawn_speed[1];
}


bool Particle_simulator::load_next_positions() {
	if (reinitialize_order) {
		resetPosLoading();
		reinitialize_order = false;
	}
	if (!finished_loading && (!paused || step || quickstep)) {
		step = false;
		if (quickstep) std::this_thread::sleep_for(std::chrono::milliseconds(50));
		conso.Start();
		uint32_t returned = partLoader->loadPos(particle_array.data(), nb_max_part, &time[0]);
		if (returned == NULLPART) {
			finished_loading = true;
			std::cout << "Finished loading particle positions" << std::endl;
		} 
		else nb_active_part = returned; 
		conso.Tick_fine(true);
	}
	return finished_loading;
}

void Particle_simulator::resetPosLoading() {
	if (partLoader) {
		partLoader->resetPosLoading();
		finished_loading = false;
	}
};