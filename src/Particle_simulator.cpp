#include "Particle_simulator.hpp"
#include "Segment.hpp"
#include "utilities.hpp"

#include <cmath>
#include <cstdlib>
#include <iostream>
#include <thread>


Particle_simulator::Particle_simulator() : world(1800, 1000, 2*radii, 2*radii, NB_PART) {
	std::cout << "Particle_simulator::Particle_simulator()" << std::endl;
	initialize_particles();
	setDefaultParameters();

	world.seg_array.reserve(2);
	world.add_segment(100, 100, 900, 500);
	world.add_segment(1700, 300, 700, 800);

	// world.add_segment(4100, 1900, 4000, 2000);
	// world.add_segment(4100, 2100, 4000, 2000);

	world.chg_seg_store_sys(nb_active_part);

	std::cout << "\t" << i2s(world.n_cell_seg) << " cells with a segment" << std::endl;
	std::cout << "\t" << i2s(NB_PART) << " particles" << std::endl;
	std::cout << "\tsize Particle array : " << i2s(sizeof(particle_array)) << " bytes" << std::endl;
	// std::cout << "End Particle_simulator::Particle_simulator()" << std::endl;
}


Particle_simulator::~Particle_simulator() {
	std::cout << "Particle_simulator::~Particle_simulator()" << std::endl;
	simulate = false;
	stop_simulation_threads();
}

void Particle_simulator::setDefaultParameters() {
	masses = 1;
	dt = 0.001;
	spawn_speed[0] = 0;
	spawn_speed[1] = 0;

	grav_force = 1000;
	grav_force_decay = 0.01f;
	grav_center[0] = world.getSize(0)/2;
	grav_center[1] = world.getSize(1)/2;
	static_speed_block = 1;
	fluid_friction_coef = 0.3;

	// User forces
	userForce appliedForce = userForce::None;
	user_point[0] = 0;
	user_point[1] = 0;
	translation_force = 3000;
	rotation_force = 1000;
	range = 200;
	deletion_order = false;

	cs = 1;
}

void Particle_simulator::initialize_particles() {
	nb_active_part = NB_PART;
	uint16_t x=0,y=0;
	for (uint32_t i=0; i<NB_PART; i++) {
		// Spawning in an ordered ractangle formation
		// particle_array[i].position[0] = x* world.getCellSize(0) + radii;
		// particle_array[i].position[1] = y* world.getCellSize(1) + radii;
		// if (++x==world.getGridSize(0)) {
		// 	x = 0;
		// 	y++;
		// }

		// random spawning
		particle_array[i].position[0] = (float)rand()/RAND_MAX * (world.getSize(0) - 4*radii) + 2*radii;
		particle_array[i].position[1] = (float)rand()/RAND_MAX * (world.getSize(1) - 4*radii) + 2*radii;
		particle_array[i].speed[0] = spawn_speed[0];
		particle_array[i].speed[1] = spawn_speed[1];
		
		// For collision testing purposes
		// particle_array[i].position[0] = i ? 30 : 10;
		// particle_array[i].position[1] = 100;
		// particle_array[i].speed[0] = i ? -500 : 0;
		// particle_array[i].speed[1] = i ? 0 : 0;
	}
}


void Particle_simulator::start_simulation_threads() {
	// std::cout << "Particle_simulator::start_simulation_threads()" << std::endl;
	threadHandler.set_nb_fun(15+world.seg_array.size());
	simulate = true;
	for (uint8_t i=0; i<std::min(MAX_THREAD_NUM, NB_PART); i++) {
		threadHandler.give_new_thread(new std::thread(&Particle_simulator::simulation_thread, this, i));
	}
}

void Particle_simulator::stop_simulation_threads() {
	// std::cout << "Particle_simulator::stop_simulation_threads()" << std::endl;
	simulate = false;
	threadHandler.wait_for_threads_end();
}


