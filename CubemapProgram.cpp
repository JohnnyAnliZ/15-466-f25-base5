#include "CubemapProgram.hpp"
#include "gl_compile_program.hpp"
#include "gl_errors.hpp"

Load< CubemapProgram > cubemap_program(LoadTagEarly, []() -> CubemapProgram const * {
	CubemapProgram *ret = new CubemapProgram();
	cubemap_pipeline.program = ret->program;
	cubemap_pipeline.CLIP_FROM_OBJECT_mat4 = ret->CLIP_FROM_OBJECT_mat4;
	return ret;
});

CubemapProgram::CubemapProgram() {
	//shader to rasterize texcoords onto one face of the cubemap
	program = gl_compile_program(
		//vertex shader:
		"#version 330\n"
        "uniform mat4x4 CLIP_FROM_OBJECT; "
		"in vec4 Position;\n"
		"in vec3 Normal;\n"
		"in vec4 Color;\n"
		"in vec2 TexCoord;\n"
		"out vec3 position;\n"
		"out vec3 normal;\n"
		"out vec4 color;\n"
		"out vec2 texCoord;\n"
		"void main() {\n"
		"	gl_Position = CLIP_FROM_OBJECT * Position;\n"
		"	position = Position.xyz;\n"
		"	normal = Normal;\n"
		"	color = Color;\n"
		"	texCoord = TexCoord;\n"
		"}\n"
	,
		//fragment shader:
		"#version 330\n"
		"in vec3 position;\n"
		"in vec3 normal;\n"
		"in vec4 color;\n"
		"in vec2 texCoord;\n"
		"out vec2 fragColor;\n"
		"void main() {\n"
		"if (texCoord.x < 0.0 || texCoord.x > 1.0 ||\n"
    	"texCoord.y < 0.0 || texCoord.y > 1.0) discard;\n"
		"if (!gl_FrontFacing)discard;\n"
		"fragColor = texCoord;\n"
		"}\n"
	);

	//look up the locations of vertex attributes:
	Position_vec4 = glGetAttribLocation(program, "Position");
	Normal_vec3 = glGetAttribLocation(program, "Normal");
	Color_vec4 = glGetAttribLocation(program, "Color");
	TexCoord_vec2 = glGetAttribLocation(program, "TexCoord");

	CLIP_FROM_OBJECT_mat4 = glGetUniformLocation(program, "CLIP_FROM_OBJECT");

	glUseProgram(program); //bind program -- glUniform* calls refer to this program now
	glUseProgram(0); //unbind program -- glUniform* calls refer to ??? now
}

CubemapProgram::~CubemapProgram() {
	glDeleteProgram(program);
	program = 0;
}
