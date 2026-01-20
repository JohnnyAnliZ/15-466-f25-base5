#include "Scene.hpp"
#include "CubemapProgram.hpp"
#include "DirectLightInjectProgram.hpp"
#include "ColorTextureProgram.hpp"


#include "gl_errors.hpp"
#include "read_write_chunk.hpp"

#include <glm/gtc/type_ptr.hpp>
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/string_cast.hpp>

#include <fstream>

//-------------------------

glm::mat4x3 Scene::Transform::make_parent_from_local() const {
	//compute:
	//   translate   *   rotate    *   scale
	// [ 1 0 0 p.x ]   [       0 ]   [ s.x 0 0 0 ]
	// [ 0 1 0 p.y ] * [ rot   0 ] * [ 0 s.y 0 0 ]
	// [ 0 0 1 p.z ]   [       0 ]   [ 0 0 s.z 0 ]
	//                 [ 0 0 0 1 ]   [ 0 0   0 1 ]

	glm::mat3 rot = glm::mat3_cast(rotation);
	return glm::mat4x3(
		rot[0] * scale.x, //scaling the columns here means that scale happens before rotation
		rot[1] * scale.y,
		rot[2] * scale.z,
		position
	);
}

glm::mat4x3 Scene::Transform::make_local_from_parent() const {
	//compute:
	//   1/scale       *    rot^-1   *  translate^-1
	// [ 1/s.x 0 0 0 ]   [       0 ]   [ 0 0 0 -p.x ]
	// [ 0 1/s.y 0 0 ] * [rot^-1 0 ] * [ 0 0 0 -p.y ]
	// [ 0 0 1/s.z 0 ]   [       0 ]   [ 0 0 0 -p.z ]
	//                   [ 0 0 0 1 ]   [ 0 0 0  1   ]

	glm::vec3 inv_scale;
	//taking some care so that we don't end up with NaN's , just a degenerate matrix, if scale is zero:
	inv_scale.x = (scale.x == 0.0f ? 0.0f : 1.0f / scale.x);
	inv_scale.y = (scale.y == 0.0f ? 0.0f : 1.0f / scale.y);
	inv_scale.z = (scale.z == 0.0f ? 0.0f : 1.0f / scale.z);

	//compute inverse of rotation:
	glm::mat3 inv_rot = glm::mat3_cast(glm::inverse(rotation));

	//scale the rows of rot:
	inv_rot[0] *= inv_scale;
	inv_rot[1] *= inv_scale;
	inv_rot[2] *= inv_scale;

	return glm::mat4x3(
		inv_rot[0],
		inv_rot[1],
		inv_rot[2],
		inv_rot * -position
	);
}

glm::mat4x3 Scene::Transform::make_world_from_local() const {
	if (!parent) {
		return make_parent_from_local();
	} else {
		return parent->make_world_from_local() * glm::mat4(make_parent_from_local()); //note: glm::mat4(glm::mat4x3) pads with a (0,0,0,1) row
	}
}
glm::mat4x3 Scene::Transform::make_local_from_world() const {
	if (!parent) {
		return make_local_from_parent();
	} else {
		return make_local_from_parent() * glm::mat4(parent->make_local_from_world()); //note: glm::mat4(glm::mat4x3) pads with a (0,0,0,1) row
	}
}

//-------------------------

glm::mat4 Scene::Camera::make_projection() const {
	return glm::infinitePerspective( fovy, aspect, near );
}

//-------------------------


