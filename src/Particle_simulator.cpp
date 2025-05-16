#include "Particle_simulator.hpp"
#include "Segment.hpp"

#include <cmath>
#include <cstdlib>
#include <iostream>
#include <thread>


Particle_simulator::Particle_simulator() : world(1800, 1000, 2*radii, 2*radii, NB_PART) {
	std::cout << "Particle_simulator::Particle_simulator()" << std::endl;
	initialize_particles();

	world.seg_array.reserve(2);
	world.add_segment(100, 100, 900, 500);
	world.add_segment(1700, 300, 700, 800);

	world.chg_seg_store_sys(nb_active_part);

	grav_center[0] = world.getSize(0)/2;
	grav_center[1] = world.getSize(1)/2;

	std::cout << "\tsize Particle array : " << sizeof(particle_array) << " bytes" << std::endl;
	// std::cout << "End Particle_simulator::Particle_simulator()" << std::endl;
}


Particle_simulator::~Particle_simulator() {
	std::cout << "Particle_simulator::~Particle_simulator()" << std::endl;
	simulate = false;
	stop_simulation_threads();
}

void Particle_simulator::initialize_particles() {
	nb_active_part = NB_PART;
	uint16_t x=0,y=0;
	for (uint32_t i=0; i<NB_PART; i++) {
		// particle_array[i].position[0] = x* world.getCellSize(0) + radii;
		// particle_array[i].position[1] = y* world.getCellSize(1) + radii;
		// if (++x==world.getGridSize(0)) {
		// 	x = 0;
		// 	y++;
		// }

		particle_array[i].position[0] = (float)rand()/RAND_MAX * (world.getSize(0) - 4*radii) + 2*radii;
		particle_array[i].position[1] = (float)rand()/RAND_MAX * (world.getSize(1) - 4*radii) + 2*radii;
		
		particle_array[i].speed[0] = 0;
		particle_array[i].speed[1] = 0;
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

	bool first_loop = true;

	threadHandler.synchronize_last(MAX_THREAD_NUM, 1, this, &Particle_simulator::pause_wait); // This is here only to make so the simulation can start paused before looping once.

	if (!th_id) {
		conso.start_perf_check("average sim loop", 20000);
		conso2.start_perf_check("Collision segments", 20000);
	}
	while (simulate) {
		// Synchronize & do stuff that shouldn't be done by multiple threads, like changing the number of Particles
		threadHandler.synchronize_last(MAX_THREAD_NUM, 1, this, &Particle_simulator::create_destroy_wait);
		nppt = nb_active_part/MAX_THREAD_NUM; // number of particles per thread +- 1
		sub_nppt = std::max(nppt/5, (uint32_t)10);
		
		// Filling the grid
		auto bound_update_grid_particle_contenance = std::bind(&World::update_grid_particle_contenance, &world, particle_array, std::placeholders::_1, std::placeholders::_2, dt);
		threadHandler.load_repartition(bound_update_grid_particle_contenance, 1, nb_active_part, sub_nppt);


		// Simulation -- applying forces and collisions
		switch (appliedForce) {
			case userForce::None:
				break;
			case userForce::Translation:
				threadHandler.load_repartition(this, &Particle_simulator::attraction, 2, nb_active_part, sub_nppt);
				break;
			case userForce::Rotation:
				threadHandler.load_repartition(this, &Particle_simulator::rotation, 2, nb_active_part, sub_nppt);
				break;
			case userForce::Vortex:
				threadHandler.load_repartition(this, &Particle_simulator::vortex, 2, nb_active_part, sub_nppt);
				break;
		}

		// threadHandler.load_repartition(this, &Particle_simulator::point_gravity, 3, nb_active_part, sub_nppt);
		threadHandler.load_repartition(this, &Particle_simulator::gravity, 4, nb_active_part, sub_nppt);
		// threadHandler.load_repartition(this, &Particle_simulator::static_friction, 5, nb_active_part, sub_nppt);
		// threadHandler.load_repartition(this, &Particle_simulator::vibrate, 6, nb_active_part, sub_nppt);
		// threadHandler.load_repartition(this, &Particle_simulator::fluid_friction, 7, nb_active_part, sub_nppt);

		if (!th_id) conso2.Start();
		if (world.sig()) {
			threadHandler.load_repartition(this, &Particle_simulator::collision_pl_grid, 14, nb_active_part, sub_nppt);
		} else {
			for (uint16_t i=0; i<world.seg_array.size(); i++) {
				auto bound_collision_pl_grid = std::bind(&Particle_simulator::collision_lp_grid, this, std::placeholders::_1, std::placeholders::_2, i);
				threadHandler.load_repartition(bound_collision_pl_grid, 14+i, world.seg_array[i].cells.size(), 20);
			}
		}
		if (!th_id) conso2.Tick_fine();
		
		// threadHandler.load_repartition(this, &Particle_simulator::collision_pp, 8, nb_active_part, sub_nppt);
		threadHandler.load_repartition(this, &Particle_simulator::collision_pp_grid_threaded, 9, nb_active_part, sub_nppt);
		// threadHandler.load_repartition(this, &Particle_simulator::collision_pp_glue, 10, nb_active_part, sub_nppt);

		threadHandler.load_repartition(this, &Particle_simulator::world_borders, 11, nb_active_part, sub_nppt);

		// updating position
		threadHandler.synchronize_last(MAX_THREAD_NUM, 1, this, &Particle_simulator::pause_wait);
		threadHandler.load_repartition(this, &Particle_simulator::update_pos, 12, nb_active_part, sub_nppt);

		// Removing the particles from the grid, thus preparing for the next loop
		if (world.emptying_blindly()) threadHandler.load_repartition(&world, &World::empty_grid_particle_blind, 0, world.getGridSize(1), world.getGridSize(1)/MAX_THREAD_NUM/2);
		else {
			threadHandler.load_repartition(&world, &World::empty_grid_particle_pbased, 0, nb_active_part, sub_nppt);
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
		collision_pp_grid_threaded(p_start, p_end);
		// collision_pp_glue(p_start, p_end);

		collision_pl_grid(p_start, p_end);
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
	conso.Tick_fine();
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
		create_particle(world.getSize(0)/2-200, 100, 800 * cos(-2*time[0]), 800 * sin(-2*time[0]));
		create_particle(world.getSize(0)/2+200, 100, -800 * cos(-2*time[0]), 800 * sin(-2*time[0]));
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
				vec[0] /= dist;
				vec[1] /= dist;
	
				if (dist < 2*radii) {
					particle_array[p1].speed[0] -= vec[0] * (2*radii - dist) /dt *collision_coef;
					particle_array[p1].speed[1] -= vec[1] * (2*radii - dist) /dt *collision_coef;
					particle_array[p2].speed[0] += vec[0] * (2*radii - dist) /dt *collision_coef;
					particle_array[p2].speed[1] += vec[1] * (2*radii - dist) /dt *collision_coef;
				}
			}

		}
	}
}


