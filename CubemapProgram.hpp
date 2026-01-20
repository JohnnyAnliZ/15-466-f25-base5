#pragma once

#include "GL.hpp"
#include "Load.hpp"
#include "Scene.hpp"

struct CubemapProgram {
	CubemapProgram();
	~CubemapProgram();

	GLuint program = 0;

	//Attribute (per-vertex variable) locations:
	GLuint Position_vec4 = -1U;
	GLuint Normal_vec3 = -1U;
	GLuint Color_vec4 = -1U;
	GLuint TexCoord_vec2 = -1U;

	GLuint CLIP_FROM_OBJECT_mat4 = -1U;
};

extern Load< CubemapProgram > cubemap_program;

//For convenient scene-graph setup, copy this object:
// NOTE: by default, has texture bound to 1-pixel white texture -- so it's okay to use with vertex-color-only meshes.
inline Scene::Drawable::Pipeline cubemap_pipeline;