//render every texel a point light could see to a cubemap
void Scene::renderToCubemap(Cubemap const &cubemap, Scene::Camera const &camera,Light const& light){
	GLint prev_viewport[4];
	glGetIntegerv(GL_VIEWPORT,prev_viewport);
	//create framebuffer 
	GLuint fbo;
	glGenFramebuffers(1, &fbo);
	glBindFramebuffer(GL_FRAMEBUFFER, fbo);
	// ensure color attachment is the draw buffer for this FBO
	{
		GLenum draw_buffers[1] = { GL_COLOR_ATTACHMENT0 };
		glDrawBuffers(1, draw_buffers);
	}

	//enable depth test
	glEnable(GL_DEPTH_TEST);
	glDepthFunc(GL_LESS);
	GLuint depth;
	glGenRenderbuffers(1, &depth);
	glBindRenderbuffer(GL_RENDERBUFFER, depth);
	glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24,
						cubemap.width, cubemap.width);
	glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT,
							GL_RENDERBUFFER, depth);
	//disable blend
	glDisable(GL_BLEND);
	{//draw to each face using the pipeline
		Scene::Drawable::Pipeline &pipeline = cubemap_pipeline;
		glUseProgram(pipeline.program);
		for(uint32_t i = 0; i < 6; i++){
			//attach the cubemap face to framebuffer
			glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
			GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, cubemap.tex, 0);
			glViewport(0,0,cubemap.width,cubemap.width);
			// ensure FBO is complete
			GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
			if (status != GL_FRAMEBUFFER_COMPLETE) {
				throw std::runtime_error("Framebuffer incomplete when creating lightmap (status=" + std::to_string(status) + ")");
			}
			glClearDepth(1.0f);
			glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
			glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
			
    
			glm::mat4 projection = glm::perspective(glm::radians(90.0f), 1.0f, 0.01f, 100.0f);
			
			glm::mat4 views[6] = {
				glm::lookAt(light.transform->position, light.transform->position + glm::vec3( 1, 0, 0), glm::vec3(0,-1, 0)),
				glm::lookAt(light.transform->position, light.transform->position + glm::vec3(-1, 0, 0), glm::vec3(0,-1, 0)),
				glm::lookAt(light.transform->position, light.transform->position + glm::vec3( 0, 1, 0), glm::vec3(0, 0, 1)),
				glm::lookAt(light.transform->position, light.transform->position + glm::vec3( 0,-1, 0), glm::vec3(0, 0,-1)),
				glm::lookAt(light.transform->position, light.transform->position + glm::vec3( 0, 0, 1), glm::vec3(0,-1, 0)),
				glm::lookAt(light.transform->position, light.transform->position + glm::vec3( 0, 0,-1), glm::vec3(0,-1, 0)),
			};
			for(auto const & drawable: drawables){
				if(drawable.pipeline.program != color_texture_pipeline.program) continue;
				
				//std::cout<<"drawing "<<drawable.transform->name<<std::endl;
				     
				glm::mat4x3 world_from_object = drawable.transform->make_world_from_local();
				glm::mat4 clip_from_world = projection * views[i];
				glm::mat4 clip_from_object = clip_from_world * glm::mat4(world_from_object);
				glUniformMatrix4fv(pipeline.CLIP_FROM_OBJECT_mat4, 1, GL_FALSE, glm::value_ptr(clip_from_object));
				glBindVertexArray(drawable.pipeline.vao);
				glDrawArrays(GL_TRIANGLES, drawable.pipeline.start, drawable.pipeline.count);
				GL_ERRORS();
			}
		}
	}
	//delete and restore
	glBindRenderbuffer(GL_RENDERBUFFER, 0);
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	glDeleteFramebuffers(1, &fbo);
	glDeleteRenderbuffers(1, &depth);
	glViewport(
    prev_viewport[0],
    prev_viewport[1],
    prev_viewport[2],
    prev_viewport[3]
	);
}