void Particle_simulator::collision_pp_grid_threaded(uint32_t p_start, uint32_t p_end) {
	// std::cout << "Particle_simulator::collision_pp_grid_threaded()" << std::endl;
	uint16_t dPos[2];
	float next_pos[2];
	float vec[2];
	float dist;
	float collision_coef = 0.49f;
	float temp_coef;
	uint32_t part2;
	uint16_t x, y;
	int8_t min_x, max_x, min_y, max_y;
	for (uint8_t i=1; i<2; i++) { // start at i=0 for a more stable simulation
		for (uint32_t p1=p_start; p1<p_end; p1++) {
			
			next_pos[0] = particle_array[p1].position[0] + i*particle_array[p1].speed[0]*dt;
			next_pos[1] = particle_array[p1].position[1] + i*particle_array[p1].speed[1]*dt;
			x = next_pos[0] / world.getCellSize(0);
			y = next_pos[1] / world.getCellSize(1);
			min_x = -1*(x!=0);
			min_y = -1*(y!=0);
			max_x = x>world.getGridSize(0)-2 ? 1-2*(x != world.getGridSize(0)-1) : 2;
			max_y = y>world.getGridSize(1)-2 ? 1-2*(y != world.getGridSize(1)-1) : 2;
	
	
			for (int8_t dy=min_y; dy<max_y; dy++) {
				dPos[1] = y + dy;
				for (int8_t dx=min_x; dx<max_x; dx++) {
					dPos[0] = x + dx;
					uint8_t parts = world.getCell(dPos[0], dPos[1]).nb_parts;
					for (uint8_t p2=0; p2<parts; p2++) {
						part2 = world.getCell(dPos[0], dPos[1]).parts[p2];
						if (p1 != part2) {
							vec[0] = particle_array[part2].position[0] + i*particle_array[part2].speed[0]*dt - next_pos[0];
							vec[1] = particle_array[part2].position[1] + i*particle_array[part2].speed[1]*dt - next_pos[1];
							dist = sqrt(vec[0]*vec[0] + vec[1]*vec[1]);

							temp_coef = (2*radii - dist) * collision_coef /(dt * dist) /2;
							vec[0] *= temp_coef;
							vec[1] *= temp_coef;
				
							if (dist < 2*radii) {
								particle_array[p1].speed[0] -= vec[0];
								particle_array[p1].speed[1] -= vec[1];
								particle_array[part2].speed[0] += vec[0];
								particle_array[part2].speed[1] += vec[1];
							}
						}
					}
	
				}
			}
	
		}
	}
}



