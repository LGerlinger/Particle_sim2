#include "FileHandler.hpp"

#include <iostream>
#include <iomanip>
#include <filesystem>
#include <ctime>
#include <vector>

namespace fs = std::filesystem;

#define SAVE_FOLDER "saves/"
#define BIN_BYTE 127 // [DEL] character

FullFileName::FullFileName() : directory(""), name(""), extension("") {}
FullFileName::FullFileName(std::string name_) : directory(""), name(name_), extension("") {}
FullFileName::FullFileName(std::string dir_, std::string name_, std::string ext) : directory(dir_), name(name_), extension(ext) {}

std::string FullFileName::getCompleted() {
	return directory + name + extension;
}


FileHandler::~FileHandler() {
	std::cout << "FileHandler::~FileHandler()" << std::endl;
	done();
}


bool FileHandler::prepareForSaving(uint32_t max_size_saving, ByteSize size_type, FullFileName fileName, bool binary) {
	max_writable_size = (uint64_t)max_size_saving * size_type;
	file_in_binary = binary;

	// Getting time of day
	std::ostringstream oss;
	if (!fileName.name.length()) { // If no fileName was provided or it corresponds to a directory
		auto t = std::time(nullptr);
		auto tm = *std::localtime(&t);
		oss << SAVE_FOLDER << fileName.directory << std::put_time(&tm, "%d-%m-%Y %H-%M-%S") << fileName.extension;
	} else {
		oss << SAVE_FOLDER << fileName.getCompleted();
	}
	std::string name = oss.str();

	if (binary) file.open(name, std::ios::out | std::ios::trunc | std::ios::binary);
	else        file.open(name, std::ios::out | std::ios::trunc);

	if (file.is_open()) {
		file.clear();
		uint8_t bin_byte = binary ? BIN_BYTE : '\n';
		written = 1;
		file.write((char*)&bin_byte, 1); // writing whether the file will be in text or binary in the first byte.
		if (file.good()) return true;
	}
	std::cout << "Failed opening file : " << name << std::endl;
	return false;
}

bool FileHandler::prepareForLoading(FullFileName fileName) {
	fileName.directory = SAVE_FOLDER + fileName.directory;

	// If no name has been provided we look for the most recent file saved
	bool found_file = false;
	if (!fileName.name.length()) {
		std::cout << "FileHandler::prepareForLoading : provided name is empty. Searching for the most recent file in " << fileName.directory << " with extension " << fileName.extension << std::endl;
		std::time_t latest_time = 0;

		for (const auto & entry : fs::directory_iterator(fileName.directory)) {
			if (entry.is_regular_file() && entry.path().extension().string() == fileName.extension) {
				auto temps_modification = fs::last_write_time(entry);
				auto temps_c = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now() + (temps_modification - fs::file_time_type::clock::now()));

				if (temps_c > latest_time) {
					latest_time = temps_c;
					// entry.path() already contains the directory, name and extension information. I don't feel like separating it in fileName considering it will be used together again in opening the file.
					fileName.name = entry.path().string();
					found_file = true;
				}
			}
		}
		if (found_file) std::cout << "Found " << fileName.name << std::endl;
	}
	if (found_file) {
		fileName.directory = "";
		fileName.extension = "";
	}
	file.open(fileName.getCompleted(), std::ios::in | std::ios::binary);
	
	if (file.is_open()) {
		uint8_t bin_byte;
		file.read((char*)&bin_byte, 1);
		file_in_binary = bin_byte == BIN_BYTE;
		return true;
	} else {
		std::cout << "Failed opening file : " << fileName.getCompleted() << std::endl;
		return false;
	}
}

void FileHandler::done() {
	if (file.is_open())	file.close();
}

bool FileHandler::save(void* obj, size_t byte_size_obj) {
	if (written + byte_size_obj <= max_writable_size) { // Enough space allowed left to write.
		written += byte_size_obj;
		file.write((char*)obj, byte_size_obj);
		return file.good();
	}
	else return false;
}


bool FileHandler::save_array(void* array, size_t byte_size_obj, uint32_t array_size) {
	uint64_t array_size_byte = array_size*(uint64_t)byte_size_obj;
	if (written + array_size_byte + sizeof(size_t) + sizeof(uint32_t) <= max_writable_size) { // Enough space allowed left to write.
		written += array_size_byte + sizeof(size_t) + sizeof(uint32_t);
		file.write((char*)&byte_size_obj, sizeof(size_t));
		file.write((char*)&array_size, sizeof(uint32_t));
		file.write((char*)array, array_size_byte);

		return file.good();
	}
	else return false;
}


void FileHandler::load(void* obj, size_t byte_size_obj) {
	file.read((char*)obj, byte_size_obj);
}

