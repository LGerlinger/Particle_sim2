#include <cstddef>
#include <cstdint>
#include <iostream>

#include "SaveLoader.hpp"
#include "Particle_simulator.hpp"
#include "World.hpp"

#define MAP_FOL "Map/" 
#define PSP_FOL "PSparameters/"
#define POS_FOL "Positions/"

#define MAP_EXT ".map" // map file
#define PSP_EXT ".psp" // Particle Simulator Parameters file
#define POS_EXT ".pos" // Particle positions file

#define SLI_STARTUP_FILE "loading_orders" // File containing the orders of loading/saving and what file to look for  
#define SLinfoFILE_EXT ".sli" // Extension for this file 


SaveLoader::~SaveLoader() {
	delete_comp_data();
}


int8_t SaveLoader::load_SLinfo(SLinfo& info) {
	FullFileName name("", SLI_STARTUP_FILE, SLinfoFILE_EXT);
	if (!prepareForLoading(name)) {
		std::cout << "Loading orders from " << name.getCompleted() << " : Failed" << std::endl;
		return -1; // return -1 if opening the file fails
	}

	bool res = 0;
	if (file_in_binary)
		load(&info);
	else {
		parse_map map = map_file();
		info = SLinfo::Lazy(); // Setting parameters as default so if a loading fails the value reverts back to its default, same if I forget to call for loading of a member.
		res |= !load_from_map(map, "load_pos", info.load_pos);
		res |= !load_from_map(map, "save_pos", info.save_pos);
		info.save_pos = info.save_pos && !info.load_pos; // Save pos only if load pos is false. Loading has priority.
		res |= !load_from_map(map, "posFileName", info.posFileName, SLinfoPos::fileNameSize);
		res |= !load_from_map(map, "comp_out_speed", info.comp_out_speed);
		res |= !load_from_map(map, "comp_discreet", info.comp_discreet);

		res |= !load_from_map(map, "load_world", info.load_world);
		res |= !load_from_map(map, "load_simP", info.load_simP);
		res |= !load_from_map(map, "save_world", info.save_world);
		res |= !load_from_map(map, "save_simP", info.save_simP);
		res |= !load_from_map(map, "worldFileName", info.worldFileName, SLinfoPos::fileNameSize);
		res |= !load_from_map(map, "simPFileName", info.simPFileName, SLinfoPos::fileNameSize);

	}
	done();
	std::cout << "Loading orders from " << name.getCompleted() << " : Success" << (res ? " (partial)" : "") << std::endl;
	return res;
}

SLinfo SaveLoader::setParameters(WorldParam& Wparam, PSparam& Sparam) {
	SLinfo info;
	if (load_SLinfo(info) < 0) { // If loading orders file couldn't be found, then there's no point in looking for files.
		Wparam = WorldParam::Default;
		Sparam = PSparam::Default;
		return SLinfo::Lazy();
	}

	if (info.isLoadWorld()) {
		if (loadParam(Wparam, info.worldName()) < 0) Wparam = WorldParam::Default; // If file opening failed,
	} else Wparam = WorldParam::Default; // or if there is no loading, takes default parameters

	if (info.isLoadSimP()) {
		if (loadParam(Sparam, info.simPName()) < 0) Sparam = PSparam::Default;     // If file opening failed,
	} else Sparam = PSparam::Default; // or if there is no loading, takes default parameters

	// We can't save the world as it would require to create it first. SO we only save Simulation parameters. 
	if (info.isSaveSimP()) saveParam(Sparam, 10, KB, info.simPName() + "-bin", 1);

	return info;
}

void SaveLoader::saveParameters(World& world, Particle_simulator& sim, std::string fileName, bool binary) {
	saveWorld(world, 10, FileHandler::KB, fileName, binary);
	saveParam(sim.params, 10, FileHandler::KB, fileName, binary);
}