void Particle_simulator::collision_pl(uint32_t p_start, uint32_t p_end) {
	// std::cout << "Particle_simulator::collision_pl()" << std::endl;
	float vec[2];
	float AB[2], AC[2];
	float AB_dim[2];
	float normABsq;
	float temp;
	float min[2], max[2];
	float part_next_pos[2];
	std::vector<Segment>& seg_array = world.seg_array;
	for (uint32_t s=0; s<seg_array.size(); s++) {
		AB[0] = seg_array[s].pos[1][0] - seg_array[s].pos[0][0];
		AB[1] = seg_array[s].pos[1][1] - seg_array[s].pos[0][1];
		normABsq = AB[0]*AB[0] + AB[1]*AB[1];
		AB_dim[0] = AB[0]/normABsq;
		AB_dim[1] = AB[1]/normABsq;

		min[0] = std::min(seg_array[s].pos[0][0], seg_array[s].pos[1][0]);
		max[0] = std::max(seg_array[s].pos[0][0], seg_array[s].pos[1][0]);
		min[1] = std::min(seg_array[s].pos[0][1], seg_array[s].pos[1][1]);
		max[1] = std::max(seg_array[s].pos[0][1], seg_array[s].pos[1][1]);

		for (uint32_t p=p_start; p<p_end; p++) {
			part_next_pos[0] = particle_array[p].position[0] + particle_array[p].speed[0]*dt;
			part_next_pos[1] = particle_array[p].position[1] + particle_array[p].speed[1]*dt;

			AC[0] = part_next_pos[0] - seg_array[s].pos[0][0];
			AC[1] = part_next_pos[1] - seg_array[s].pos[0][1];
			
			// Calculating AP = (AC.AB)/||AB||² * AB
			temp = AC[0]*AB_dim[0] + AC[1]*AB_dim[1];
			vec[0] = temp * AB[0];
			vec[1] = temp * AB[1];

			// Calculating OP : OP = OA + AP
			vec[0] = vec[0] + seg_array[s].pos[0][0];
			vec[1] = vec[1] + seg_array[s].pos[0][1];

			if (min[0] <= vec[0] && vec[0] <= max[0] && min[1] <= vec[1] && vec[1] <= max[1]) { // P is in [AB]
				// Calculating CP : CP = CO + OP = OP - OC
				vec[0] = vec[0] - part_next_pos[0];
				vec[1] = vec[1] - part_next_pos[1];
				temp = sqrt(vec[0]*vec[0] + vec[1]*vec[1]); // ||CP||

				if (temp < radii) { // Collision
					vec[0] /= temp;
					vec[1] /= temp;
					particle_array[p].speed[0] -= vec[0] * (radii - temp) /dt;
					particle_array[p].speed[1] -= vec[1] * (radii - temp) /dt;
				}
			}
			else { // P is not in [AB] but the particle might still be near the edges A or B
				for (uint8_t i=0; i<2; i++) {
					vec[0] = seg_array[s].pos[i][0] - part_next_pos[0];
					vec[1] = seg_array[s].pos[i][1] - part_next_pos[1];
					temp = sqrt(vec[0]*vec[0] + vec[1]*vec[1]); // ||CX||

					if (temp < radii) { // Collision
						vec[0] /= temp;
						vec[1] /= temp;
						particle_array[p].speed[0] -= vec[0] * (radii - temp) /dt;
						particle_array[p].speed[1] -= vec[1] * (radii - temp) /dt;
					}
				}
			}
		}

	}
}