bool FileHandler::load(void* array, size_t byte_size_obj, uint32_t max_array_size, uint32_t* loaded_obj) {
	size_t read_byte_size_obj=0;
	auto original_position = file.tellg();
	file.read((char*)&read_byte_size_obj, sizeof(size_t));
	if (read_byte_size_obj != byte_size_obj) {
		if (file.eof()) std::cout << "FileHandler::load : reached end of file\n"; 
		else std::cout << "FileHandler::load : incompatible object size needed and read.\n";
		//  Position is " << file.tellg() << "  while original pos is " << original_position << "  fail flag ? " << file.fail() << std::endl;
		// file.seekg(original_position, std::ios_base::beg); // Returning to original position
		// std::cout << "moved to " << file.tellg() << std::endl;
		return false;
	}

	uint32_t read_array_size=0;
	file.read((char*)&read_array_size, sizeof(uint32_t));
	uint64_t bytes_saved = read_array_size*(uint64_t)byte_size_obj;
	
	if (read_array_size > max_array_size) { // More object saved than can be loaded now. Load what we can then move to the end of the array in the file
		*loaded_obj = max_array_size;
		file.read((char*)array, max_array_size*byte_size_obj);
		file.seekg((read_array_size-max_array_size) * byte_size_obj, std::ios_base::cur);
	}
	else {
		*loaded_obj = read_array_size;
		file.read((char*)array, read_array_size*(uint64_t)byte_size_obj);
	}
	return !file.fail();
}

FileHandler::parse_map FileHandler::map_file(uint16_t start_section, uint16_t end_section) {
	// std::cout << "FileHandler::map_file()" << std::endl;
	if (end_section == (uint16_t)-1) end_section = start_section+1;
	parse_map map;
	
	file.clear();
	file.seekg(0, std::ios_base::beg);
	uint16_t section=0;
	while (section < start_section && !file.eof()) {
		file.ignore(std::numeric_limits<std::streamsize>::max(), '^');

		if (file.fail()) { // reached EOF (or failed somehow) before reaching section start_section
			file.clear();
			return map; // returning empty map
		}
		// verifying the symbol '^' is at the start of a line
		file.seekg(-2, std::ios_base::cur);
		if (file.peek() == '\n') section++; 
		file.seekg(2, std::ios_base::cur);
	}

	std::string key;
	std::streampos value[2];
	int c;
	while (file.peek() != EOF && section < end_section) {
		c = file.peek();
		if (c == '#' || c == '\n') { // comment or empty line
			file.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
		} else if (c == '^') { // End of string data on file
			section ++;
			file.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
		} else {
			std::getline(file, key, '=');
			value[0] = file.tellg();
			file.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
			value[1] = file.tellg();
			if (file.eof() && !file.bad()) { // Read untill the end of the file
				// std::cout << "value is " << value[1] << "\n";
				file.clear();
				file.seekg(0, std::ios_base::end);
				value[1] = file.tellg();
			}
			map[key] = {value[0], value[1]};
		}
	}

	// Showing the map
	// std::cout << "mapping" << std::endl;
	// for (auto elem : map) {
	// 	std::cout << "\t'" << elem.first << "' : " << elem.second[0] << ", " << elem.second[1] << '\n';
	// }

	return map;
}

// TEMPLATE SPECIALIZATIONS because signed and unsigned char don't work well. During insertion/extraction, file sees it as a variation of char while I want a number.

template<> bool FileHandler::save_array_in_string(std::string name, uint8_t* array, uint32_t arr_size) {
	std::vector<int16_t> translator(arr_size);
	for (uint32_t i=0; i<arr_size; i++) translator[i] = array[i];
	return save_array_in_string(name, translator.data(), arr_size);
}
template<> bool FileHandler::save_array_in_string(std::string name, int8_t* array, uint32_t arr_size) {
	std::cout<<"passing by template implementation"<<std::endl;
	std::vector<int16_t> translator(arr_size);
	for (uint32_t i=0; i<arr_size; i++) translator[i] = array[i];
	return save_array_in_string(name, translator.data(), arr_size);
}


template<> bool FileHandler::load_from_map<int8_t>(FileHandler::parse_map& map, std::string name, int8_t* array, uint32_t arr_size) {
	std::vector<int16_t> translator(arr_size);
	for (uint32_t i=0; i<arr_size; i++) translator[i] = array[i];
	bool ret = load_from_map(map, name, translator.data(), arr_size);
	for (uint32_t i=0; i<arr_size; i++) array[i] = translator[i];
	return ret;
}
template<> bool FileHandler::load_from_map<uint8_t>(FileHandler::parse_map& map, std::string name, uint8_t* array, uint32_t arr_size) {
	std::vector<int16_t> translator(arr_size);
	for (uint32_t i=0; i<arr_size; i++) translator[i] = array[i];
	bool ret = load_from_map(map, name, translator.data(), arr_size);
	for (uint32_t i=0; i<arr_size; i++) array[i] = translator[i];
	return ret;
}

template<> bool FileHandler::load_from_map<char>(FileHandler::parse_map& map, std::string name, char* array, uint32_t arr_size) {
	std::string stringGetter;
	bool ret = load_from_map(map, name, &stringGetter, 1);
	if (stringGetter.size() == 0) return false;

	stringGetter.copy(array, arr_size-1);
	array[arr_size-1] = '\0';
	
	return ret;
}