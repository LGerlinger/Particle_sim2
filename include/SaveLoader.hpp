#pragma once

#include "FileHandler.hpp"
#include "Particle.hpp"
#include "World.hpp"

#include <cmath>
#include <cstring>

class PSparam;
class Particle_simulator;

class SLinfoPos {
public :
	static const uint8_t fileNameSize = 28;

	bool load_pos;
	bool save_pos;

	bool comp_out_speed;
	bool comp_discreet;
	char posFileName[fileNameSize];

public :
	inline bool isLoadPos() {return load_pos;};
	inline bool isSavePos() {return save_pos;};

	inline bool isCompOutSpeed() {return comp_out_speed;}; //< Whether the speed is NOT to be included
	inline bool isCompDiscreet() {return comp_discreet;};

	enum compression_mode {Normal, Discreet, PosOnly, Both};
	inline uint8_t compressionMode() {
		return comp_discreet + 2*comp_out_speed;
	};

	std::string posName() {return posFileName;};

	/**
	@return A SLinfoPos ordering to do nothing. File names are empty.
	*/
	static inline SLinfoPos Lazy() {
		SLinfoPos lazy;
		memset(&lazy, 0, sizeof(SLinfoPos));
		lazy.posFileName[0] = '\0';
		return lazy;
	};
};


class SLinfo : public SLinfoPos {
public :
	bool load_world;
	bool load_simP;

	bool save_world;
	bool save_simP;

	char worldFileName[fileNameSize];
	char simPFileName[fileNameSize];

public :
	inline bool isLoadWorld() {return load_world;};
	inline bool isLoadSimP()  {return load_simP;};

	inline bool isSaveWorld() {return save_world;};
	inline bool isSaveSimP()  {return save_simP;};

	std::string worldName() {return worldFileName;};
	std::string simPName()  {return simPFileName;};

	/**
	@return True if any order of loading or saving is set to true.
	*/
	bool does_anything() {
		return load_world || load_simP || load_pos || save_world || save_simP || save_pos;
	}

	/**
	@return A SLinfo ordering to do nothing. File names are empty.
	*/
	static inline SLinfo Lazy() {
		SLinfo lazy;
		memset(&lazy, 0, sizeof(SLinfo));
		lazy.posFileName[0] = '\0';
		lazy.worldFileName[0] = '\0';
		lazy.simPFileName[0] = '\0';
		return lazy;
	};
};

/**
* SaveLoader interacts with files through its parent class FileHandler and its knowledge of classes such as World, WorldParam, Particle and PSparam to save or load data for the simulation.
* This is the class that chooses what should be written, where and how.
*/
class SaveLoader : protected FileHandler {
private :
	double time_of_last_save = -INFINITY; //< When was the last time particle positions were saved. Used so dt can be decreased without changing the dt between saves.
	double min_delta_save_time = 0; //< Minimum simulation time between 2 particle positions saves.

	SLinfoPos comp_mode; //< SaveLoader needs to remember what level of compression is applied on Particles because 1) a file should only use 1 level of compression and 2) some compression level use specific buffer.
	float world_size[2] = {1, 1};
	float max_speed[2] = {20000, 20000}; //< Maximum savable speed. The higher, the less saving is precise.
	void* comp_data = nullptr; //< Particle buffer for reading/writing. Might be an array of Particles, floats or uint16_t.
	void delete_comp_data();
	size_t loadPos_start_file_offset = 0; //< Position of the file read pointer after prepareLoadPos.

	/**
	* @brief Sets the compression level and prepares the buffer comp_data.
	* @param compression
	* @param max_particles Maximum number of Particles that will be saved at a time. Needed so it can set the particle buffer at the right size.
	*/
	void set_compression(SLinfoPos compression, uint32_t max_particles);

public :
	~SaveLoader();

	inline SLinfoPos getCompression() {return comp_mode;};

	/**
	* @brief Loads start up orders for reading/saving and from which files.
	* @details Reads the data from the unique start up file (loading_orders.sli). Orders are about :
	* - whether or not to load world parameters
	* - from which file name
	* - if those parameters should be saved in a 
	* - whether or not to load simulation parameters
	* - from which file name
	* @return In order of priority :
	* -1 if the file couldn't be opened.
	* 0 if all the data could be extracted from the file.
	* 1 if at least one data element couldn't be extracted from the file.
	*/
	int8_t load_SLinfo(SLinfo& info);