void Particle_simulator::collision_pl_grid(uint32_t p_start, uint32_t p_end) {
	// std::cout << "Particle_simulator::collision_pl_grid()" << std::endl;
	float vec[2];
	float AC[2];
	float scal;
	float part_next_pos[2];
	uint16_t x, y;
	std::vector<Segment>& seg_array = world.seg_array;
	for (uint32_t p=p_start; p<p_end; p++) {
		x = particle_array[p].position[0] / world.getCellSize(0);
		y = particle_array[p].position[1] / world.getCellSize(1);

		if (x < world.getGridSize(0) && y < world.getGridSize(1)) {
			Cell_seg& cell = world.getCell_seg(x, y);
			for (uint8_t seg=0; seg<cell.nb_segs; seg++) {
				uint16_t s = cell.segs[seg];

				part_next_pos[0] = particle_array[p].position[0] + particle_array[p].speed[0]*dt;
				part_next_pos[1] = particle_array[p].position[1] + particle_array[p].speed[1]*dt;

				AC[0] = part_next_pos[0] - seg_array[s].pos[0][0];
				AC[1] = part_next_pos[1] - seg_array[s].pos[0][1];
				
				// AP = (AC.AB)/||AB||² * AB = [(AC.AB)/||AB|| * AB/||AB||]
				scal = AC[0]*seg_array[s].vect_norm[0] + AC[1]*seg_array[s].vect_norm[1];
				vec[0] = scal * seg_array[s].vect_norm[0];
				vec[1] = scal * seg_array[s].vect_norm[1];

				// Calculating OP : OP = OA + AP
				vec[0] = vec[0] + seg_array[s].pos[0][0];
				vec[1] = vec[1] + seg_array[s].pos[0][1];

				if (seg_array[s].min[0] <= vec[0] && vec[0] <= seg_array[s].max[0] && seg_array[s].min[1] <= vec[1] && vec[1] <= seg_array[s].max[1]) { // P is in [AB]
					// Calculating CP : CP = CO + OP = OP - OC
					vec[0] = vec[0] - part_next_pos[0];
					vec[1] = vec[1] - part_next_pos[1];
					scal = sqrt(vec[0]*vec[0] + vec[1]*vec[1]); // ||CP||

					if (scal < radii) { // Collision
						if (scal==0) break;
						vec[0] /= scal;
						vec[1] /= scal;
						particle_array[p].speed[0] -= vec[0] * (radii - scal) /dt;
						particle_array[p].speed[1] -= vec[1] * (radii - scal) /dt;
					}
				}
				else { // P is not in [AB] but the particle might still be near the edges A or B
					for (uint8_t i=0; i<2; i++) {
						vec[0] = seg_array[s].pos[i][0] - part_next_pos[0];
						vec[1] = seg_array[s].pos[i][1] - part_next_pos[1];
						scal = sqrt(vec[0]*vec[0] + vec[1]*vec[1]); // ||CX||

						if (scal < radii) { // Collision
							vec[0] /= scal;
							vec[1] /= scal;
							if (scal==0) break;
							particle_array[p].speed[0] -= vec[0] * (radii - scal) /dt;
							particle_array[p].speed[1] -= vec[1] * (radii - scal) /dt;
						}
					}
				}
			}
		}
	}
}