void Particle_simulator::simulation_thread(uint8_t th_id) {
	// std::cout << "Particle_simulator::simulation_thread()" << std::endl;
	uint32_t nppt; // number of particles per thread
	uint32_t sub_nppt;
	uint8_t num_fun;

	threadHandler.synchronize_last(MAX_THREAD_NUM, 1, this, &Particle_simulator::pause_wait); // This is here only to make so the simulation can start paused before looping once.

	if (!th_id) {
		conso.start_perf_check("average sim loop", 20000);
		conso2.start_perf_check("Collision particles", 20000);
	}
	while (simulate) {
		num_fun = 0;
		// Synchronize & do stuff that shouldn't be done by multiple threads, like changing the number of Particles
		threadHandler.synchronize_last(MAX_THREAD_NUM, 1, this, &Particle_simulator::create_destroy_wait);
		nppt = nb_active_part/MAX_THREAD_NUM; // number of particles per thread +- 1
		sub_nppt = std::max(nppt/5, (uint32_t)10);
		
		// Filling the grid
		auto bound_update_grid_particle_contenance = std::bind(&World::update_grid_particle_contenance, &world, particle_array, std::placeholders::_1, std::placeholders::_2, dt);
		threadHandler.load_repartition(bound_update_grid_particle_contenance, num_fun++, nb_active_part, sub_nppt);


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

		// threadHandler.load_repartition(this, &Particle_simulator::point_gravity, num_fun++, nb_active_part, sub_nppt);
		// threadHandler.load_repartition(this, &Particle_simulator::point_gravity_invSquared, num_fun++, nb_active_part, sub_nppt);
		threadHandler.load_repartition(this, &Particle_simulator::gravity, num_fun++, nb_active_part, sub_nppt);
		// threadHandler.load_repartition(this, &Particle_simulator::vibrate, num_fun++, nb_active_part, sub_nppt);
		// threadHandler.load_repartition(this, &Particle_simulator::fluid_friction, num_fun++, nb_active_part, sub_nppt);


		if (!th_id) conso2.Start();
		threadHandler.load_repartition(this, &Particle_simulator::collision_pp_grid, num_fun++, nb_active_part, sub_nppt);
		if (!th_id) conso2.Tick_fine(true);

		if (world.sig()) {
			threadHandler.load_repartition(this, &Particle_simulator::comparison_ps_grid, num_fun++, nb_active_part, sub_nppt);
		} else {
			for (uint16_t i=0; i<world.seg_array.size(); i++) {
				auto bound_collision_pl_grid = std::bind(&Particle_simulator::comparison_sp_grid, this, std::placeholders::_1, std::placeholders::_2, i);
				threadHandler.load_repartition(bound_collision_pl_grid, num_fun++, world.seg_array[i].cells.size(), 20);
			}
		}
		
		threadHandler.load_repartition(this, &Particle_simulator::world_borders, num_fun++, nb_active_part, sub_nppt);
		// threadHandler.load_repartition(this, &Particle_simulator::world_borders_rebound, num_fun++, nb_active_part, sub_nppt);
		// threadHandler.load_repartition(this, &Particle_simulator::world_loop, num_fun++, nb_active_part, sub_nppt);

		// threadHandler.load_repartition(this, &Particle_simulator::static_friction, num_fun++, nb_active_part, sub_nppt);

		// updating position
		threadHandler.synchronize_last(MAX_THREAD_NUM, 1, this, &Particle_simulator::pause_wait);
		threadHandler.load_repartition(this, &Particle_simulator::update_pos, num_fun++, nb_active_part, sub_nppt);

		// Removing the particles from the grid, thus preparing for the next loop
		if (world.emptying_blindly()) threadHandler.load_repartition(&world, &World::empty_grid_particle_blind, num_fun++, world.getGridSize(1), world.getGridSize(1)/MAX_THREAD_NUM/2);
		else {
			threadHandler.load_repartition(&world, &World::empty_grid_particle_pbased, num_fun++, nb_active_part, sub_nppt);
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
		threadHandler.synchronize_last(MAX_THREAD_NUM, 1, this, &Particle_simulator::loop_wait);

		// Simulation -- applying forces and collisions
		nppt = nb_active_part/MAX_THREAD_NUM; // number of particles per thread +- 1
		p_start = nppt *th_id       + std::min(th_id, (uint8_t)(nb_active_part%MAX_THREAD_NUM));
		p_end =   nppt *(th_id+1)   + std::min(th_id, (uint8_t)(nb_active_part%MAX_THREAD_NUM)) + (th_id < (uint8_t)(nb_active_part%MAX_THREAD_NUM));

		if (world.emptying_blindly()) {
			uint16_t c_start = world.getGridSize(1)/MAX_THREAD_NUM *th_id + std::min(th_id, (uint8_t)(world.getGridSize(1)%MAX_THREAD_NUM));
			uint16_t c_end =   world.getGridSize(1)/MAX_THREAD_NUM *(th_id+1)   + std::min(th_id, (uint8_t)(world.getGridSize(1)%MAX_THREAD_NUM)) + (th_id < (uint8_t)(world.getGridSize(1)%MAX_THREAD_NUM));
			world.empty_grid_particle_blind(c_start, c_end);
		
			} else world.empty_grid_particle_pbased(p_start, p_end);

		world.update_grid_particle_contenance(particle_array, p_start, p_end, dt);

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

		// point_gravity(p_start, p_end);
		gravity(p_start, p_end);
		// fluid_friction(p_start, p_end);
		// static_friction(p_start, p_end);
		// vibrate(p_start, p_end);

		// updating speed

		// collision_pp(p_start, p_end);
		collision_pp_grid(p_start, p_end);
		// collision_pp_glue(p_start, p_end);

		if (world.sig()) comparison_ps_grid(p_start, p_end);
		else {
			for (uint16_t i=0; i<world.seg_array.size(); i++) {
				uint32_t nspt = world.seg_array[i].cells.size() / MAX_THREAD_NUM;
				uint32_t c_start= i   *nspt;
				uint32_t c_end = (i+1)*nspt;
				comparison_sp_grid(c_start, c_end, i);
			}
		}
		world_borders(p_start, p_end);

		// Simulation -- displacement
		// solver(p_start, p_end);

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
	time[0] += dt;
	// world.chg_seg_store_sys(nb_active_part);
	if (deletion_order) {
		delete_range(user_point[0], user_point[1], range);
	}
	if (!(rand()%4096)) {
		delete_NaNs(0, nb_active_part);
	}
	if (0.01f * radii < time[0] - time[1]) {
		time[1] = time[0];
		// create_particle(0, rand()%(uint16_t)world.getSize(1), spawn_speed[0], spawn_speed[1]); // Spawn on the left border
		create_particle(world.getSize(0)/2-200, 100,  800 * cos(2*time[0]), -800 * sin(2*time[0])); // Spawn fountain
		create_particle(world.getSize(0)/2+200, 100, -800 * cos(2*time[0]), -800 * sin(2*time[0])); // Spawn fountain

	}
}


void Particle_simulator::update_pos(uint32_t p_start, uint32_t p_end) {
	// std::cout << "Particle_simulator::solver()" << std::endl;
	for (uint32_t p=p_start; p<p_end; p++) {
		particle_array[p].position[0] += particle_array[p].speed[0] * dt;
		particle_array[p].position[1] += particle_array[p].speed[1] * dt;
	}
}


void Particle_simulator::collision_pp(uint32_t p_start, uint32_t p_end) {
	// std::cout << "Particle_simulator::collision_pp()" << std::endl;
	float vec[2];
	float dist;
	float collision_coef = 0.45f;
	for (uint8_t i=0; i<2; i++) {
		for (uint32_t p1=p_start; p1<p_end; p1++) {

			for (uint32_t p2=p1+1; p2<nb_active_part; p2++) {
				vec[0] = particle_array[p2].position[0] + particle_array[p2].speed[0]*dt - particle_array[p1].position[0] - particle_array[p1].speed[0]*dt;
				vec[1] = particle_array[p2].position[1] + particle_array[p2].speed[1]*dt - particle_array[p1].position[1] - particle_array[p1].speed[1]*dt;
				dist = sqrt(vec[0]*vec[0] + vec[1]*vec[1]);
				
				collision_pp_base(p1, p2, dist, vec);
			}

		}
	}
}


void Particle_simulator::collision_pp_grid(uint32_t p_start, uint32_t p_end) {
	cs = 1;
	uint16_t dPos[2];
	float next_pos[2];
	float vec[2];
	float dist;
	uint32_t p2;
	uint16_t x, y;
	uint16_t min_x, max_x, min_y, max_y;
	for (uint32_t p1=p_start; p1<p_end; p1++) {
		next_pos[0] = particle_array[p1].position[0] + particle_array[p1].speed[0]*dt;
		next_pos[1] = particle_array[p1].position[1] + particle_array[p1].speed[1]*dt;
		x = next_pos[0] / world.getCellSize(0);
		y = next_pos[1] / world.getCellSize(1);
		// std::cout << "coord = " << x << ", " << y << std::endl;
		min_x = std::max(x-cs, 0);
		min_y = std::max(y-cs, 0);
		max_x = std::min((uint16_t)(x+cs+1), world.getGridSize(0));
		max_y = std::min((uint16_t)(y+cs+1), world.getGridSize(1));
		// std::cout << "x [" << min_x << ", " << max_x << "]   y [" << min_y << ", " << max_y << "]" << std::endl;

		for (dPos[1]=min_y; dPos[1]<max_y; dPos[1]++) {
			for (dPos[0]=min_x; dPos[0]<max_x; dPos[0]++) {
				// print_vect("dPos", dPos);
				Cell& cell = world.getCell(dPos[0], dPos[1]);
				for (uint8_t part2=0; part2<cell.nb_parts; part2++) {
					p2 = cell.parts[part2];
					if (p1 != p2) {

						vec[0] = particle_array[p2].position[0] + particle_array[p2].speed[0]*dt - next_pos[0];
						vec[1] = particle_array[p2].position[1] + particle_array[p2].speed[1]*dt - next_pos[1];
						dist = sqrt(vec[0]*vec[0] + vec[1]*vec[1]);
						// std::cout << "Comparing " << p1 << " with " << p2 << " : dits=" << dist;
						// print_vect("vec", vec);
						
						// appel fonction
						collision_pp_base(p1, p2, dist, vec);
						// collision_pp_glue(p1, p2, dist, vec);
						// collision_pp_pĥyacc(p1, p2, dist, vec);
					}
				}
				
			}
		}
		// std::cout << std::endl;

	}
}


void Particle_simulator::collision_pp_base(uint32_t p1, uint32_t p2, float dist, float vec[2]) {
	if (dist < 2*radii) {
		float collision_coef = 0.45f;
		float temp_coef;

		temp_coef = (2*radii - dist) * collision_coef /(dt * dist);
		vec[0] *= temp_coef;
		vec[1] *= temp_coef;

		particle_array[p1].speed[0] -= vec[0];
		particle_array[p1].speed[1] -= vec[1];
		particle_array[p2].speed[0] += vec[0];
		particle_array[p2].speed[1] += vec[1];
	}
}

void Particle_simulator::collision_pp_glue(uint32_t p1, uint32_t p2, float dist, float vec[2]) {
	float collision_coef = 0.49f;
	float glue_coef = 0.0005f;
	float temp_coef;

	temp_coef = (2*radii - dist) * collision_coef /(dt * dist);
	temp_coef = dist < 2*radii ? temp_coef*collision_coef : temp_coef * glue_coef;
	vec[0] *= temp_coef;
	vec[1] *= temp_coef;

	particle_array[p1].speed[0] -= vec[0];
	particle_array[p1].speed[1] -= vec[1];
	particle_array[p2].speed[0] += vec[0];
	particle_array[p2].speed[1] += vec[1];
}

void Particle_simulator::collision_pp_pĥyacc(uint32_t p1, uint32_t p2, float dist, float rho[2]) {
	if (dist < 2*radii) {
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
			float E = (v1[0]*v1[0] + v2[0]*v2[0]) /2; // E = (m1*v1*v1 + m2*v2*v2)/2
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
			float collision_coef = 1;
			float temp_coef;

			temp_coef = (2*radii - dist) * collision_coef;
			rho[0] *= temp_coef;
			rho[1] *= temp_coef;

			particle_array[p1].speed[0] -= rho[0];
			particle_array[p1].speed[1] -= rho[1];
			particle_array[p2].speed[0] += rho[0];
			particle_array[p2].speed[1] += rho[1];
		}
	}
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
		x = particle_array[p].position[0] / world.getCellSize(0);
		y = particle_array[p].position[1] / world.getCellSize(1);

		if (x < world.getGridSize(0) && y < world.getGridSize(1)) {
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
		part_next_pos[0] = particle_array[p].position[0] + particle_array[p].speed[0]*dt,
		part_next_pos[1] = particle_array[p].position[1] + particle_array[p].speed[1]*dt
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
		collision_ps_base(p, vec);
		// collision_ps_rebound(p, vec);
	}
	else { // C is not in [AB] but the particle might still be near the edges A or B
		for (uint8_t i=0; i<2; i++) {
			// The collision point could then be an end of the Segment
			vec[0] = seg.pos[i][0] - part_next_pos[0];
			vec[1] = seg.pos[i][1] - part_next_pos[1];
			collision_ps_base(p, vec);
			// collision_ps_rebound(p, vec);
		}
	}
}

void Particle_simulator::collision_ps_base(uint32_t p, float PC[2]) {
	float scal = sqrt(PC[0]*PC[0] + PC[1]*PC[1]); // ||CX||

	if (scal < radii) { // Collision
		scal += 0.00001f; // to avoid scal=0
		scal = (radii - scal) / (dt*scal);
		particle_array[p].speed[0] -= PC[0] * scal;
		particle_array[p].speed[1] -= PC[1] * scal;
	}
}

void Particle_simulator::collision_ps_rebound(uint32_t p, float PC[2]) {
	float scal = sqrt(PC[0]*PC[0] + PC[1]*PC[1]); // ||CX||

	if (scal < radii) { // Collision
		// Normalize PC (as in ||PC|| = 1);
		PC[0] /= scal;
		PC[1] /= scal;
		// Calculate normal speed (in regard of the Segment)
		float normal_speed = particle_array[p].speed[0]*PC[0] + particle_array[p].speed[1]*PC[1]; // vp = v.PC
		normal_speed = std::max(0.f, normal_speed);
		particle_array[p].speed[0] -= 2* PC[0] *normal_speed;
		particle_array[p].speed[1] -= 2* PC[1] *normal_speed;
	}
}




void Particle_simulator::world_borders(uint32_t p_start, uint32_t p_end) {
	float vec[2];
	bool in[2];
	for (uint32_t p1=p_start; p1<p_end; p1++) {
		for (uint8_t i=0; i<2; i++) {
			vec[i] = particle_array[p1].position[i] + particle_array[p1].speed[i]*dt;
			if (vec[i] < radii) {
				particle_array[p1].speed[i] += (radii - vec[i]) /dt;
			} else if (world.getSize(i) - radii < vec[i]) {
				particle_array[p1].speed[i] += ((world.getSize(i) - radii) - vec[i]) /dt;
			}
		}
	}
}


void Particle_simulator::world_borders_rebound(uint32_t p_start, uint32_t p_end) {
	float vec[2];
	bool in[2];
	for (uint32_t p1=p_start; p1<p_end; p1++) {
		for (uint8_t i=0; i<2; i++) {
			vec[i] = particle_array[p1].position[i] + particle_array[p1].speed[i]*dt;
			if (vec[i] < radii && particle_array[p1].speed[i] < 0) {
				particle_array[p1].position[i] = radii;
				particle_array[p1].speed[i] = -particle_array[p1].speed[i];
			} else if (world.getSize(i) - radii < vec[i] && particle_array[p1].speed[i] > 0) {
				particle_array[p1].position[i] = world.getSize(i) - radii;
				particle_array[p1].speed[i] = -particle_array[p1].speed[i];
			}
		}
	}
}

void Particle_simulator::world_loop(uint32_t p_start, uint32_t p_end) {
	float vec[2];
	for (uint32_t p1=p_start; p1<p_end; p1++) {
		// teleporting from right to left
		if (world.getSize(0) < particle_array[p1].position[0]) {
			particle_array[p1].position[0] -= world.getSize(0);
			particle_array[p1].position[1] = ((float)rand()/RAND_MAX) * world.getSize(1);
			particle_array[p1].speed[0] = spawn_speed[0];
			particle_array[p1].speed[1] = spawn_speed[1];
		}

		// left, top and bottom borders
		vec[0] = particle_array[p1].position[0] + particle_array[p1].speed[0]*dt;
		if (vec[0] < radii && particle_array[p1].speed[0] < 0) { // left
			particle_array[p1].position[0] = radii;
			particle_array[p1].speed[0] = -particle_array[p1].speed[0];
		}
		vec[1] = particle_array[p1].position[1] + particle_array[p1].speed[1]*dt;
		if (vec[1] < radii && particle_array[p1].speed[1] < 0) { // top
			particle_array[p1].position[1] = radii;
			particle_array[p1].speed[1] = -particle_array[p1].speed[1];
		} else if (world.getSize(1) - radii < vec[1] && particle_array[p1].speed[1] > 0) { // bottom
			particle_array[p1].position[1] = world.getSize(1) - radii;
			particle_array[p1].speed[1] = -particle_array[p1].speed[1];
		}
	}
}


void Particle_simulator::delete_NaNs(uint32_t p_start, uint32_t p_end) {
	// uint32_t found = 0;
	for (uint32_t p1=p_start; p1<p_end; p1++) {
		if (std::isnan(particle_array[p1].position[0]) || std::isnan(particle_array[p1].position[1])) {
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
		particle_array[p].speed[1] += grav_force*dt;
	}
}

void Particle_simulator::point_gravity(uint32_t p_start, uint32_t p_end) {
	// std::cout << "Particle_simulator::point_gravity()" << std::endl;
	float vec[2];
	float multiplier;

	for (uint32_t p=p_start; p<p_end; p++) {
		vec[0] = grav_center[0] - particle_array[p].position[0];
		vec[1] = grav_center[1] - particle_array[p].position[1];
		multiplier = sqrt(vec[0]*vec[0] + vec[1]*vec[1]);
		multiplier = dt* grav_force / multiplier;
	
		particle_array[p].speed[0] += vec[0] * multiplier;
		particle_array[p].speed[1] += vec[1] * multiplier;
	}
}

void Particle_simulator::point_gravity_invSquared(uint32_t p_start, uint32_t p_end) {
	// std::cout << "Particle_simulator::point_gravity()" << std::endl;
	float vec[2], norm;
	float multiplier;

	for (uint32_t p=p_start; p<p_end; p++) {
		vec[0] = grav_center[0] - particle_array[p].position[0];
		vec[1] = grav_center[1] - particle_array[p].position[1];
		norm = sqrt(vec[0]*vec[0] + vec[1]*vec[1]) * grav_force_decay;
		multiplier = atan(norm) / norm;
		multiplier = grav_force * multiplier * multiplier *dt / norm;
	
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
		if (norm < static_speed_block*dt) {
			particle_array[p].speed[0] = 0;
			particle_array[p].speed[1] = 0;
		}
	}
}


void Particle_simulator::fluid_friction(uint32_t p_start, uint32_t p_end) {
	// std::cout << "Particle_simulator::fluid_friction()" << std::endl;
	float multiplier;
	for (uint32_t p=p_start; p<p_end; p++) {
		multiplier = 1 - fluid_friction_coef*dt;
		particle_array[p].speed[0] *= multiplier;
		particle_array[p].speed[1] *= multiplier;
	}
}

void Particle_simulator::vibrate(uint32_t p_start, uint32_t p_end) { // This function needs to be done better. I don't like the result
	int32_t max = 20000;
	for (uint32_t p=p_start; p<p_end; p++) {
		particle_array[p].speed[0] += sin(60*(time[0] +p)) * max *dt;
		particle_array[p].speed[1] += (p%2 ? 1 : -1)*cos(60*(time[0] +p)) * max *dt;
	}
}



void Particle_simulator::attraction(uint32_t p_start, uint32_t p_end) {
	// std::cout << "Particle_simulator::attraction_ranged()" << std::endl;
	float vec[2], norm;
	for (uint32_t p=p_start; p<p_end; p++) {
		vec[0] = user_point[0] - particle_array[p].position[0];
		vec[1] = user_point[1] - particle_array[p].position[1];
		norm = vec[0]*vec[0] + vec[1]*vec[1];

		if (norm < range*range) {
			norm = translation_force / sqrt(norm) * dt;
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

		if (norm < range*range) {
			norm = rotation_force / sqrt(norm) *dt;
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

		if (norm < range*range) {
			norm = dt / sqrt(norm);
			particle_array[p].speed[0] += (vec[0]*translation_force - vec[1]*rotation_force) *norm;
			particle_array[p].speed[1] += (vec[1]*translation_force + vec[0]*rotation_force) *norm;
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

		if (norm < range) {
			delete_particle(p);
			p--;
		}
	}
}


uint32_t Particle_simulator::create_particle(float x, float y, float vx, float vy) {
	// std::cout << "Particle_simulator::create_particle(), nb_actif=" << nb_active_part << std::endl;
	if (nb_active_part < NB_PART) {
		particle_array[nb_active_part].position[0] = x;
		particle_array[nb_active_part].position[1] = y;

		particle_array[nb_active_part].speed[0] = vx;
		particle_array[nb_active_part].speed[1] = vy;

		return nb_active_part++;

	} else {
		return NULLPART;
	}
}