	/**
	* @brief Sets Wparam and Sparam according to loading orders from the unique start up file.
	* If order says to save Simulation parameters, said parameters will be saved in binary, with the same filename + "-bin".
	* @details Calls @see int8_t load_SLinfo(SLinfo&) to get the loading orders.
	* Then if required by said orders :
	* Calls @see int8_t loadParam(WorldParam&, std::string) to load World parameters.
	* Calls @see int8_t loadParam(PSparam&, std::string) to load Simulation parameters.
	* If loading isn't required or possible parameters are set to default.
	* @return loading orders, loaded from the start up file.
	* @see int8_t loadParam(WorldParam&, std::string)
	* @see int8_t loadParam(PSparam&, std::string)
	*/
	SLinfo setParameters(WorldParam& Wparam, PSparam& Sparam);
	/**
	* @brief Saves the world and simulator parameters in 2 different files.
	* @param fileName Name of the saving file. Path and extension are automatically be added.
	* @param binary 'true' if the files should be written in binary, 'false' for full text.
	* @see void saveWorld(World&, uint32_t, ByteSize, std::string, bool)
	* @see void saveParam(PSparam&, uint32_t, ByteSize, std::string, bool)
	*/
	void saveParameters(World& world, Particle_simulator& sim, std::string fileName = "", bool binary = 1);

	// World

	/**
	* @brief Saves the world parameters, its segments and zones in the file fileName.
	* @details World parameters are defined by class WorldParam's members.
	* Segment positions are absolute coordinates (point A, point B).
	* Zone positions are absolute and relative coordinates : (absolute top left, relative size).
	* @param world world to save.
	* @param max_size_saving Size limit of the file to write in size_type unit. While writing, if the writing would lead the file to be bigger than max_size_saving, then nothing is written.
	* @param size_type max_size_saving's unit (in B, kB, MB, GB).
	* @param fileName Name of the saving file. Path and extension are automatically be added. If no fileName is provided, it will use the time and date instead.
	* @param binary 'true' if the files should be written in binary, 'false' for full text.
	*/
	void saveWorld(World& world, uint32_t max_size_saving, ByteSize size_type, std::string fileName = "", bool binary = 1);
	/**
	* @brief Loads the world parameters into param from the file fileName.
	* @details World parameters are defined by class WorldParam's members.
	* @param param world parameters in which to load file data.
	* @param fileName Name of the loading file. Path and extension are automatically be added. If no fileName is provided, searches for latest file with same path and extension.
	* @return In order of priority :
	* -1 if the file couldn't be opened.
	* 0 if all the data could be extracted from the file.
	* 1 if at least one data element couldn't be extracted from the file.
	*/
	int8_t loadParam(WorldParam& param, std::string fileName = "");
	/**
	* @brief Loads the world's segments and zone into world from the file fileName.
	* @details Segment positions are absolute coordinates (point A, point B).
	* Zone positions are absolute and relative coordinates : (absolute top left, relative size).
	* @param world World in which to load file data.
	* @param fileName Name of the loading file. Path and extension are automatically be added. If no fileName is provided, searches for latest file with same path and extension.
	* @return In order of priority :
	* -1 if the file couldn't be opened.
	* 0 if all the data could be extracted from the file.
	* 1 if at least one data element couldn't be extracted from the file.
	*/
	int8_t loadWorldSegNZones(World& world, std::string fileName = "");

	// Particle simulator

	/**
	* @brief Saves the simulation parameters in the file fileName.
	* @details Simulation parameters are defined by class PSparam's members.
	* @param param Simualtion parameters to save.
	* @param max_size_saving Size limit of the file to write in size_type unit. While writing, if the writing would lead the file to be bigger than max_size_saving, then nothing is written.
	* @param size_type max_size_saving's unit (in B, kB, MB, GB).
	* @param fileName Name of the saving file. Path and extension are automatically be added. If no fileName is provided, it will use the time and date instead.
	* @param binary 'true' if the files should be written in binary, 'false' for full text.
	*/
	void saveParam(PSparam& param, uint32_t max_size_saving, ByteSize size_type, std::string fileName = "", bool binary = 1);
	/**
	* @brief Loads the simulation parameters into param from the file fileName.
	* @details Simulation parameters are defined by class PSparam's members.
	* @param param Simulation parameters in which to load file data.
	* @param fileName Name of the loading file. Path and extension are automatically be added. If no fileName is provided, searches for latest file with same path and extension.
	* @return In order of priority :
	* -1 if the file couldn't be opened.
	* 0 if all the data could be extracted from the file.
	* 1 if at least one data element couldn't be extracted from the file.
	*/
	int8_t loadParam(PSparam& param, std::string fileName = "");