void Particle_simulator::collision_lp_grid(uint32_t c_start, uint32_t c_end, uint16_t seg_num) {
	// std::cout << "Particle_simulator::collision_pl_grid(" << c_start << ", " << c_end << ", " << seg_num << ")" << std::endl;
	float vec[2];
	float AC[2];
	float scal;
	float part_next_pos[2];
	uint16_t x, y;
	Segment& segment = world.seg_array[seg_num];
	// std::cout << "collision pl2,  segment " << seg_num << " : " << c_start << ", " << c_end << "  vec=" << segment.vect_norm[0] << ", " << segment.vect_norm[1] << std::endl;
	for (uint32_t c=c_start; c<c_end; c++) {
		Cell& cell = world.getCell(segment.cells[c][0], segment.cells[c][1]);

		// std::cout << "\t" << "cell [" << segment.cells[c][0] << ", " << segment.cells[c][1] << "]  contient " << (short)cell.nb_parts << " particules" << std::endl;
		for (uint8_t p_c=0; p_c<cell.nb_parts; p_c++) {
			uint32_t p = cell.parts[p_c];
			// std::cout << "\t\t" << (short)p_c << " : " << p << std::endl;
	
			part_next_pos[0] = particle_array[p].position[0] + particle_array[p].speed[0]*dt;
			part_next_pos[1] = particle_array[p].position[1] + particle_array[p].speed[1]*dt;
	
			AC[0] = part_next_pos[0] - segment.pos[0][0];
			AC[1] = part_next_pos[1] - segment.pos[0][1];
			
			scal = AC[0]*segment.vect_norm[0] + AC[1]*segment.vect_norm[1];
			vec[0] = scal * segment.vect_norm[0];
			vec[1] = scal * segment.vect_norm[1];
	
			// Calculating OP : OP = OA + AP
			vec[0] = vec[0] + segment.pos[0][0];
			vec[1] = vec[1] + segment.pos[0][1];
	
			if (segment.min[0] <= vec[0] && vec[0] <= segment.max[0] && segment.min[1] <= vec[1] && vec[1] <= segment.max[1]) { // P is in [AB]
				// Calculating CP : CP = CO + OP = OP - OC
				vec[0] = vec[0] - part_next_pos[0];
				vec[1] = vec[1] - part_next_pos[1];
				scal = sqrt(vec[0]*vec[0] + vec[1]*vec[1]); // ||CP||
	
				if (scal < radii) { // Collision
					if (scal==0) break;
					vec[0] /= scal;
					vec[1] /= scal;
					particle_array[p].speed[0] -= vec[0] * (radii - scal) /dt;
					particle_array[p].speed[1] -= vec[1] * (radii - scal) /dt;
				}
			}
			else { // P is not in [AB] but the particle might still be near the edges A or B
				for (uint8_t i=0; i<2; i++) {
					vec[0] = segment.pos[i][0] - part_next_pos[0];
					vec[1] = segment.pos[i][1] - part_next_pos[1];
					scal = sqrt(vec[0]*vec[0] + vec[1]*vec[1]); // ||CX||
	
					if (scal < radii) { // Collision
						if (scal==0) break;
						vec[0] /= scal;
						vec[1] /= scal;
						particle_array[p].speed[0] -= vec[0] * (radii - scal) /dt;
						particle_array[p].speed[1] -= vec[1] * (radii - scal) /dt;
					}
				}
			}
		}
	}
}