void SaveLoader::saveWorld(World& world, uint32_t max_size_saving, ByteSize size_type, std::string fileName, bool binary) {
	FullFileName name(MAP_FOL, fileName, MAP_EXT);
	if (!prepareForSaving(max_size_saving, size_type, name, binary)) {
		std::cout << "Saving World as " << name.getCompleted() << " : Failed" << std::endl;
		return;
	}

	// Saving world parameters
	if (binary) {
		save(&world.getParams());

		// Saving world's segments (only their positions)
		uint32_t n_segments = world.seg_array.size();
		save(&n_segments);
		if (n_segments) {
			float* posBuffer = new float[4*n_segments];
			for (uint32_t i=0; i<n_segments; i++) {
				posBuffer[4*i   ] = world.seg_array[i].pos[0][0];
				posBuffer[4*i +1] = world.seg_array[i].pos[0][1];
				posBuffer[4*i +2] = world.seg_array[i].pos[1][0];
				posBuffer[4*i +3] = world.seg_array[i].pos[1][1];
			}

			save(posBuffer, 4*n_segments * sizeof(float));
			delete[] posBuffer;
		}


		uint32_t n_zones = world.getNbOfZones();
		save(&n_zones);
		if (n_zones) {
			float* posBuffer = new float[4*n_zones];
			int8_t* funBuffer = new int8_t[n_zones];
			for (uint32_t i=0; i<n_zones; i++) {
				posBuffer[4*i   ] = world.getZone(i).pos(0);
				posBuffer[4*i +1] = world.getZone(i).pos(1);
				posBuffer[4*i +2] = world.getZone(i).endPos(0);
				posBuffer[4*i +3] = world.getZone(i).endPos(1);

				funBuffer[i] = world.getZone(i).fun;
			}

			save(posBuffer, 4*n_zones * sizeof(float));
			save(funBuffer, n_zones * sizeof(int8_t));
			delete[] posBuffer;
			delete[] funBuffer;
		}
	}
	else {
		save_array_in_string("size", world.getParams().size, 2);
		save_array_in_string("cellSize", world.getParams().cellSize, 2);
		save_array_in_string("spawnRect", world.getParams().spawn_rect.getMem(), 4);
		// save_in_string("segments_in_grid", world.getParams().segments_in_grid);
		
		file << "^\n";
		
		save_in_string("n_segments", world.seg_array.size());
		float posBuffer[5]; // 4 positions and 1 for the zone function
		for (uint32_t i=0; i<world.seg_array.size(); i++) {
			posBuffer[0] = world.seg_array[i].pos[0][0];
			posBuffer[1] = world.seg_array[i].pos[0][1];
			posBuffer[2] = world.seg_array[i].pos[1][0];
			posBuffer[3] = world.seg_array[i].pos[1][1];
			save_array_in_string("segment " + std::to_string(i), posBuffer, 4);
		}

		file << "\n";
		
		save_in_string("n_zones", world.getNbOfZones());
		for (uint32_t i=0; i<world.getNbOfZones(); i++) {
			posBuffer[0] = world.getZone(i).fun;
			posBuffer[1] = world.getZone(i).pos(0);
			posBuffer[2] = world.getZone(i).pos(1);
			posBuffer[3] = world.getZone(i).endPos(0);
			posBuffer[4] = world.getZone(i).endPos(1);
			save_array_in_string("zone " + std::to_string(i), posBuffer, 5);
		}
	}


	done();
	std::cout << "Saving World as " << name.getCompleted() << " : Success" << std::endl;
}

int8_t SaveLoader::loadParam(WorldParam& param, std::string fileName) {
	FullFileName name(MAP_FOL, fileName, MAP_EXT);
	if (!prepareForLoading(name)) {
		std::cout << "Loading World parameters from " << name.getCompleted() << " : Failed" << std::endl;
		return -1; // return false if opening the file fails
	}

	bool res = 0;
	if (file_in_binary)
		load(&param);
	else {
		parse_map map = map_file();
		param = WorldParam::Default; // Setting parameters as default so if a loading fails the value reverts back to its default, same if I forget to call for loading of a member.
		res |= !load_from_map(map, "size", param.size, 2);
		res |= !load_from_map(map, "cellSize", param.cellSize, 2);
		res |= !load_from_map(map, "spawnRect", param.spawn_rect.getMem(), 4);
		// res |= !load_from_map(map, "segments_in_grid", param.segments_in_grid);
	}
	done();
	std::cout << "Loading World parameters from " << name.getCompleted() << " : Success" << (res ? " (partial)" : "") << std::endl;
	return res;
}