//new inject direct lighting function that injects cubemap contents using the CPU instead of shaders
void Scene::injectCubemapToLightmap(Cubemap const &cubemap, Lightmap const &lightmap, Light const &light){
	//declare vector to store cubemap
	struct UV{
		float u = 0.0f;
		float v = 0.0f;
	};
	uint32_t cubemap_width = cubemap.width;
	uint32_t cubemap_face_size = cubemap_width * cubemap_width; 
	uint32_t cubemap_size = cubemap_face_size * 6; 
	std::vector<UV> cubemap_uvs(cubemap_size, UV{-1.0f,-1.0f});
	// //declare vector to store lightmap initialized to black
	std::vector<glm::vec4> lightmap_colors(lightmap.width * lightmap.height, glm::vec4(0.0f,0.0f,0.0f,1.0f));
	
	glBindTexture(GL_TEXTURE_CUBE_MAP, cubemap.tex);
	for (int i = 0; i < 6; ++i) {
		GLint w = 0,  internal = 0;
		glGetTexLevelParameteriv(
			GL_TEXTURE_CUBE_MAP_POSITIVE_X + i,
			0,
			GL_TEXTURE_WIDTH,
			&w
		);
		glGetTexLevelParameteriv(
			GL_TEXTURE_CUBE_MAP_POSITIVE_X + i,
			0,
			GL_TEXTURE_INTERNAL_FORMAT,
			&internal
		);

		// std::cout << "Face " << i << " width=" << w
		// 		<< " format=" << internal << std::endl;
	}
	for(uint32_t i = 0; i < 6; i ++){
		glGetTexImage(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, GL_RG, GL_FLOAT, cubemap_uvs.data() + i * cubemap_face_size);	
	}
	
	GLenum err = glGetError();
	assert(err == GL_NO_ERROR);
	glBindTexture(GL_TEXTURE_CUBE_MAP, 0);
	//write to the lightmap
	for(uint32_t j = 0; j < cubemap_size; j++){
		UV cur_uv = cubemap_uvs[j];
		if (cur_uv.u <= 0.0f && cur_uv.v <= 0.0f) continue;

		//std::cout<<"actual uvs: "<< cur_uv.u << " "<< cur_uv.v<<std::endl;
		//calculate using rasmus's equation
		//this coordinate only keeps track of where in a cube face a pixel is in, doesn't map to each face
		uint32_t x_texel = (j % cubemap_face_size) % cubemap_width;
		uint32_t y_texel = (j % cubemap_face_size) / cubemap_width;
		glm::vec2 cubemap_coordinate;
		cubemap_coordinate.x = (x_texel + 0.5f) / cubemap_width * 2.0f - 1.0f;  // -1 to 1
		cubemap_coordinate.y = (y_texel + 0.5f) / cubemap_width * 2.0f - 1.0f;  // -1 to 1_width, 1);
		float denominator = std::powf(std::powf(cubemap_coordinate.x,2) + std::powf(cubemap_coordinate.y, 2) + 1, 1.5f);
		glm::vec4 lightOutput = glm::vec4(light.energy * 240.0f / (cubemap_size * denominator), 1.0);//rasmus's equation
		uint32_t x = std::min(uint32_t(cur_uv.u * lightmap.width),  lightmap.width  - 1);
		uint32_t y = std::min(uint32_t(cur_uv.v * lightmap.height), lightmap.height - 1);

		if(y * lightmap.width + x >= lightmap.width * lightmap.height){
			std::cout<<"array out of bounds fucker"<<std::endl;
		}
		lightmap_colors[y * lightmap.width + x] += lightOutput;
	}
	//write to the lightmap texture
	glBindTexture(GL_TEXTURE_2D, lightmap.tex);
	glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, lightmap.width, lightmap.height, GL_RGBA, GL_FLOAT, lightmap_colors.data());
	glBindTexture(GL_TEXTURE_2D,0);	
	
}




