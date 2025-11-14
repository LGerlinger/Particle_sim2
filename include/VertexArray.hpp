#pragma once

#include "Attribute.hpp"

#if OPENGL_INCLUDE_SUCCESS
#include <cstdint>
#include <vector>


/**
* Simplifies using an array of vertices, more precisely OpenGL's Vertex Array Objects and Vertex Buffer Objects.
* The vertices exist both on the CPU and GPU's memory. The VertexArray always handles the GPU's side but the CPU's vertices can either belong to the VertexArray or something else in the program.
* If the VertexArray handles the vertices on the CPU's side, it will allocate, reallocate and deallocate memory by itself.
* In the other case, when the vertices are outside, then a pointer to them can be passed when time comes to send data to the GPU. The VertexArray, will use this pointer as read-only.
*/
class VertexArray {
protected :
	GLuint VAO, VBO;
	void* vertices = nullptr;
	uint64_t n_vert = 0;
	uint32_t memSize1vert = 1;
	OPGL::DrawUse usage;
	OPGL::Primitive primitive;

public :
	VertexArray();
	/**
	* @brief Constructor. Calls set with the same arguments. Called when the vertices should be inside the VertexArray.
	* @see bool VertexArray::set(const std::vector<Attribute>&, OPGL::DrawUse, OPGL::Primitive, uint64_t)
	*/
	VertexArray(const std::vector<Attribute>& attribute_list, OPGL::DrawUse usage_, OPGL::Primitive primitive_, uint64_t number_of_vertices=0);
	/**
	* @brief Constructor. Calls set with the same arguments. Called when the vertices should be outside the VertexArray.
	* @see bool VertexArray::set(const std::vector<Attribute>&, OPGL::DrawUse, OPGL::Primitive, uint64_t, void*, size_t)
	*/
	VertexArray(const std::vector<Attribute>& attribute_list, OPGL::DrawUse usage_, OPGL::Primitive primitive_, uint64_t number_of_vertices, void* vertices_, size_t mem_size_vertices);
	/**
	* @brief Destructor. Frees all the dynamically allocated memory by this VertexArray on both CPU and GPU.
	* @details If vertices belong to this VertexArray vertices is freed.
	* The VAO and VBO are deleted.
	*/
	~VertexArray();

	/**
	* @brief Sets all the memory and attributes ready for drawing. Called when the vertices should be inside the VertexArray.
	* @param attribute_list List of Attributes describing a single vertex.
	* @param usage_ What the VertexArray will be used for. @see OPGL::DrawUse
	* @param primitive_ Primitive type of this VertexArray, like points or triangles. @see OPGL::Primitive
	* @param number_of_vertices Number of vertices to create in memory. If the argument is not explicited (i.e. 0), then no CPU memory is allocated and a buffer of size 0 is created on the GPU.
	* @return true if setup was successful. False otherwise. 
	* @see bool VertexArray::setupMemory(uint64_t, void*, size_t)
	* @see void VertexArray::setupAttributes(const std::vector<Attribute>&)
	*/
	bool set(const std::vector<Attribute>& attribute_list, OPGL::DrawUse usage_, OPGL::Primitive primitive_, uint64_t number_of_vertices=0);
	/**
	* @brief Sets all the memory and attributes ready for drawing. Called when the vertices should be outside the VertexArray.
	* @param attribute_list List of Attributes describing a single vertex.
	* @param usage_ What the VertexArray will be used for. @see OPGL::DrawUse
	* @param primitive_ Primitive type of this VertexArray, like points or triangles. @see OPGL::Primitive
	* @param number_of_vertices Number of vertices to create in memory. If the argument is not explicited (i.e. 0), then no CPU memory is allocated and a buffer of size 0 is created on the GPU.
	* @param vertices_ Pointer to the vertices data to send. If nullptr, nothing is sent (but everything else works the same).
	* @param mem_size_vertices Memory size of the data pointed by "vertices_". If "vertices_" is nullptr, mem_size_vertices is disregarded.
	* @return true if setup was successful. False otherwise. 
	* @see bool VertexArray::setupMemory(uint64_t, void*, size_t)
	* @see void VertexArray::setupAttributes(const std::vector<Attribute>&)
	*/
	bool set(const std::vector<Attribute>& attribute_list, OPGL::DrawUse usage_, OPGL::Primitive primitive_, uint64_t number_of_vertices, void* vertices_, size_t mem_size_vertices);
	
