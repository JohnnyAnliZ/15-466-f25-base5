#include "Registry.hpp"
#include "data_path.hpp"
#include "load_save_png.hpp"
#include "LitColorTextureProgram.hpp"
#include "ColorTextureProgram.hpp"
#include "GL.hpp"
#include <glm/glm.hpp>
#include <iostream>

static TextureLoadingInfo *load_texture_from_png(const std::string &path, bool use_mipmap, bool repeat)
{
    glm::uvec2 size;
    std::vector<glm::u8vec4> data;
    load_png(path, &size, &data, UpperLeftOrigin);

    GLuint tex = 0;
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);

    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8,
                 size.x, size.y, 0,
                 GL_RGBA, GL_UNSIGNED_BYTE, data.data());

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, use_mipmap ? GL_LINEAR_MIPMAP_LINEAR : GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, use_mipmap ? GL_LINEAR : GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, repeat ? GL_REPEAT : GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, repeat ? GL_REPEAT : GL_CLAMP_TO_EDGE);
    if (use_mipmap)
    {
        glGenerateMipmap(GL_TEXTURE_2D);
    }

    glBindTexture(GL_TEXTURE_2D, 0);
    return new TextureLoadingInfo(tex, size.x, size.y);
}


//--- Textures ---
Load<TextureLoadingInfo> lightmap_texture(LoadTagDefault,[]() -> TextureLoadingInfo const* {
    return load_texture_from_png(data_path("resources/textures/noise.png"),false,false);
});


//---- Meshes and Scene----

GLuint sumo_character_meshes_for_lit_color_texture_program = 0;
Load< MeshBuffer > sumo_character_meshes(LoadTagDefault, []() -> MeshBuffer const* {
	MeshBuffer const* ret = new MeshBuffer(data_path("sumo_character.pnct"));
	sumo_character_meshes_for_lit_color_texture_program = ret->make_vao_for_program(lit_color_texture_program->program);
	return ret;
	});

GLuint sumo_scene_meshes_for_lit_color_texture_program = 1;
Load< MeshBuffer > sumo_scene_meshes(LoadTagDefault, []() -> MeshBuffer const* {
	MeshBuffer const* ret = new MeshBuffer(data_path("sumo_scene.pnct"));
	sumo_scene_meshes_for_lit_color_texture_program = ret->make_vao_for_program(lit_color_texture_program->program);
	return ret;
	});

Load< Scene > sumo_scene(LoadTagDefault, []() -> Scene const* {
return new Scene(data_path("sumo.scene"), [&](Scene& scene, Scene::Transform* transform, std::string const& mesh_name) {
        Mesh const& mesh = sumo_scene_meshes->lookup(mesh_name);
        std::cout<< "adding mesh " << mesh_name << " to scene" << std::endl;
        scene.drawables.emplace_back(transform);
        transform->name = mesh_name;
        Scene::Drawable& drawable = scene.drawables.back();
        drawable.pipeline.program = lit_color_texture_program->program;
        drawable.pipeline.vao = sumo_scene_meshes_for_lit_color_texture_program;
        drawable.pipeline.type = mesh.type;
        drawable.pipeline.start = mesh.start;
        drawable.pipeline.count = mesh.count;
        drawable.pipeline.textures[0].target = GL_TEXTURE_2D;
        drawable.pipeline.textures[0].texture = lightmap_texture->texture;
    });
});


