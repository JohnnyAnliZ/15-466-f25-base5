#pragma once

#include "Load.hpp"
#include "GL.hpp"

#include <glm/glm.hpp>
#include "Mesh.hpp"
#include "Scene.hpp"
#include "Sound.hpp"

//---- Meshes and Scene----
extern GLuint sumo_character_meshes_for_lit_color_texture_program;
extern GLuint sumo_scene_meshes_vao;//static meshes with light map
extern Load< Scene > sumo_scene;
extern Load< MeshBuffer > sumo_character_meshes;
extern Load< MeshBuffer > sumo_scene_meshes;

//---- Lightmap----
extern Load<Lightmap> lightmap;
//---- Shadowmap----
extern Load<Cubemap> cubemap;