	/**
	* @brief Sets the number of vertices to "number_of_vertices". Called when the vertices are inside the VertexArray.
	* @details Changes the memory size of the array of vertices on both GPU and CPU.
	* @param number_of_vertices Number of vertices to create. If the VertexArray was already this size, nothing changes and the function returns true immediatly.
	* @return true if allocation was successful. False otherwise. 
	*/
	bool setupMemory(uint64_t number_of_vertices);
	/**
	* @brief Sets the number of vertices to "number_of_vertices". Called when the vertices are outside the VertexArray.
	* @details Changes the memory size of the array of vertices on the GPU.
	* @param number_of_vertices Number of vertices to create/send. If the VertexArray was already this size, nothing changes and the function returns true immediatly.
	* @param vertices_ Pointer to the vertices data to send. If nullptr, nothing is sent (but everything else works the same).
	* @param mem_size_vertices Memory size of the data pointed by "vertices_". If "vertices_" is nullptr, mem_size_vertices is disregarded.
	* @return true if allocation was successful. False otherwise. 
	*/
	bool setupMemory(uint64_t number_of_vertices, void* vertices_, size_t mem_size_vertices);

	/**
	* @brief Deletes and recreates everything on the GPU's side. Useful when reloadong the window.
	*/
	void reset(const std::vector<Attribute>& attribute_list);
	
	/**
	* @brief Frees all the memory on the side of the GPU.
	* @details unbinds and deletes VAO and VBO.
	*/
	void freeGPUside();
	/**
	* @brief Frees all the dynamically allocated memory by this VertexArray on both CPU and GPU.
	* @details If vertices belong to this VertexArray vertices is freed.
	* The VBO size is set to 0 but not deleted. So the VAO and VBO will stay ready for when memory is reallocated on the GPU.
	*/
	void freeMemory();

private :
	/**
	* @brief Internal function meant to only be called by set().
	* @details Generates VAO and VBO, declares attributes.
	*/
	void setHelper(const std::vector<Attribute>& attribute_list, OPGL::DrawUse usage_, OPGL::Primitive primitive_);

	/**
	* @brief Sets and enables the attributes in "attribute_list".
	* @warning Ensure VAO and VBO are binded correctly before calling.
	* @warning As of now only DOUBLE, FLOAT, INT, UNSIGNED_INT, BYTES_4, PADDING attributes types are handled. I'll add more if I ever need it. Also I am unsure if I can handle attributes of size not a multiple of 4 bytes... too bad.
	*/
	void setupAttributes(const std::vector<Attribute>& attribute_list);


public:
	/**
	* @brief Sends the vertices to the GPU.
	* @details Useful when vertices don't change often, so you can update them once then draw many times.
	* @param start_index Index of the first vertex (included) to send.
	* @param end_index Index of the last vertex (excluded) to send.
	* @param data Data to send if the vertices are outside of the VertexArray.
	*/
	void updateVertices(uint64_t start_index=0, uint64_t end_index=-1, void* data = nullptr);
	/**
	* @brief Draws the vertices to the window.
	* @details Useful when vertices don't change often, so you can update them once then draw many times.
	* @param shader Shader that wil be used to draw the vertices.
	* @param start_index Index of the first vertex (included) to draw.
	* @param end_index Index of the last vertex (excluded) to draw.
	*/
	void draw(GLuint shader, uint64_t start_index=0, uint64_t end_index=-1);
	/**
	* @brief Sends then draws the vertices to the window.
	* @param shader Shader that wil be used to draw the vertices.
	* @param start_index Index of the first vertex (included) to draw.
	* @param end_index Index of the last vertex (excluded) to draw.
	* @param data Vertices data if they are outside of the VertexArray.
	*/
	void updateAndDraw(GLuint shader, uint64_t start_index=0, uint64_t end_index=-1, void* data = nullptr);

	/**
	* @brief Returns this VertexArray's vertices. If the vertices are outside of it, it will return nullptr.
	* @details Used to read or write to the array of vertices, NOT to free memory.
	*/
	void* getVertices() {return vertices;};

	/**
	* @brief Returns the reference to the vertex of index vIndex.
	* @details The function assumes each vertex is the size of T. The behavior is undefined if this is not the case.
	* @warning If the vertices are outside of the VertexArray, this will cause a segmentation fault.
	*/
	template<typename T>
	T& get(size_t vIndex) {return ((T*)vertices)[vIndex];};
	/**
	* @brief Returns a pointer to the vertex of index vIndex.
	* @warning If the vertices are outside of the VertexArray, this will cause a segmentation fault.
	*/
	void* operator[](size_t vIndex) {return (char*)vertices + vIndex * memSize1vert;};

	/**
	* @return Returns the number of vertices, even if the vertices are outside of the VertexArray.
	*/
	uint64_t nVert() {return n_vert;};
	/**
	* @return Returns the size of each vertex, even if the vertices are outside of the VertexArray.
	*/
	uint32_t size1Vert() {return memSize1vert;};

	/**
	* @brief Prints the values of its members to the standard output.
	*/
	void printSelf();
};

#endif // OPENGL_INCLUDE_SUCCESS