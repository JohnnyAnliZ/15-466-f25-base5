#pragma once


#include "GL.hpp"
#include "Load.hpp"
#include "Scene.hpp"



struct DirectLightInjectProgram {
	DirectLightInjectProgram();
	~DirectLightInjectProgram();

	GLuint program = 0;

	//Attribute (per-vertex variable) locations:
	GLuint Position_vec4 = -1U;
	GLuint Normal_vec3 = -1U;
	GLuint Color_vec4 = -1U;
	GLuint TexCoord_vec2 = -1U;

	//Uniform (per-invocation variable) locations:
	GLuint WORLD_FROM_OBJECT_mat4 = -1U;
	GLuint WORLD_FROM_OBJECT_mat4x3 = -1U;
	GLuint CLIP_FROM_OBJECT_mat4 = -1U;
	GLuint LIGHT_FROM_OBJECT_mat4x3 = -1U;
	GLuint LIGHT_FROM_NORMAL_mat3 = -1U;

	//lighting:
	GLuint LIGHT_TYPE_int = -1U;
	GLuint LIGHT_LOCATION_vec3 = -1U;
	GLuint LIGHT_DIRECTION_vec3 = -1U;
	GLuint LIGHT_ENERGY_vec3 = -1U;
	GLuint LIGHT_CUTOFF_float = -1U;
	
	//Textures:
	//TEXTURE0 - texture that is accessed by TexCoord
};

extern Load< DirectLightInjectProgram > direct_light_inject_program;

//For convenient scene-graph setup, copy this object:
// NOTE: by default, has texture bound to 1-pixel white texture -- so it's okay to use with vertex-color-only meshes.
inline Scene::Drawable::Pipeline direct_light_inject_pipeline;