int8_t SaveLoader::loadWorldSegNZones(World& world, std::string fileName) {
	FullFileName name(MAP_FOL, fileName, MAP_EXT);
	if (!prepareForLoading(name)) {
		std::cout << "Loading World segments & zones from " << name.getCompleted() << " : Failed" << std::endl;
		return -1; // return -1 if opening the file fails
	}

	bool res = 0;

	if (file_in_binary) {
		file.seekg(sizeof(WorldParam), std::ios_base::cur);
		uint32_t n_segments=0, n_zones=0;
		load(&n_segments);
		if (n_segments) {
			float* posBuffer = new float[4*n_segments];
			load(posBuffer, n_segments * 4*sizeof(float));

			world.seg_array.reserve(n_segments);
			for (uint32_t i=0; i<n_segments; i++) {
				world.add_segment(posBuffer[4*i], posBuffer[4*i+1], posBuffer[4*i+2], posBuffer[4*i+3]);
			}
			delete[] posBuffer;
		}


		load(&n_zones);
		if (n_zones) {
			float* posBuffer = new float[4*n_zones];
			int8_t* funBuffer = new int8_t[n_zones];
			load(posBuffer, n_zones * 4*sizeof(float));
			load(funBuffer, n_zones * sizeof(int8_t));

			for (uint32_t i=0; i<n_zones; i++) {
				world.add_zone(funBuffer[i], posBuffer[4*i], posBuffer[4*i+1], posBuffer[4*i+2], posBuffer[4*i+3]);
			}
			delete[] posBuffer;
			delete[] funBuffer;
		}
	}
	else {
		parse_map map = map_file(1);

		unsigned long n = 0;
		float posBuffer[5];
		if (load_from_map(map, "n_segments", n)) {
			for (unsigned long i=0; i<n; i++) {
				if (load_from_map(map, "segment " + std::to_string(i), posBuffer, 4)) {
					world.add_segment(posBuffer[0], posBuffer[1], posBuffer[2], posBuffer[3]);
				}
				else res = 1;
			}
		} else res = 1;

		if (load_from_map(map, "n_zones", n)) {
			for (unsigned long i=0; i<n; i++) {
				if (load_from_map(map, "zone " + std::to_string(i), posBuffer, 5)) {
					world.add_zone(posBuffer[0], posBuffer[1], posBuffer[2], posBuffer[3], posBuffer[4]);
				}
				else res = 1;
			}
		} else res = 1;
	}

	done();
	std::cout << "Loading World segments & zones from " << name.getCompleted() << " : Success" << (res ? " (partial)" : "") << std::endl;
	return res;
}



void SaveLoader::saveParam(PSparam& param, uint32_t max_size_saving, ByteSize size_type, std::string fileName, bool binary) {
	FullFileName name(PSP_FOL, fileName, PSP_EXT);
	if (!prepareForSaving(max_size_saving, size_type, name, binary)) {
		std::cout << "Saving Simulation parameters as " << name.getCompleted() << " : Failed" << std::endl;
		return;
	}
	
	if (binary)
		save(&param);
	else {
		save_in_string("n_threads", param.n_threads);
		save_in_string("max_part", param.max_part);
		save_in_string("n_part_start", param.n_part_start);
		save_in_string("pps", param.pps);
		file << '\n';

		save_in_string("dt", param.dt);
		save_in_string("radii", param.radii);
		save_array_in_string("spawn_speed", param.spawn_speed, 2);
		save_in_string("temperature", param.temperature);
		file << '\n';

		save_in_string("grav_force", param.grav_force);
		save_in_string("grav_force_decay", param.grav_force_decay);
		save_array_in_string("grav_center", param.grav_center, 2);
		save_in_string("static_speed_block", param.static_speed_block);
		save_in_string("fluid_friction_coef", param.fluid_friction_coef);
		file << '\n';
		
		// User forces
		save_in_string("translation_force", param.translation_force);
		save_in_string("rotation_force", param.rotation_force);
		save_in_string("range", param.range);
		file << '\n';
		
		save_in_string("cs", param.cs);
		file << '\n';

		save_in_string("pp_repulsion", param.pp_repulsion);
		save_in_string("pp_repulsion_2lev", param.pp_repulsion_2lev);
		save_in_string("pp_repulsion_phyacc", param.pp_repulsion_phyacc);
		save_in_string("pp_energy_conservation", param.pp_energy_conservation);
		file << '\n';

		save_in_string("apl_point_gravity", param.apl_point_gravity);
		save_in_string("apl_point_gravity_invSquared", param.apl_point_gravity_invSquared);
		save_in_string("apl_gravity", param.apl_gravity);
		save_in_string("apl_vibrate", param.apl_vibrate);
		save_in_string("apl_fluid_friction", param.apl_fluid_friction);
		save_in_string("apl_static_friction", param.apl_static_friction);
		file << '\n';

		save_in_string("apl_pp_collision", param.apl_pp_collision);
		save_in_string("apl_ps_collision", param.apl_ps_collision);
		save_in_string("apl_world_border", param.apl_world_border);
		save_in_string("apl_zone", param.apl_zone);
		file << '\n';

		save_in_string("pp_collision_fun", param.pp_collision_fun);
		file << "#\tBASE=0, TLEV=1, PHYACC=2\n";
		save_in_string("ps_collision_fun", param.ps_collision_fun);
		file << "#\tBASE=0, REBOUND=1\n";
		save_in_string("world_border_fun", param.world_border_fun);
		file << "#\tBASE=0, REBOUND=1\n";
	}
	done();
	std::cout << "Saving Simulation parameters as " << name.getCompleted() << " : Success" << std::endl;
}