void Scene::injectDirectLighting(Lightmap const &lightmap){
	// Create framebuffer
	GLint prev_viewport[4];
	glGetIntegerv(GL_VIEWPORT, prev_viewport);

	GLuint fbo;
	glGenFramebuffers(1, &fbo);
	glBindFramebuffer(GL_FRAMEBUFFER, fbo);


	// ensure color attachment is the draw buffer for this FBO
	{
		GLenum draw_buffers[1] = { GL_COLOR_ATTACHMENT0 };
		glDrawBuffers(1, draw_buffers);
	}
	//attach lightmap texture to framebuffer
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, 
                          GL_TEXTURE_2D, lightmap.tex, 0);
	// Set viewport to lightmap size   
	glViewport(0, 0, lightmap.width, lightmap.height);

	// ensure FBO is complete
	GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
	if (status != GL_FRAMEBUFFER_COMPLETE) {
		throw std::runtime_error("Framebuffer incomplete when creating lightmap (status=" + std::to_string(status) + ")");
	}
    
	// Clear to black (no light)
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

	//depth test to avoid light leaking through walls
	glEnable(GL_DEPTH_TEST);
	glDepthFunc(GL_LESS);
	glClearDepth(1.0f);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    
    // Enable additive blending (multiple lights)
    glEnable(GL_BLEND);
    glBlendFunc(GL_ONE, GL_ONE);
    
	//get the direct light inject pipeline
	Scene::Drawable::Pipeline const &pipeline = direct_light_inject_pipeline;
    // Use inject shader
	glUseProgram(pipeline.program);
	// if (pipeline.WORLD_FROM_OBJECT_mat4 == -1U && pipeline.WORLD_FROM_OBJECT_mat4x3 == -1U) {
	// 	throw std::runtime_error("DirectLightInjectProgram pipeline missing WORLD_FROM_OBJECT (mat4 or mat4x3) uniform");
	// }
	if(pipeline.LIGHT_FROM_OBJECT_mat4x3 == -1U ) {
		throw std::runtime_error("DirectLightInjectProgram pipeline missing LIGHT_FROM_OBJECT_mat4x3 uniform");
	}
	if(pipeline.LIGHT_FROM_NORMAL_mat3 == -1U ) {
		throw std::runtime_error("DirectLightInjectProgram pipeline missing LIGHT_FROM_NORMAL_mat3 uniform");
	}

	// if(pipeline.LIGHT_LOCATION_vec3 == -1U ) {
	// 	throw std::runtime_error("DirectLightInjectProgram pipeline missing LIGHT_LOCATION_vec3 uniform");
	// }
	if(pipeline.LIGHT_ENERGY_vec3 == -1U ) {
		throw std::runtime_error("DirectLightInjectProgram pipeline missing LIGHT_ENERGY_vec3 uniform");
	}

    // Render scene once per light
    for (const Light& light : lights) {
		
		glUniform3fv(pipeline.LIGHT_LOCATION_vec3, 1, glm::value_ptr(light.transform->position));
		glUniform3fv(pipeline.LIGHT_ENERGY_vec3, 1, glm::value_ptr(light.energy));
		// set cutoff to avoid division by zero in shader
		glUniform1f(pipeline.LIGHT_CUTOFF_float, 1.0f);
		
        //inject the light into only the static scene meshes
        for (auto const &drawable : drawables) {
			//only if it uses color texture pipeline
			if(drawable.pipeline.program != color_texture_pipeline.program) continue;
			assert(drawable.transform); //drawables *must* have a transform
			glm::mat4x3 world_from_object = drawable.transform->make_world_from_local();
			if (pipeline.WORLD_FROM_OBJECT_mat4 != -1U) {
				glm::mat4 world_from_object_4 = glm::mat4(world_from_object);
				glUniformMatrix4fv(pipeline.WORLD_FROM_OBJECT_mat4, 1, GL_FALSE, glm::value_ptr(world_from_object_4));
			} else if (pipeline.WORLD_FROM_OBJECT_mat4x3 != -1U) {
				glUniformMatrix4x3fv(pipeline.WORLD_FROM_OBJECT_mat4x3, 1, GL_FALSE, glm::value_ptr(world_from_object));
			}

			//here, light_space is just world space
			glm::mat4x3 light_from_object =  glm::mat4(world_from_object);
			glUniformMatrix4x3fv(pipeline.LIGHT_FROM_OBJECT_mat4x3, 1, GL_FALSE, glm::value_ptr(light_from_object));
			glm::mat3 light_from_normal = glm::inverse(glm::transpose(glm::mat3(light_from_object)));
			glUniformMatrix3fv(pipeline.LIGHT_FROM_NORMAL_mat3, 1, GL_FALSE, glm::value_ptr(light_from_normal));
			//set light specific uniforms
			glUniform1i(pipeline.LIGHT_TYPE_int, static_cast<int>(light.type));
			glUniform3fv(pipeline.LIGHT_LOCATION_vec3, 1, glm::value_ptr(light.transform->position));
			glUniform3fv(pipeline.LIGHT_ENERGY_vec3, 1, glm::value_ptr(light.energy));
			//optional uniforms for different light types
			if(pipeline.LIGHT_CUTOFF_float != -1U){
				std::cout<<"spot fov: "<<light.spot_fov<<std::endl;
				glUniform1f(pipeline.LIGHT_CUTOFF_float, light.spot_fov*0.5f);
			}
			if(pipeline.LIGHT_DIRECTION_vec3 != -1U){
				glm::vec3 direction = glm::normalize(-glm::mat3_cast(light.transform->rotation) * glm::vec3(0.0f, 0.0f, 1.0f));
				glUniform3fv(pipeline.LIGHT_DIRECTION_vec3, 1, glm::value_ptr(direction));
			}     
            glBindVertexArray(drawable.pipeline.vao);
            glDrawArrays(GL_TRIANGLES, drawable.pipeline.start, drawable.pipeline.count);
        }
    }
    
	glDisable(GL_BLEND);
	// re-enable depth test for subsequent scene rendering


	// restore previous framebuffer/viewport
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	glViewport(prev_viewport[0], prev_viewport[1], prev_viewport[2], prev_viewport[3]);
	glDeleteFramebuffers(1, &fbo);
}


