#pragma once
// # define USE_OPENGL 1
#if defined(__has_include) && USE_OPENGL
	#if __has_include(<GL/glew.h>)
		#include <GL/glew.h>
		#define OPENGL_INCLUDE_SUCCESS 1

#include <string>
#include <vector>

/**
* Simple namespace to turn some GLenum into c++ enums and to declare useful functions using OpenGL.
*/
namespace OPGL {
	enum class Type : GLenum {
		PADDING=         0, // Padding is for when a part of the memory is sent but isn't an attribute.
		BYTE=            GL_BYTE,
		UNSIGNED_BYTE=   GL_UNSIGNED_BYTE,
		BYTES_2=         GL_2_BYTES,
		BYTES_3=         GL_3_BYTES,
		BYTES_4=         GL_4_BYTES,
		SHORT=           GL_SHORT,
		UNSIGNED_SHORT=  GL_UNSIGNED_SHORT,
		INT=             GL_INT,
		UNSIGNED_INT=    GL_UNSIGNED_INT,
		FLOAT=           GL_FLOAT,
		HALF_FLOAT=      GL_HALF_FLOAT,
		DOUBLE=          GL_DOUBLE,
	};

	enum class Primitive : GLenum {
		POINTS=                   GL_POINTS,
		LINES=                    GL_LINES,
		LINE_LOOP=                GL_LINE_LOOP,
		LINE_STRIP=               GL_LINE_STRIP,
		TRIANGLES=                GL_TRIANGLES,
		TRIANGLE_STRIP=           GL_TRIANGLE_STRIP,
		TRIANGLE_FAN=             GL_TRIANGLE_FAN,
		LINES_ADJACENCY=          GL_LINES_ADJACENCY,
		LINE_STRIP_ADJACENCY=     GL_LINE_STRIP_ADJACENCY,
		TRIANGLES_ADJACENCY=      GL_TRIANGLES_ADJACENCY,
		TRIANGLE_STRIP_ADJACENCY= GL_TRIANGLE_STRIP_ADJACENCY,
	};

	enum class DrawUse : GLenum {
		STREAM_DRAW=  GL_STREAM_DRAW,
		STREAM_READ=  GL_STREAM_READ,
		STREAM_COPY=  GL_STREAM_COPY,
		STATIC_DRAW=  GL_STATIC_DRAW,
		STATIC_READ=  GL_STATIC_READ,
		STATIC_COPY=  GL_STATIC_COPY,
		DYNAMIC_DRAW= GL_DYNAMIC_DRAW,
		DYNAMIC_READ= GL_DYNAMIC_READ,
		DYNAMIC_COPY= GL_DYNAMIC_COPY,
	};

	/**
	* @brief Initializes included OpenGL functions if necessary.
	* @details As of now I only use Glew. Glew requires to initialize its functions before calling them.
	* @return True if OpenGL functions were successfuly initialized and vertex_array are available because I really need them.
	*/
	bool initializeOpenGLfunctions();

	/**
	* @brief Sets the uniform "name" of the "shader" to "value".
	* @details I normally use SFML's "setUniform" but it doesn't work with unsigned integers.
	*/
	void sendUnsignedUniform(const GLuint shader, const std::string &name, unsigned int value);
};

/**
* Describes a Vertex's attributes like meta-data.
* More precisely, gives the type and number of elements of that type.
* @example If a vertex contains 2 floats for its position and 2 floats for its texture coordinates it could be described like :
* @code
* std::vector<Attribute> particle_attr = {
* Attribute(OPGL::Type::FLOAT, 2),
* Attribute(OPGL::Type::FLOAT, 2),
* };
* @endcode
*/
class Attribute {
public :
	/**
	* @brief Calculates the memory size of an Attribute.
	*/
	static size_t calculate_size(Attribute attribute);
	/**
	* @brief Calculates the memory size of a list of Attributes.
	*/
	static size_t calculate_size(const std::vector<Attribute>& list);

	OPGL::Type type;
	uint8_t n_elem;

	Attribute() = default;
	Attribute(OPGL::Type type_, uint8_t number_of_elements) {set(type_, number_of_elements);};
	inline void set(OPGL::Type type_, uint8_t number_of_elements) {
		type = type_;
		n_elem = number_of_elements;
	};
};


	#else // __has_include(<GL/glew.h>)
		#define OPENGL_INCLUDE_SUCCESS 0
	#endif // __has_include(<GL/glew.h>)
#else // __has_include
	#define OPENGL_INCLUDE_SUCCESS 0
#endif // __has_include