	// Particle positions
	
	/**
	* @brief Opens a file to save Particle positions later and prepares for continuus saving.
	* @details A data buffer may be created, compression level, world size and maximum speed are written in the file.
	* Particle position files are always written in binary.
	* @param max_size_saving Size limit of the file to write in size_type unit. While writing, if the writing would lead the file to be bigger than max_size_saving, then nothing is written.
	* @param size_type max_size_saving's unit (in B, kB, MB, GB).
	* @param compression Describes the compression level that will be used and the name of the file where to save.
	* @param max_particles Maximum number of Particles that will be saved at once.
	* @param min_delta_save_time_ Minimum simulation time between 2 particle positions saves.
	* @see void set_compression(SLinfoPos, uint32_t)
	*/
	void prepareSavePos(uint32_t max_size_saving, ByteSize size_type, SLinfoPos compression, uint32_t max_particles, World& world, double min_delta_save_time_);
	/**
	* @brief Opens a file to load Particle positions later and prepares for continuus loading.
	* @details A data buffer may be created, compression level, passed world size and maximum speed are read from the file.
	* Particle position files are always written in binary. 
	* @param max_particles Maximum number of Particles that will be loaded at once.
	* @param known Is used to give the file name. It will be set into @see comp_mode, but its compression levels will be changed to whatever is in the file
	* @see void set_compression(SLinfoPos, uint32_t)
	*/
	void prepareLoadPos(uint32_t max_particles, SLinfoPos known);

	/**
	* @brief Writes the position and speed (so just Particles) into the opened file.
	* @details @see void prepareSavePos(uint32_t, ByteSize, SLinfoPos, uint32_t, double) must have been called before hand (and the file shouldn't be closed obviously).
	* The compression level used is described by @see comp_mode.
	* If comp_mode.comp_out_speed is true, then the speed of Particles isn't saved.
	* If comp_mode.comp_discreet is true, then the position (and maybe speed) of Particles will be saved as uint16_t (int16_t for speed) instead of floats.
	* @param particle_array Array of Particles to save.
	* @param part_arr_size Number of Particles to save (no check is performed to ensure it won't seg fault).
	* @param time Simulation time at the moment of call. Used so every consecutive saves can be associated to a moment in the simulation. 
	*/
	void savePos(Particle* particle_array, uint32_t part_arr_size, double time);
	/**
	* @brief Reads the position and speed (so just Particles) from the opened file.
	* @details @see void prepareLoadPos(uint32_t max_particles, SLinfoPos known) must have been called before hand (and the file shouldn't be closed obviously).
	* The compression level used is described by @see comp_mode.
	* If comp_mode.comp_out_speed is true, then the speed of Particles isn't found in the file.
	* If comp_mode.comp_discreet is true, then the position (and maybe speed) of Particles found in the file are as uint16_t (int16_t for speed) instead of floats.
	* @param particle_array Array of Particles in which to load the data.
	* @param part_arr_size Size of the passed array.
	* @param time Simulation time at the moment the Particles were saved. Used so every consecutive saves can be associated to a moment in the simulation.
	* @return The number of Particles loaded. If loading was unsuccessful (e.g. reached en of position file), returns NULLPART instead.
	*/
	uint32_t loadPos(Particle* particle_array, uint32_t arr_size, double* time);

	/**
	* @brief Resets the position reading to the initial state.
	* @details Sets the file read pointer back to the position it was after reading the metadata of the positions file.
	* @see loadPos_start_file_offset
	*/
	void resetPosLoading();

	/**
	* @brief Purely a debug function to check if saving and loading world and simulation works.
	*/
	void proove_you_work_please(bool binary);
};