int8_t SaveLoader::loadParam(PSparam& param, std::string fileName) {
	FullFileName name(PSP_FOL, fileName, PSP_EXT);
	if (!prepareForLoading(name)){
		std::cout << "Loading Simulation parameters from " << name.getCompleted() << " : Failed" << std::endl;
		return -1; // return -1 if opening the file fails
	}

	bool res = 0;
	if (file_in_binary)
		load(&param);
	else {
		short temp;

		parse_map map = map_file();

		param = PSparam::Default; // Setting parameters as default so if a loading fails the value reverts back to its default, same if I forget to call for loading of a member.
		res |= !load_from_map(map, "n_threads", param.n_threads);
		res |= !load_from_map(map, "max_part", param.max_part);
		res |= !load_from_map(map, "n_part_start", param.n_part_start);
		res |= !load_from_map(map, "pps", param.pps);

		res |= !load_from_map(map, "dt", param.dt);
		res |= !load_from_map(map, "radii", param.radii);
		res |= !load_from_map(map, "spawn_speed", param.spawn_speed, 2);
		res |= !load_from_map(map, "temperature", param.temperature);

		res |= !load_from_map(map, "grav_force", param.grav_force);
		res |= !load_from_map(map, "grav_force_decay", param.grav_force_decay);
		res |= !load_from_map(map, "grav_center", param.grav_center, 2);
		res |= !load_from_map(map, "static_speed_block", param.static_speed_block);
		res |= !load_from_map(map, "fluid_friction_coef", param.fluid_friction_coef);

		res |= !load_from_map(map, "translation_force", param.translation_force);
		res |= !load_from_map(map, "rotation_force", param.rotation_force);
		res |= !load_from_map(map, "range", param.range);

		res |= !load_from_map(map, "cs", param.cs);

		res |= !load_from_map(map, "pp_repulsion", param.pp_repulsion);
		res |= !load_from_map(map, "pp_repulsion_2lev", param.pp_repulsion_2lev);
		res |= !load_from_map(map, "pp_repulsion_phyacc", param.pp_repulsion_phyacc);
		res |= !load_from_map(map, "pp_energy_conservation", param.pp_energy_conservation);

		res |= !load_from_map(map, "apl_point_gravity", param.apl_point_gravity);
		res |= !load_from_map(map, "apl_point_gravity_invSquared", param.apl_point_gravity_invSquared);
		res |= !load_from_map(map, "apl_gravity", param.apl_gravity);
		res |= !load_from_map(map, "apl_vibrate", param.apl_vibrate);
		res |= !load_from_map(map, "apl_fluid_friction", param.apl_fluid_friction);
		res |= !load_from_map(map, "apl_static_friction", param.apl_static_friction);

		res |= !load_from_map(map, "apl_pp_collision", param.apl_pp_collision);
		res |= !load_from_map(map, "apl_ps_collision", param.apl_ps_collision);
		res |= !load_from_map(map, "apl_world_border", param.apl_world_border);
		res |= !load_from_map(map, "apl_zone", param.apl_zone);

		
		res |= !load_from_map(map, "pp_collision_fun", param.pp_collision_fun);
		res |= !load_from_map(map, "ps_collision_fun", param.ps_collision_fun);
		res |= !load_from_map(map, "world_border_fun", param.world_border_fun);

	}

	param.n_part_start = std::min(param.n_part_start, param.max_part);

	done();
	std::cout << "Loading Simulation parameters as " << name.getCompleted() << " : Success" << (res ? " (partial)" : "") << std::endl;
	return res;
}


void SaveLoader::set_compression(SLinfoPos compression, uint32_t max_particles) {
	delete_comp_data();
	comp_mode = compression;
	switch (comp_mode.compressionMode()) {
		case SLinfoPos::compression_mode::Discreet:
			comp_data = new uint16_t[4*max_particles];
			break;
		case SLinfoPos::compression_mode::PosOnly:
			comp_data = new float[2*max_particles];
			break;
		case SLinfoPos::compression_mode::Both:
			comp_data = new uint16_t[2*max_particles];
			break;
		default: // In the case of default we directly save the particle data array
			break;
	}
}