void Scene::draw(Camera const &camera) const {
	assert(camera.transform);
	glm::mat4 clip_from_world = camera.make_projection() * glm::mat4(camera.transform->make_local_from_world());

	//right now it's just one light
	glm::mat4x3 light_from_world = lights.begin()->transform->make_local_from_world();
	draw(clip_from_world, light_from_world);
}



void Scene::draw(glm::mat4 const &clip_from_world, glm::mat4x3 const &light_from_world) const {

	//Iterate through all drawables, sending each one to OpenGL:
	for (auto const &drawable : drawables) {
		//Reference to drawable's pipeline for convenience:
		Scene::Drawable::Pipeline const &pipeline = drawable.pipeline;

		//skip any drawables without a shader program set:
		if (pipeline.program == 0) continue;
		//skip any drawables that don't reference any vertex array:
		if (pipeline.vao == 0) continue;
		//skip any drawables that don't contain any vertices:
		if (pipeline.count == 0) continue;


		//Set shader program:wd
		glUseProgram(pipeline.program);

		if(pipeline.program == color_texture_pipeline.program){
			glActiveTexture(GL_TEXTURE0);
			glBindTexture(GL_TEXTURE_2D, drawable.pipeline.textures[0].texture);	

			
		}
		//Set attribute sources:
		glBindVertexArray(pipeline.vao);

		//Configure program uniforms:

		//the object-to-world matrix is used in all three of these uniforms:
		assert(drawable.transform); //drawables *must* have a transform
		
		glm::mat4x3 world_from_object = drawable.transform->make_world_from_local();
		

		if (pipeline.WORLD_FROM_OBJECT_mat4 != -1U) {
			glUniformMatrix4fv(pipeline.WORLD_FROM_OBJECT_mat4, 1, GL_FALSE, glm::value_ptr(world_from_object));
		}

		//CLIP_FROM_OBJECT takes vertices from object space to clip space:
		if (pipeline.CLIP_FROM_OBJECT_mat4 != -1U) {
			glm::mat4 clip_from_object = clip_from_world * glm::mat4(world_from_object);
			glUniformMatrix4fv(pipeline.CLIP_FROM_OBJECT_mat4, 1, GL_FALSE, glm::value_ptr(clip_from_object));
		}

		//the object-to-light matrix is used in the next two uniforms:
		glm::mat4x3 light_from_object = light_from_world * glm::mat4(world_from_object);

		//LIGHT_FROM_OBJECT takes vertices from object space to light space:
		if (pipeline.LIGHT_FROM_OBJECT_mat4x3 != -1U) {
			glUniformMatrix4x3fv(pipeline.LIGHT_FROM_OBJECT_mat4x3, 1, GL_FALSE, glm::value_ptr(light_from_object));
		}
		
		//LIGHT_FROM_NORMAL takes normals from object space to light space:
		if (pipeline.LIGHT_FROM_NORMAL_mat3 != -1U) {
			glm::mat3 light_from_normal = glm::inverse(glm::transpose(glm::mat3(light_from_object)));
			glUniformMatrix3fv(pipeline.LIGHT_FROM_NORMAL_mat3, 1, GL_FALSE, glm::value_ptr(light_from_normal));
		}

		//set any requested custom uniforms:
		if (pipeline.set_uniforms) pipeline.set_uniforms();

		//set up textures:
		for (uint32_t i = 0; i < Drawable::Pipeline::TextureCount; ++i) {
			if (pipeline.textures[i].texture != 0) {
				glActiveTexture(GL_TEXTURE0 + i);
				glBindTexture(pipeline.textures[i].target, pipeline.textures[i].texture);
			}
		}


		//draw the object:
		glDrawArrays(pipeline.type, pipeline.start, pipeline.count);

		//un-bind textures:
		for (uint32_t i = 0; i < Drawable::Pipeline::TextureCount; ++i) {
			if (pipeline.textures[i].texture != 0) {
				glActiveTexture(GL_TEXTURE0 + i);
				glBindTexture(pipeline.textures[i].target, 0);
			}
		}
		glActiveTexture(GL_TEXTURE0);

	}

	glUseProgram(0);
	glBindVertexArray(0);

	GL_ERRORS();
}


