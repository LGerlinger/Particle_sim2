#pragma once

#include <fstream>
#include <iostream>
#include <map>
#include <array>
#include <cstdint>

class FullFileName {
public :
	std::string directory;
	std::string name;
	std::string extension;
	
	/**
	@brief Constructor, sets directory, name and extension to "" (empty string).
	*/
	FullFileName();
	/**
	@brief Constructor, sets directory and extension to "" (empty string) and name to name_.
	*/
	FullFileName(std::string name_);
	FullFileName(std::string dir_, std::string name_, std::string ext);

	/**
	@return concatenation of (directory + name + extension) 
	*/
	std::string getCompleted();
};

/**
A class made to interact with a file, in a more formatted way if needed.
Files can be written and read in either binary or full text, each with its format.
*/
class FileHandler {
public :
	using strpos_pair = std::array<std::streampos, 2>;
	using parse_map = std::map<std::string, strpos_pair>;
	enum ByteSize {B=1, KB=1000, MB=1000000, GB=1000000000};
	
protected :
	std::fstream file;
	uint64_t written = 0;
	uint64_t max_writable_size; //< Exist so the object automatically stops writing and filling the computer.

	bool file_in_binary = true;

public :
	~FileHandler();

	/**
	* @brief Opens the file fileName or creates it if it doesn't already exist.
	* @details Writes whether or not the file will be in binary in the first byte.
	* @param max_size_saving Futur file size limit in size_type unit. While writing, if the writing would lead the file to be bigger than max_size_saving, then nothing is written.
	* @param size_type max_size_saving's unit (in B, kB, MB, GB).
	* @param fileName Path+Name+extension of the file to be opened. If no .name is provided, it will use the time and date instead.
	* @param binary Whether the user has the intention to write into the file in binary or in full text.
	* @return true if the file is successfully opened, false otherwise.
	*/
	bool prepareForSaving(uint32_t max_size_saving, ByteSize size_type, FullFileName fileName, bool binary = 1);
	
	/**
	* @brief Opens the file fileName or searches for the latest one.
	* @details Reads the first byte to check if the file is written in binary or in full text. If it is in binary : file_in_binary=true, else file_in_binary=false.
	* @param fileName Path+Name+extension of the file to be opened. If no .name is provided, it will look for the latest file in fileName.path with the extension fileName.extension.
	* @return true if the file is successfully opened, false otherwise.
	*/
	bool prepareForLoading(FullFileName fileName);
	bool isFileBinary() {return file_in_binary;};

	/**
	* @brief Writes obj in file.
	* @details Only writes if there is enough space before max_writable_size. Increases written by byte_size_obj.
	* @return true if writing was successful, false otherwise.
	*/
	bool save(void* obj, size_t byte_size_obj);
	/**
	* @brief Writes obj in file.
	* @details Only writes if there is enough space before max_writable_size. Increases written by the memory size of the object.
	* @return true if writing was successful, false otherwise.
	* @see bool save(void*, size_t)
	*/
	template<typename T> inline bool save(T* obj) { return save(obj, sizeof(T)); };
	

	/**
	* @brief Writes in file the array, preceded by its element object's size and the number of element.
	* @details Only writes if there is enough space before max_writable_size. Increases written by byte_size_obj*array_size.
	* This function exists beside bool save(void* obj, size_t byte_size_obj), so that it is not necessary to know the size of the array at the time of writing when the file is then read.
	* @return true if writing was successful, false otherwise.
	*/
	bool save_array(void* array, size_t byte_size_obj, uint32_t array_size);
	/**
	* @brief Writes in file the array, preceded by its element object's size and the number of element, only if there is enough space before max_writable_size.
	* @details Only writes if there is enough space before max_writable_size. Increases written by (memory size of an element)*array_size.
	* This function exists beside bool save(void*, size_t), so that it is not necessary to know, when reading, the size of the array at the time of writing.
	* @return true if writing was successful, false otherwise.
	* @see bool save_array(void*, size_t, uint32_t)
	*/
	template<typename T> inline bool save_array(T* array, uint32_t array_size) { return save_array(array, sizeof(T), array_size); };
	
	/**
	* @brief Writes variable as a string in the format : name=variable\n
	* @details Only writes if there is enough space before max_writable_size. Increases written by the number of characters written (i.e. 0 is writing failed).
	* @return true if writing was successful, false otherwise.
	*/
	template<typename T> bool save_in_string(std::string name, T variable) {
		if (written + name.size()+3 <= max_writable_size) { // Enough space allowed left to write.
			auto startPos = file.tellp();
			file << name << '=' << variable << '\n';
			if (file.fail()) return false;
			written += file.tellp() - startPos;
			return true;
		}
		else return false;
	};

