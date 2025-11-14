#include "Attribute.hpp"

#if OPENGL_INCLUDE_SUCCESS

#include <iostream>

bool OPGL::initializeOpenGLfunctions() {
	glewExperimental = GL_TRUE; // Needed for core profile
	if (glewInit() != GLEW_OK) {
		std::cerr << "Failed to initialize GLEW" << std::endl;
		return false;
	}
	if (!GLEW_ARB_vertex_array_object) { // Checking if VAOs are available
		std::cerr << "VAOs not supported!" << std::endl;
		return false;
	}
	return true;
}

void OPGL::sendUnsignedUniform(const GLuint shader, const std::string &name, unsigned int value) {
	GLuint location = glGetUniformLocation(shader, name.c_str());
	glUseProgram(shader);
	glUniform1ui(location, value);
}



size_t Attribute::calculate_size(Attribute attribute) {
	switch (attribute.type) {
		case OPGL::Type::PADDING:
			return(attribute.n_elem);
			break;
		case OPGL::Type::BYTE:
			return(attribute.n_elem * sizeof(GLbyte));
			break;
		case OPGL::Type::UNSIGNED_BYTE:
			return(attribute.n_elem * sizeof(GLubyte));
			break;
		case OPGL::Type::BYTES_2:
			return(attribute.n_elem * sizeof(GLbyte) *2);
			break;
		case OPGL::Type::BYTES_3:
			return(attribute.n_elem * sizeof(GLbyte) *3);
			break;
		case OPGL::Type::BYTES_4:
			return(attribute.n_elem * sizeof(GLbyte) *4);
			break;
		case OPGL::Type::SHORT:
			return(attribute.n_elem * sizeof(GLshort));
			break;
		case OPGL::Type::UNSIGNED_SHORT:
			return(attribute.n_elem * sizeof(GLushort));
			break;
		case OPGL::Type::INT:
			return(attribute.n_elem * sizeof(GLint));
			break;
		case OPGL::Type::UNSIGNED_INT:
			return(attribute.n_elem * sizeof(GLuint));
			break;
		case OPGL::Type::FLOAT:
			return(attribute.n_elem * sizeof(GLfloat));
			break;
		case OPGL::Type::HALF_FLOAT:
			return(attribute.n_elem * sizeof(GLhalf));
			break;
		case OPGL::Type::DOUBLE:
			return(attribute.n_elem * sizeof(GLdouble));
			break;
		}
	return 0;
}

size_t Attribute::calculate_size(const std::vector<Attribute>& list) {
	size_t list_mem_size = 0;
	for (const auto& attr : list) {
		list_mem_size += calculate_size(attr);
	}
	return list_mem_size;
}

#endif // OPENGL_INCLUDE_SUCCESS