void Scene::load(std::string const &filename,
	std::function< void(Scene &, Transform *, std::string const &) > const &on_drawable) {

	std::ifstream file(filename, std::ios::binary);

	std::vector< char > names;
	read_chunk(file, "str0", &names);

	struct HierarchyEntry {
		uint32_t parent;
		uint32_t name_begin;
		uint32_t name_end;
		glm::vec3 position;
		glm::quat rotation;
		glm::vec3 scale;
	};
	static_assert(sizeof(HierarchyEntry) == 4 + 4 + 4 + 4*3 + 4*4 + 4*3, "HierarchyEntry is packed.");
	std::vector< HierarchyEntry > hierarchy;
	read_chunk(file, "xfh0", &hierarchy);

	struct MeshEntry {
		uint32_t transform;
		uint32_t name_begin;
		uint32_t name_end;
	};
	static_assert(sizeof(MeshEntry) == 4 + 4 + 4, "MeshEntry is packed.");
	std::vector< MeshEntry > meshes;
	read_chunk(file, "msh0", &meshes);

	struct CameraEntry {
		uint32_t transform;
		char type[4]; //"pers" or "orth"
		float data; //fov in degrees for 'pers', scale for 'orth'
		float clip_near, clip_far;
	};
	static_assert(sizeof(CameraEntry) == 4 + 4 + 4 + 4 + 4, "CameraEntry is packed.");
	std::vector< CameraEntry > loaded_cameras;
	read_chunk(file, "cam0", &loaded_cameras);

	struct LightEntry {
		uint32_t transform;
		char type;
		glm::u8vec3 color;
		float energy;
		float distance;
		float fov;
	};
	static_assert(sizeof(LightEntry) == 4 + 1 + 3 + 4 + 4 + 4, "LightEntry is packed.");
	std::vector< LightEntry > loaded_lights;
	read_chunk(file, "lmp0", &loaded_lights);


	//--------------------------------
	//Now that file is loaded, create transforms for hierarchy entries:

	std::vector< Transform * > hierarchy_transforms;
	hierarchy_transforms.reserve(hierarchy.size());

	for (auto const &h : hierarchy) {
		transforms.emplace_back();
		Transform *t = &transforms.back();
		if (h.parent != -1U) {
			if (h.parent >= hierarchy_transforms.size()) {
				throw std::runtime_error("scene file '" + filename + "' did not contain transforms in topological-sort order.");
			}
			t->parent = hierarchy_transforms[h.parent];
		}

		if (h.name_begin <= h.name_end && h.name_end <= names.size()) {
			t->name = std::string(names.begin() + h.name_begin, names.begin() + h.name_end);
		} else {
				throw std::runtime_error("scene file '" + filename + "' contains hierarchy entry with invalid name indices");
		}

		t->position = h.position;
		t->rotation = h.rotation;
		t->scale = h.scale;

		hierarchy_transforms.emplace_back(t);
	}
	assert(hierarchy_transforms.size() == hierarchy.size());

	for (auto const &m : meshes) {
		if (m.transform >= hierarchy_transforms.size()) {
			throw std::runtime_error("scene file '" + filename + "' contains mesh entry with invalid transform index (" + std::to_string(m.transform) + ")");
		}
		if (!(m.name_begin <= m.name_end && m.name_end <= names.size())) {
			throw std::runtime_error("scene file '" + filename + "' contains mesh entry with invalid name indices");
		}
		std::string name = std::string(names.begin() + m.name_begin, names.begin() + m.name_end);

		if (on_drawable) {
			on_drawable(*this, hierarchy_transforms[m.transform], name);
		}

	}

	for (auto const &c : loaded_cameras) {
		if (c.transform >= hierarchy_transforms.size()) {
			throw std::runtime_error("scene file '" + filename + "' contains camera entry with invalid transform index (" + std::to_string(c.transform) + ")");
		}
		if (std::string(c.type, 4) != "pers") {
			std::cout << "Ignoring non-perspective camera (" + std::string(c.type, 4) + ") stored in file." << std::endl;
			continue;
		}
		std::cout << "here's a camera" << hierarchy_transforms[c.transform]->position.y<<std::endl;
		cameras.emplace_back(hierarchy_transforms[c.transform]);
		Camera *camera = &cameras.back();
		camera->fovy = c.data / 180.0f * 3.1415926f; //FOV is stored in degrees; convert to radians.
		camera->near = c.clip_near;
		//N.b. far plane is ignored because cameras use infinite perspective matrices.
	}

	for (auto const &l : loaded_lights) {
		if (l.transform >= hierarchy_transforms.size()) {
			throw std::runtime_error("scene file '" + filename + "' contains lamp entry with invalid transform index (" + std::to_string(l.transform) + ")");
		}
		if (l.type == 'p') {
			//good
		} else if (l.type == 'h') {
			//fine
		} else if (l.type == 's') {
			//okay
		} else if (l.type == 'd') {
			//sure
		} else {
			std::cout << "Ignoring unrecognized lamp type (" + std::string(&l.type, 1) + ") stored in file." << std::endl;
			continue;
		}
		std::cout<<"got light "<<l.type<<std::endl;
		lights.emplace_back(hierarchy_transforms[l.transform]);
		Light *light = &lights.back();
		light->type = static_cast<Light::Type>(l.type);
		light->energy = glm::vec3(l.color) / 255.0f * l.energy;
		light->spot_fov = l.fov / 180.0f * 3.1415926f; //FOV is stored in degrees; convert to radians.
	}

	//load any extra that a subclass wants:
	load_extra(file, names, hierarchy_transforms);

	if (file.peek() != EOF) {
		std::cerr << "WARNING: trailing data in scene file '" << filename << "'" << std::endl;
	}
}

