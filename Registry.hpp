#pragma once

#include "Load.hpp"
#include "Load.hpp"
#include "GL.hpp"

#include <glm/glm.hpp>
#include "Mesh.hpp"
#include "Scene.hpp"

#include "Sound.hpp"


struct TextureLoadingInfo {
    GLuint texture = 0;
    uint32_t width,height = 0;
    TextureLoadingInfo(GLuint t, uint32_t w, uint32_t h) : texture(t), width(w), height(h) {};
    GLenum target = GL_TEXTURE_2D;
};

//---- Meshes and Scene----
extern GLuint sumo_character_meshes_for_lit_color_texture_program;
extern GLuint sumo_scene_meshes_for_lit_color_texture_program;//static meshes with light map
extern Load< Scene > sumo_scene;
extern Load< MeshBuffer > sumo_character_meshes;
extern Load< MeshBuffer > sumo_scene_meshes;