void Particle_simulator::collision_pp_glue(uint32_t p_start, uint32_t p_end) {
	uint16_t dPos[2];
	float next_pos[2];
	float vec[2];
	float dist;
	float collision_coef = 0.45f;
	float glue_coef = 0.0005f;
	float temp_coef;
	uint32_t part2;
	uint16_t x, y;
	int8_t min_x, max_x, min_y, max_y;
	uint8_t cs = 2; // check_size : how far (in cells) the particle while check for other particles
	for (uint32_t p1=p_start; p1<p_end; p1++) {
		next_pos[0] = particle_array[p1].position[0] + particle_array[p1].speed[0]*dt;
		next_pos[1] = particle_array[p1].position[1] + particle_array[p1].speed[1]*dt;
		x = next_pos[0] / world.getCellSize(0);
		y = next_pos[1] / world.getCellSize(1);
		min_x = -(x<cs ? x : cs);
		min_y = -(y<cs ? y : cs);
		max_x = x>world.getGridSize(0)-1 -cs ? (x >= world.getGridSize(0) ? -cs : world.getGridSize(0) -x) : cs;
		max_y = y>world.getGridSize(1)-1 -cs ? (y >= world.getGridSize(1) ? -cs : world.getGridSize(1) -y) : cs;
		for (int8_t dy=min_y; dy<max_y; dy++) {
			dPos[1] = y + dy;
			for (int8_t dx=min_x; dx<max_x; dx++) {
				dPos[0] = x + dx;
				
				for (uint8_t p2=0; p2<world.getCell(dPos[0], dPos[1]).nb_parts; p2++) {
					part2 = world.getCell(dPos[0], dPos[1]).parts[p2];
					if (p1 != part2) {

						vec[0] = particle_array[part2].position[0] + particle_array[part2].speed[0]*dt - particle_array[p1].position[0] - particle_array[p1].speed[0]*dt;
						vec[1] = particle_array[part2].position[1] + particle_array[part2].speed[1]*dt - particle_array[p1].position[1] - particle_array[p1].speed[1]*dt;
						dist = sqrt(vec[0]*vec[0] + vec[1]*vec[1]);
						temp_coef = (2*radii - dist) * collision_coef /(dt * dist);
						temp_coef = dist < 2*radii ? temp_coef*collision_coef : temp_coef * glue_coef;
						vec[0] *= temp_coef;
						vec[1] *= temp_coef;
			
						particle_array[p1].speed[0] -= vec[0];
						particle_array[p1].speed[1] -= vec[1];
						particle_array[part2].speed[0] += vec[0];
						particle_array[part2].speed[1] += vec[1];
					}
				}

			}
		}

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

void Particle_simulator::world_loop(uint32_t p_start, uint32_t p_end) {
	for (uint32_t p1=p_start; p1<p_end; p1++) {
		for (uint8_t i=0; i<2; i++) {
			if (particle_array[p1].position[i] < 0) {
				particle_array[p1].position[i] += world.getSize(i);
			} else if (world.getSize(i) < particle_array[p1].position[i]) {
				particle_array[p1].position[i] -= world.getSize(i);
			}
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
		particle_array[p].speed[1] += 1000*dt;
	}
}

void Particle_simulator::point_gravity(uint32_t p_start, uint32_t p_end) {
	// std::cout << "Particle_simulator::point_gravity()" << std::endl;
	float vec[2];
	float dist;
	float temp;

	for (uint32_t p=p_start; p<p_end; p++) {
		vec[0] = grav_center[0] - particle_array[p].position[0];
		vec[1] = grav_center[1] - particle_array[p].position[1];
		dist = sqrt(vec[0]*vec[0] + vec[1]*vec[1]);
		temp = atan(dist) / dist;
		temp = temp * temp;
		vec[0] *= dist*temp;
		vec[1] *= dist*temp;
	
		particle_array[p].speed[0] += vec[0] * 1000 *dt;
		particle_array[p].speed[1] += vec[1] * 1000 *dt;
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
		particle_array[p].speed[0] = (norm<1) ? 0 : particle_array[p].speed[0];
		particle_array[p].speed[1] = (norm<1) ? 0 : particle_array[p].speed[1];
	}
}


void Particle_simulator::fluid_friction(uint32_t p_start, uint32_t p_end) {
	// std::cout << "Particle_simulator::fluid_friction()" << std::endl;
	for (uint32_t p=p_start; p<p_end; p++) {
		particle_array[p].speed[0] *= 1-0.3f*dt;
		particle_array[p].speed[1] *= 1-0.3f*dt;
	}
}

void Particle_simulator::vibrate(uint32_t p_start, uint32_t p_end) {
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