	/**
	* @brief Writes the array as a string in the format : name=variable[0], ..., variable[arr_size-1]\n
	* @details Only writes if there is enough space before max_writable_size. Increases written by the number of characters written (i.e. 0 is writing failed).
	* @return true if writing was successful, false otherwise.
	*/
	template<typename T> bool save_array_in_string(std::string name, T* array, uint32_t arr_size) {
		if (written + name.size()+3 + arr_size*3 <= max_writable_size) { // Enough space allowed left to write.
			auto startPos = file.tellp();
			file << name << '=';
			if (arr_size) file << array[0];
			for (uint32_t i=1; i<arr_size; i++) file << ", " << array[i];
			file << '\n';
			if (file.fail()) return false;
			written += file.tellp() - startPos;
			return true;
		}
		else return false;
	};

	/**
	* @brief Reads byte_size_obj bytes form the file and put it in obj.
	* @return true if reading was successful, false otherwise.
	*/
	bool load(void* obj, size_t byte_size_obj);
	/**
	* @brief Reads "size of T objects" bytes form the file and put it in obj.
	* @see void load(void*, size_t)
	*/
	template<typename T> inline void load(T* obj) { load(obj, sizeof(T)); };

	/**
	* @brief Tries to read an array from the file. Only procedes if the size of an element needed corresponds to the one written.
	* @details If there are more elements in the array than written, only the loaded_obj firsts elements are set.
	* The read position is set at the end of the array no matter the size of the written array and max_array_size.
	* If size byte_size_obj (the size of the objects of the array needed) doesn't correspond to the one written, the read pointer will not be moved by the function.
	* @param array Array in which to write read elements.
	* @param byte_size_obj Memory size of an array element.
	* @param max_array_size Size of the given array (i.e. maximum number of element that can be set in array).
	* @param loaded_obj Number of elements actually loaded into array.
	* @return true if reading was successful, false otherwise.
	* @see bool save_array(void*, size_t, uint32_t)
	*/
	bool load(void* array, size_t byte_size_obj, uint32_t max_array_size, uint32_t* loaded_obj);
	/**
	* @brief Tries to read an array from the file. Only procedes if the size of element needed corresponds to the one written.
	* @details If there are more elements in the array than written, only the loaded_obj firsts elements are set.
	* The read position is set at the end of the array no matter the size of the written array and max_array_size.
	* @param array Array in which to write read elements.
	* @param max_array_size Size of the given array (i.e. maximum number of element that can be set in array).
	* @param loaded_obj Number of elements actually loaded into array.
	* @return true if reading was successful, false otherwise.
	* @see void load(void*, size_t, uint32_t, uint32_t*)
	* @see bool save_array(void*, size_t, uint32_t)
	*/
	template<typename T> inline bool load(T* array, uint32_t max_array_size, uint32_t* loaded_obj) { return load(array, sizeof(T), max_array_size, loaded_obj); };

	/**
	* @brief Creates a map linking variable names to end and start position of its values in the file.
	* @details This function is designed for file written in text. This is a necessary step in reading a file (written in full text).
	* The file needs to comply to the following format :
	* -values are written name=value
	* -arrays are written name=value0, value1, ...
	* -lines starting with '#' or '\n' are ignored
	* -lines starting with '^' signify the start of a new section. Section 0 has no starting '^', it is implied.
	* The reading position of the file is changed.
	* @param start_section (included) Section from which to start mapping. Section 0 includes the first byte of the file.
	* @param end_section (excluded) Section from which to stop mapping. If end_section=-1 then map_file only maps start_section 
	* @return Created map. map will be empty if desired sections weren't found or were empty
	*/
	parse_map map_file(uint16_t start_section=0, uint16_t end_section=-1);