void SaveLoader::delete_comp_data() {
	if (comp_data) delete[]((uint16_t*)comp_data); // no destructor needs to be called no matter the level of compression
	comp_data = nullptr;
}


void SaveLoader::prepareSavePos(uint32_t max_size_saving, ByteSize size_type, SLinfoPos compression, uint32_t max_particles, World& world, double min_delta_save_time_) {
	min_delta_save_time = min_delta_save_time_;
	world_size[0] = world.getSize(0);
	world_size[1] = world.getSize(1);
	
	set_compression(compression, max_particles);
	std::cout << "\tSaveLoader::prepareSavePos, saving at compression " << (short)comp_mode.compressionMode() << std::endl;
	if ( !prepareForSaving(max_size_saving, size_type, FullFileName(POS_FOL, comp_mode.posName(), POS_EXT), true) ) {
		std::cout << "\tSaving positions in " << FullFileName(POS_FOL, comp_mode.posName(), POS_EXT).getCompleted() << " : Failed" << std::endl;
		return;
	}

	save(&comp_mode.comp_discreet);
	save(&comp_mode.comp_out_speed);
	save(&world_size[0]);
	save(&world_size[1]);
	if (!comp_mode.isCompOutSpeed()) {
		std::cout << "\tmax speed : " << max_speed[0]  << ", " << max_speed[1] << std::endl;
		save(&max_speed);
	}
}

void SaveLoader::prepareLoadPos(uint32_t max_particles, SLinfoPos known) {
	std::cout << "SaveLoader::prepareLoadPos" << std::endl;
	prepareForLoading(FullFileName(POS_FOL, known.posName(), POS_EXT));
	load(&known.comp_discreet);
	load(&known.comp_out_speed);
	load(&world_size[0]);
	load(&world_size[1]);
	std::cout << "\tPositions saved in a world of size [" << world_size[0] << ", " << world_size[1] << "]. Compression level " << (short)known.compressionMode() << std::endl;
	set_compression(known, max_particles);
	if (!comp_mode.isCompOutSpeed()) {
		load(&max_speed);
		std::cout << "\tloaded max speed "  <<  max_speed[0] << ", " << max_speed[1] << std::endl;
	}
}

void SaveLoader::savePos(Particle* particle_array, uint32_t part_arr_size, double time) {
	// std::cout << "SaveLoader::savePos, " << time << " - " << time_of_last_save << " < " << min_delta_save_time << "\n";

	if (time - time_of_last_save < min_delta_save_time) return;
	time_of_last_save = time;
	save(&time);

	size_t byte_size_obj;
	float discreet_multiplier[2][2] = {
		65536/world_size[0],
		65536/world_size[1],
		32768/max_speed[0], // max measurable speed
		32768/max_speed[1] // The higher, the less saving is precise
	};
	switch (comp_mode.compressionMode()) {
		case SLinfoPos::compression_mode::Discreet:
			byte_size_obj = 4*sizeof(uint16_t);
			for (uint32_t p=0; p<part_arr_size; p++) {
				((uint16_t*)comp_data)[4*p   ] = particle_array[p].position[0] * discreet_multiplier[0][0];
				((uint16_t*)comp_data)[4*p +1] = particle_array[p].position[1] * discreet_multiplier[0][1];
				(( int16_t*)comp_data)[4*p +2] = particle_array[p].speed[0]    * discreet_multiplier[1][0];
				(( int16_t*)comp_data)[4*p +3] = particle_array[p].speed[1]    * discreet_multiplier[1][1];
			}
			break;
		case SLinfoPos::compression_mode::PosOnly:
			byte_size_obj = 2*sizeof(float);
			for (uint32_t p=0; p<part_arr_size; p++) {
				((float*)comp_data)[2*p   ] = particle_array[p].position[0];
				((float*)comp_data)[2*p +1] = particle_array[p].position[1];
			}
			break;
		case SLinfoPos::compression_mode::Both:
			byte_size_obj = 2*sizeof(uint16_t);
			for (uint32_t p=0; p<part_arr_size; p++) {
				((uint16_t*)comp_data)[2*p   ] = particle_array[p].position[0] * discreet_multiplier[0][0];
				((uint16_t*)comp_data)[2*p +1] = particle_array[p].position[1] * discreet_multiplier[0][1];
			}
			break;
		default:
			byte_size_obj = 0;
			save_array(particle_array, sizeof(Particle), part_arr_size);
			break;
	}
	if (byte_size_obj) save_array(comp_data, byte_size_obj, part_arr_size);
}


