#include "VertexArray.hpp"

#if OPENGL_INCLUDE_SUCCESS

#include <cstdlib>
#include <iostream>

VertexArray::VertexArray() {
	VAO = 0;
	VBO = 0;
}

VertexArray::VertexArray(const std::vector<Attribute>& attribute_list, OPGL::DrawUse usage_, OPGL::Primitive primitive_, uint64_t number_of_vertices) {
	set(attribute_list, usage_, primitive_, number_of_vertices);
}
VertexArray::VertexArray(const std::vector<Attribute>& attribute_list, OPGL::DrawUse usage_, OPGL::Primitive primitive_, uint64_t number_of_vertices, void* vertices_, size_t mem_size_vertices) {
	set(attribute_list, usage_, primitive_, number_of_vertices, vertices_, mem_size_vertices);
}


bool VertexArray::set(const std::vector<Attribute>& attribute_list, OPGL::DrawUse usage_, OPGL::Primitive primitive_, uint64_t number_of_vertices) {
	// std::cout << "VertexArray::set(usage_=" << (GLenum)usage_ << ", primitive_=" << (GLenum)primitive_ << ", number_of_vertices=" << number_of_vertices << ", vertices_=" << vertices_ << ", mem_size_vertices=" << mem_size_vertices << ")\n";

	setHelper(attribute_list, usage_, primitive_);

	if (number_of_vertices) { // !=0 means that memory should be allocated
		if (!setupMemory(number_of_vertices)) return false;
	} else {
		glBufferData(GL_ARRAY_BUFFER, 0, NULL, (GLenum)usage);
	}
	return true;
}

bool VertexArray::set(const std::vector<Attribute>& attribute_list, OPGL::DrawUse usage_, OPGL::Primitive primitive_, uint64_t number_of_vertices, void* vertices_, size_t mem_size_vertices) {
	// std::cout << "VertexArray::set(usage_=" << (GLenum)usage_ << ", primitive_=" << (GLenum)primitive_ << ", number_of_vertices=" << number_of_vertices << ", vertices_=" << vertices_ << ", mem_size_vertices=" << mem_size_vertices << ")\n";

	setHelper(attribute_list, usage_, primitive_);

	if (number_of_vertices) { // !=0 means that memory should be allocated
		if (!setupMemory(number_of_vertices, vertices_, mem_size_vertices)) return false;
	} else {
		glBufferData(GL_ARRAY_BUFFER, 0, NULL, (GLenum)usage);
	}
	return true;
}


void VertexArray::setHelper(const std::vector<Attribute>& attribute_list, OPGL::DrawUse usage_, OPGL::Primitive primitive_) {
	usage = usage_;
	primitive = primitive_;
	memSize1vert = Attribute::calculate_size(attribute_list);
	
	glGenVertexArrays(1, &VAO);
	glGenBuffers(1, &VBO);
	glBindVertexArray(VAO);
	glBindBuffer(GL_ARRAY_BUFFER, VBO);
	
	// Setting attributes
	setupAttributes(attribute_list);
}

VertexArray::~VertexArray() {
	// std::cout << "VertexArray::~VertexArray()\n";
	if (vertices) free(vertices);
	freeGPUside();
}


bool VertexArray::setupMemory(uint64_t number_of_vertices) {
	if (n_vert == number_of_vertices) return true; // The VertexArray already has that many vertices.
	n_vert = number_of_vertices;

	float mem_size_vertices = n_vert * memSize1vert;
	if (vertices) {
		void* newVertices = realloc(vertices, mem_size_vertices); // Vertices are stored inside the VertexArray so we allocate memory for that.
		if (newVertices) vertices = newVertices; // Verifying reallocation went well or memory would be leaked.
		else return false;
	}
	else {
		vertices = malloc(mem_size_vertices);
		if (!vertices) return false; // failed allocating memory for the vertices
	}

	glBindBuffer(GL_ARRAY_BUFFER, VBO);
	glBufferData(GL_ARRAY_BUFFER, mem_size_vertices, NULL, (GLenum)usage);
	return true; // success
}

bool VertexArray::setupMemory(uint64_t number_of_vertices, void* vertices_, size_t mem_size_vertices) {
	if (n_vert == number_of_vertices) return true; // The VertexArray already has that many vertices.
	n_vert = number_of_vertices;

	if (vertices_ && // checking that the promessed memory size of vertices_ corresponds to what was calculated here.
		n_vert * memSize1vert != mem_size_vertices ) {
			return false; // failure
	}

	glBindBuffer(GL_ARRAY_BUFFER, VBO);
	glBufferData(GL_ARRAY_BUFFER, mem_size_vertices, vertices_, (GLenum)usage);
	return true; // success
}


