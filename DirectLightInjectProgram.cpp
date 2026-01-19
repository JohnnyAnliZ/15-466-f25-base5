#include "DirectLightInjectProgram.hpp"

#include "gl_compile_program.hpp"
#include "gl_errors.hpp"
#include "Scene.hpp"

// Shader for injecting direct lighting
const char* injectVertShader = R"(
#version 330

uniform mat4x3 LIGHT_FROM_OBJECT; 
uniform mat3 LIGHT_FROM_NORMAL;

in vec4 Position;
in vec3 Normal;
in vec4 Color;
in vec2 TexCoord;  

out vec3 position;
out vec3 normal;
out vec4 color;
out vec2 texCoord;


void main() {
    // Render to lightmap
	position = LIGHT_FROM_OBJECT * Position;
    float distanceFromLight = length((position).xyz);
	gl_Position = vec4(TexCoord * 2.0 - 1.0, 0.0, 1.0);
    normal = LIGHT_FROM_NORMAL * Normal;
	color = Color;
	texCoord = TexCoord;
}
)";


const char* injectFragShader = R"(
#version 330
in vec3 position;
in vec3 normal;
in vec4 color;
in vec2 texCoord;

out vec4 lightOutput;

uniform int LIGHT_TYPE;
uniform vec3 LIGHT_LOCATION;
uniform vec3 LIGHT_DIRECTION;
uniform vec3 LIGHT_ENERGY;
uniform float LIGHT_CUTOFF;
uniform samplerCube Cubemap;

void main() {   
    vec3 L = position - LIGHT_LOCATION;
    float distance = length(L);
    L = normalize(L);
	//sample the what uv is the cubemap hitting at the direction
	vec2 visibleUV = texture(Cubemap, L).rg;
	if(length(visibleUV - texCoord) > 0.1){
		discard;
	}

	vec3 N = normalize(normal);
	float NdotL = dot(N, -L);
    if (NdotL <= 0.0) {
        discard;  // discard backface
    }
	//eqauation to balance light contribution given by 
	//https://rasmusbarr.github.io/blog/
	//get total number of pixels
	float n = 6 * pow(textureSize(Cubemap, 0).width,2);
	float denominator = pow(pow(L.x,2) + pow(L.y,2) + 1,1.5);
	lightOutput = vec4(LIGHT_ENERGY * 24 /(n * denominator),1.0);
}
)";

DirectLightInjectProgram::DirectLightInjectProgram() {
	program = gl_compile_program(injectVertShader, injectFragShader);

	//look up the locations of vertex attributes:
	Position_vec4 = glGetAttribLocation(program, "Position");
	Normal_vec3 = glGetAttribLocation(program, "Normal");
	Color_vec4 = glGetAttribLocation(program, "Color");
	TexCoord_vec2 = glGetAttribLocation(program, "TexCoord");

	//look up the locations of uniforms:
	WORLD_FROM_OBJECT_mat4x3 = glGetUniformLocation(program, "WORLD_FROM_OBJECT");
	CLIP_FROM_OBJECT_mat4 = glGetUniformLocation(program, "CLIP_FROM_OBJECT");
	LIGHT_FROM_OBJECT_mat4x3 = glGetUniformLocation(program, "LIGHT_FROM_OBJECT");
	LIGHT_FROM_NORMAL_mat3 = glGetUniformLocation(program, "LIGHT_FROM_NORMAL");

	LIGHT_TYPE_int = glGetUniformLocation(program, "LIGHT_TYPE");
	
	LIGHT_LOCATION_vec3 = glGetUniformLocation(program, "LIGHT_LOCATION");
	LIGHT_DIRECTION_vec3 = glGetUniformLocation(program, "LIGHT_DIRECTION");
	LIGHT_ENERGY_vec3 = glGetUniformLocation(program, "LIGHT_ENERGY");
	LIGHT_CUTOFF_float = glGetUniformLocation(program, "LIGHT_CUTOFF");


	GLuint TEX_sampler2D = glGetUniformLocation(program, "TEX");

	//set TEX to always refer to texture binding zero:
	glUseProgram(program); //bind program -- glUniform* calls refer to this program now

	glUniform1i(TEX_sampler2D, 0); //set TEX to sample from GL_TEXTURE0

	glUseProgram(0); 
}

Load< DirectLightInjectProgram > direct_light_inject_program(LoadTagEarly, []() -> DirectLightInjectProgram const * {
	DirectLightInjectProgram *ret = new DirectLightInjectProgram();

	//----- build the pipeline template -----
	direct_light_inject_pipeline.program = ret->program;

	direct_light_inject_pipeline.WORLD_FROM_OBJECT_mat4 = ret->WORLD_FROM_OBJECT_mat4;
	direct_light_inject_pipeline.WORLD_FROM_OBJECT_mat4x3 = ret->WORLD_FROM_OBJECT_mat4x3;
	direct_light_inject_pipeline.CLIP_FROM_OBJECT_mat4 = ret->CLIP_FROM_OBJECT_mat4;
	direct_light_inject_pipeline.LIGHT_FROM_OBJECT_mat4x3 = ret->LIGHT_FROM_OBJECT_mat4x3;
	direct_light_inject_pipeline.LIGHT_FROM_NORMAL_mat3 = ret->LIGHT_FROM_NORMAL_mat3;

	/* This will be used later if/when we build a light loop into the Scene:*/
	direct_light_inject_pipeline.LIGHT_TYPE_int = ret->LIGHT_TYPE_int;
	direct_light_inject_pipeline.LIGHT_LOCATION_vec3 = ret->LIGHT_LOCATION_vec3;
	direct_light_inject_pipeline.LIGHT_DIRECTION_vec3 = ret->LIGHT_DIRECTION_vec3;
	direct_light_inject_pipeline.LIGHT_ENERGY_vec3 = ret->LIGHT_ENERGY_vec3;
	direct_light_inject_pipeline.LIGHT_CUTOFF_float = ret->LIGHT_CUTOFF_float;
	

	//make a 1-pixel white texture to bind by default:
	GLuint tex;
	glGenTextures(1, &tex);

	glBindTexture(GL_TEXTURE_2D, tex);
	std::vector< glm::u8vec4 > tex_data(1, glm::u8vec4(0xff));
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, tex_data.data());
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glBindTexture(GL_TEXTURE_2D, 0);


	direct_light_inject_pipeline.textures[0].texture = tex;
	direct_light_inject_pipeline.textures[0].target = GL_TEXTURE_2D;

	return ret;
});

DirectLightInjectProgram::~DirectLightInjectProgram() {
	glDeleteProgram(program);
	program = 0;
}