uint32_t SaveLoader::loadPos(Particle* particle_array, uint32_t arr_size, double* time) {
	// std::cout << "SaveLoader::loadPos" << std::endl;
	// std::cout << "bout to load time : tellg=" << file.tellg() << '\n';
	load(time);
	// if (file.fail()) {
	// 	file.clear();
	// 	file.seekg(0, std::ios_base::beg);
	// 	std::cout << "load time failure. tellg=" << file.tellg() << '\n';
	// }
	uint32_t loaded_obj = 0;
	float discreet_multiplier[2][2] = {
		world_size[0] / 65536,
		world_size[1] / 65536,
		max_speed[0]/32768, // max measurable speed
		max_speed[1]/32768 // The higher, the less saving is precise
	};
	switch (comp_mode.compressionMode()) {
		case SLinfoPos::compression_mode::Discreet:
			load(comp_data, 4*sizeof(uint16_t), arr_size, &loaded_obj);
			for (uint32_t p=0; p<loaded_obj; p++) {
				particle_array[p].position[0] = ((uint16_t*)comp_data)[4*p   ] * discreet_multiplier[0][0];
				particle_array[p].position[1] = ((uint16_t*)comp_data)[4*p +1] * discreet_multiplier[0][1];
				particle_array[p].speed[0]    = (( int16_t*)comp_data)[4*p +2] * discreet_multiplier[1][0];
				particle_array[p].speed[1]    = (( int16_t*)comp_data)[4*p +3] * discreet_multiplier[1][1];
			}
			break;
		case SLinfoPos::compression_mode::PosOnly:
			load(comp_data, 2*sizeof(float), arr_size, &loaded_obj);
			for (uint32_t p=0; p<loaded_obj; p++) {
				particle_array[p].position[0] = ((float*)comp_data)[2*p   ];
				particle_array[p].position[1] = ((float*)comp_data)[2*p +1];
			}
			break;
		case SLinfoPos::compression_mode::Both:
			if (! load(comp_data, 2*sizeof(uint16_t), arr_size, &loaded_obj)) {
				// I am trying to make position loading loop when reaching the end of the file.
				// std::cout << "found end of file in loading, pos in file : " << file.tellg() << std::endl;
				// file.clear();
				// file.seekg(0, std::ios_base::beg);
				// std::cout << "new pos : " << file.tellg() << std::endl;
			};
			for (uint32_t p=0; p<loaded_obj; p++) {
				particle_array[p].position[0] = ((uint16_t*)comp_data)[2*p   ] * discreet_multiplier[0][0];
				particle_array[p].position[1] = ((uint16_t*)comp_data)[2*p +1] * discreet_multiplier[0][1];
			}
			break;
		default:
			load(particle_array, sizeof(Particle), arr_size, &loaded_obj);
			break;
	}
	return loaded_obj;
}

void SaveLoader::proove_you_work_please(bool binary) {
	std::string name[2];
	name[0] = "SaveLoader_test_before"; 
	name[1] = "SaveLoader_test_after"; 

	PSparam psParam[2];
	psParam[0] = PSparam::Default;
	psParam[0].n_threads = 24;
	psParam[0].apl_point_gravity_invSquared = -1;
	psParam[0].cs = 5;
	psParam[0].world_border_fun = 1;
	saveParam(psParam[0], 10, KB, name[0], binary);
	loadParam(psParam[1], name[0]);
	saveParam(psParam[1], 10, KB, name[1], binary);
	if (psParam[0] == psParam[1]) {}

	WorldParam worldParam[2];
	worldParam[0] = WorldParam::Default;
	{
		worldParam[0].cellSize[0] = 18;
		worldParam[0].size[1] = 520;
		World world(worldParam[0]);
		world.add_segment(10, 20, 30, 40);
		world.add_segment(140, 130, 120, 110);

		world.add_zone(0, 0, 0, 20, 20);
	
		saveWorld(world, 10, KB, name[0], binary);
	}

	loadParam(worldParam[1], name[0]);

	{
		World world(worldParam[1]);
		loadWorldSegNZones(world, name[0]);
	
		saveWorld(world, 10, KB, name[1], binary);
	}

}