	/**
	* @brief Reads the at most arr_size values associated to name in the file and writes them into array.
	* @details This function only works for files written in full text.
	* Searches for name in map, if found tries to extract at most arr_size elements at positions map[name] from the file and put them into array.
	* If extraction of an element is unsuccessful, it is skipped and the function tries to read the next one.
	* In the file, elements are separated by ',' and end-of-line signifies end of list.
	* @return true if name has been found in map and at least one element could be extracted from the file. false otherwise.
	* @see parse_map map_file(uint16_t, uint16_t)
	*/
	template<typename T> bool load_from_map(parse_map& map, std::string name, T* array, uint32_t arr_size) {
		try {
			bool found_one = false;
			uint32_t arr_pos = 0;
			strpos_pair val = map.at(name);
			// std::cout << "loading from map " << name << "\n";
			file.clear();
			file.seekg(val[0]);
			while(file.tellg() < val[1]) {
				T temp;
				file.clear(); // clear state flags, so we don't import error states info from a previous use.
				file >> temp;
				// std::cout << "\textract " << temp;
				if (file.fail()) file.clear(); // Was unable to extract element of type T -> Clear flags
				else {
					array[arr_pos] = temp;
					found_one = true;
					// std::cout << " << found : " << array[arr_pos];
				}
				// std::cout << '\n';

				if (++arr_pos >= arr_size) break;
				file.ignore(val[1] - file.tellg(), ',');
			}
			return found_one;
		} catch(const std::out_of_range& ex) { // the variable 'name' wasn't found in mapped file
			std::cout << "'" << name << "' not found in map of name:values\n";
			return false;
		}
	};

	/**
	* @brief Reads the value associated to name in the file and writes it into obj.
	* @details This function only works for files written in full text.
	* Searches for name in map, if found tries to extract the value at positions map[name] from the file and put it into obj.
	* obj is only modified upon successful extraction.
	* Only calls @see template<typename T> bool load_from_map(parse_map&, std::string, T*, uint32_t).
	* @return true if name has been found in map and the element could be extracted from the file. false otherwise.
	* @see parse_map map_file(uint16_t, uint16_t)
	*/
	template<typename T> inline bool load_from_map(parse_map& map, std::string name, T& obj) { return load_from_map(map, name, &obj, 1); };

	/**
	* @brief Closes the file (if opened).
	*/
	void done();
};


// TEMPLATE SPECIALIZATIONS because signed and unsigned char don't work well. During insertion/extraction, file sees it as a variation of char while I want a number.

/**
* @brief uint8_t-Template specialization of @see template<typename T> bool save_in_string(std::string, T).
*/
template<> inline bool FileHandler::save_in_string(std::string name, uint8_t variable) {return save_in_string(name, (short)variable);}
/**
* @brief  int8_t-Template specialization of @see template<typename T> bool save_in_string(std::string, T).
*/
template<> inline bool FileHandler::save_in_string(std::string name,  int8_t variable) {return save_in_string(name, (short)variable);} 

/**
* @brief uint8_t-Template specialization of @see template<typename T> bool save_array_in_string(std::string, T*, uint32_t).
*/
template<> bool FileHandler::save_array_in_string(std::string name, uint8_t* array, uint32_t arr_size);
/**
* @brief  int8_t-Template specialization of @see template<typename T> bool save_array_in_string(std::string, T*, uint32_t).
*/
template<> bool FileHandler::save_array_in_string(std::string name,  int8_t* array, uint32_t arr_size);


/**
* @brief uint8_t-Template specialization of @see template<typename T> bool load_from_map(parse_map&, std::string, T*, uint32_t).
*/
template<> bool FileHandler::load_from_map(FileHandler::parse_map& map, std::string name, uint8_t* array, uint32_t arr_size);
/**
* @brief  int8_t-Template specialization of @see template<typename T> bool load_from_map(parse_map&, std::string, T*, uint32_t).
*/
template<> bool FileHandler::load_from_map(FileHandler::parse_map& map, std::string name,  int8_t* array, uint32_t arr_size);
/**
* @brief    char-Template specialization of @see template<typename T> bool load_from_map(parse_map&, std::string, T*, uint32_t).
*/
template<> bool FileHandler::load_from_map(FileHandler::parse_map& map, std::string name,    char* array, uint32_t arr_size);


/**
* @brief  int8_t-Template specialization of @see template<typename T> bool load_from_map(parse_map&, std::string, T&).
*/
template<> inline bool FileHandler::load_from_map(parse_map& map, std::string name, uint8_t& obj) {
	int16_t obj_temp = obj;
	bool ret = load_from_map(map, name, &obj_temp, 1);
	obj = obj_temp;
	return ret;
};
/**
* @brief uint8_t-Template specialization of @see template<typename T> bool load_from_map(parse_map&, std::string, T&).
*/
template<> inline bool FileHandler::load_from_map(parse_map& map, std::string name,  int8_t& obj) {
	int16_t obj_temp = obj;
	bool ret = load_from_map(map, name, &obj_temp, 1);
	obj = obj_temp;
	return ret;
};