void VertexArray::setupAttributes(const std::vector<Attribute>& attribute_list) {
	unsigned int current_attr = 0;
	size_t current_offset = 0;
	bool found_type, padding;
	// int index = 0;
	for (const auto& attr : attribute_list) {
		// std::cout << "d attribut " << index++ << "\n\t" << (short)attr.n_elem << ' ';
		found_type = true;
		padding = false;
		switch (attr.type) {
			case OPGL::Type::DOUBLE :
			glVertexAttribLPointer(current_attr, attr.n_elem, (GLenum)attr.type, (GLsizei)memSize1vert, (void*)current_offset);
			// std::cout << "double\n";
			break;

			case OPGL::Type::FLOAT :
			glVertexAttribPointer(current_attr, attr.n_elem, (GLenum)attr.type, GL_FALSE, (GLsizei)memSize1vert, (void*)current_offset);
			// std::cout << "float\n";
			break;

			case OPGL::Type::INT :
			glVertexAttribIPointer(current_attr, attr.n_elem, (GLenum)attr.type, memSize1vert, (void*)current_offset);
			// std::cout << "int\n";
			break;
			case OPGL::Type::UNSIGNED_INT :
			glVertexAttribIPointer(current_attr, attr.n_elem, (GLenum)attr.type, memSize1vert, (void*)current_offset);
			// std::cout << "unsigned int\n";
			break;
			case OPGL::Type::BYTES_4 :
			glVertexAttribIPointer(current_attr, attr.n_elem, (GLenum)attr.type, memSize1vert, (void*)current_offset);
			// std::cout << "bytes 4\n";
			break;

			case OPGL::Type::PADDING :
			// std::cout << "padding\n";
			padding = true;
			break;

			default : // I don't actually handle most cases. I'll do it if I need it.
			found_type = false;
			break;
		}
		if (found_type) {
			current_offset += Attribute::calculate_size(attr);
			if (!padding) {
				glEnableVertexAttribArray(current_attr++);
			}
		}
	}
}


void VertexArray::reset(const std::vector<Attribute>& attribute_list) {
	// Deleting and recreating buffers
	freeGPUside();
	glGenVertexArrays(1, &VAO);
	glGenBuffers(1, &VBO);
	glBindVertexArray(VAO);
	glBindBuffer(GL_ARRAY_BUFFER, VBO);
	setupAttributes(attribute_list);
	glBufferData(GL_ARRAY_BUFFER, n_vert*memSize1vert, NULL, (GLenum)usage);
}


void VertexArray::freeGPUside() {
	// Unbinding ressources is advised in the case of multiple context. I still add it because it could be useful and it costs nothing.
	glBindVertexArray(0);
	glBindBuffer(GL_ARRAY_BUFFER, 0);
	glDeleteVertexArrays(1, &VAO);
	glDeleteBuffers(1, &VBO);
}

void VertexArray::freeMemory() {
	// Freeing GPU memory without destroying the buffers
	glBindBuffer(GL_ARRAY_BUFFER, VBO);
	glBufferData(GL_ARRAY_BUFFER, 0, NULL, (GLenum)usage);
	// Freeing CPU memory
	if (vertices) free(vertices);
	vertices = nullptr;
	n_vert = 0;
}


void VertexArray::updateVertices(uint64_t start_index, uint64_t end_index, void* data) {
	glBindVertexArray(VAO);
	glBindBuffer(GL_ARRAY_BUFFER, VBO);
	glBufferSubData(GL_ARRAY_BUFFER, start_index * memSize1vert, (std::min(end_index, n_vert) - start_index) * memSize1vert, data ? data : vertices);
}

/**
* @warning Do not send n_vert < start_index
*/
void VertexArray::draw(GLuint shader, uint64_t start_index, uint64_t end_index) {
	glUseProgram(shader);
	glBindVertexArray(VAO);
	glBindBuffer(GL_ARRAY_BUFFER, VBO);
	glDrawArrays((GLenum)primitive, start_index, std::min(end_index, n_vert) - start_index);
}

/**
* @warning Do not send n_vert < start_index
*/
void VertexArray::updateAndDraw(GLuint shader, uint64_t start_index, uint64_t end_index, void* data) {
	glUseProgram(shader);
	glBindVertexArray(VAO);
	glBindBuffer(GL_ARRAY_BUFFER, VBO);
	glBufferSubData(GL_ARRAY_BUFFER, start_index * memSize1vert, (std::min(end_index, n_vert) - start_index) * memSize1vert, data ? data : vertices);
	glDrawArrays((GLenum)primitive, start_index, std::min(end_index, n_vert) - start_index);
	// glBindVertexArray(0); // Unbind VAO after drawing
}


void VertexArray::printSelf() {
	std::cout << "VertexArray::printSelf()\n";
	std::cout << "\tVAO=" << VAO << ", VBO=" << VBO << "\n";
	std::cout << "\tvertices=" << vertices << ",   n_vert=" << n_vert << ",  memSize1vert=" << memSize1vert << "\n";
	std::cout << "\tprimitive=" << (GLenum)primitive << ",  usage=" << (GLenum)usage << "\n\n";
}

#endif // OPENGL_INCLUDE_SUCCESS