//-------------------------

Scene::Scene(std::string const &filename, std::function< void(Scene &, Transform *, std::string const &) > const &on_drawable) {
	load(filename, on_drawable);
}

Scene::Scene(Scene const &other) {
	set(other);
}

Scene &Scene::operator=(Scene const &other) {
	set(other);
	return *this;
}

void Scene::set(Scene const &other, std::unordered_map< Transform const *, Transform * > *transform_map_) {

	std::unordered_map< Transform const *, Transform * > t2t_temp;
	std::unordered_map< Transform const *, Transform * > &transform_to_transform = *(transform_map_ ? transform_map_ : &t2t_temp);

	transform_to_transform.clear();

	//null transform maps to itself:
	transform_to_transform.insert(std::make_pair(nullptr, nullptr));

	//Copy transforms and store mapping:
	transforms.clear();
	for (auto const &t : other.transforms) {
		transforms.emplace_back();
		transforms.back().name = t.name;
		transforms.back().position = t.position;
		transforms.back().rotation = t.rotation;
		transforms.back().scale = t.scale;
		transforms.back().parent = t.parent; //will update later

		//store mapping between transforms old and new:
		auto ret = transform_to_transform.insert(std::make_pair(&t, &transforms.back()));
		assert(ret.second);
	}

	//update transform parents:
	for (auto &t : transforms) {
		t.parent = transform_to_transform.at(t.parent);
	}

	//copy other's drawables, updating transform pointers:
	drawables = other.drawables;
	for (auto &d : drawables) {
		d.transform = transform_to_transform.at(d.transform);
	}

	//copy other's cameras, updating transform pointers:
	cameras = other.cameras;
	for (auto &c : cameras) {
		c.transform = transform_to_transform.at(c.transform);
	}

	//copy other's lights, updating transform pointers:
	lights = other.lights;
	for (auto &l : lights) {
		l.transform = transform_to_transform.at(l.transform);
	}
}
