#pragma once

/*
 * A scene manages a hierarchical arrangement of transformations (via "Transform").
 *
 * Each transformation may have associated:
 *  - Drawing data (via "Drawable")
 *  - Camera information (via "Camera")
 *  - Light information (via "Light")
 *
 */

#include "GL.hpp"
#include "Load.hpp"

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

#include <list>
#include <array>
#include <memory>
#include <functional>
#include <string>
#include <vector>
#include <unordered_map>


struct Texture {
    GLuint tex = 0;
    uint32_t width,height = 0;
    Texture(GLuint t, uint32_t w, uint32_t h) : tex(t), width(w), height(h) {};
    Texture(): tex(0),width(0),height(0){};
    GLenum target = GL_TEXTURE_2D;
};


//Stores a texture that can be rendered to for light mapping
struct Lightmap: public Texture {
    //constructor where you don't bind the texture altogether
    Lightmap(GLuint t, int w, int h) : Texture(t,w,h) {}

    //constructor where you bind the texture altogether
    Lightmap(int w, int h){
		width = w;
		height = h;
        glGenTextures(1, &tex);
        glBindTexture(GL_TEXTURE_2D, tex);
        
        // RGB for lighting, use floating point for HDR
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB16F, w, h, 0, 
                     GL_RGB, GL_FLOAT, nullptr);
        
        // Linear filtering (lighting is smooth)
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        
        // Clear to black
        std::vector<glm::vec3> blackData(w * h, glm::vec3(0.0f));
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, w, h, 
                       GL_RGB, GL_FLOAT, blackData.data());
    }
};

struct Cubemap{
	GLuint tex;
	uint32_t width;
	GLenum target = GL_TEXTURE_2D;

	Cubemap(uint32_t w){
		glGenTextures(1, &tex);
		glBindTexture(GL_TEXTURE_CUBE_MAP, tex);
		width = w;
		// Create 6 faces, each storing RG (U,V) as floats
		for (uint32_t face = 0; face < 6; face++) {
			glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + face, 
						0, GL_RG32F,  // 2 channels, 32-bit float
						w, w, 0,
						GL_RGBA, GL_FLOAT, nullptr);
		}
		
		glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
		glBindTexture(GL_TEXTURE_CUBE_MAP, 0);
	};
};





struct Scene {
	struct Transform {
		//Transform names are useful for debugging and looking up locations in a loaded scene:
		std::string name;

		//The core function of a transform is to store a transformation in the world:
		glm::vec3 position = glm::vec3(0.0f, 0.0f, 0.0f);
		glm::quat rotation = glm::quat(1.0f, 0.0f, 0.0f, 0.0f); //n.b. wxyz init order
		glm::vec3 scale = glm::vec3(1.0f, 1.0f, 1.0f);

		//The transform above may be relative to some parent transform:
		Transform *parent = nullptr;

		//It is often convenient to construct matrices representing this transformation:
		// ..relative to its parent:
		glm::mat4x3 make_parent_from_local() const;
		glm::mat4x3 make_local_from_parent() const;
		// ..relative to the world:
		glm::mat4x3 make_world_from_local() const;
		glm::mat4x3 make_local_from_world() const;

		//since hierarchy is tracked through pointers, copy-constructing a transform  is not advised:
		Transform(Transform const &) = delete;
		//if we delete some constructors, we need to let the compiler know that the default constructor is still okay:
		Transform() = default;
	};

	struct Drawable {
		//a 'Drawable' attaches attribute data to a transform:
		Drawable(Transform *transform_) : transform(transform_) { assert(transform); }
		Transform * transform;

		//Contains all the data needed to run the OpenGL pipeline:
		struct Pipeline {
			GLuint program = 0; //shader program; passed to glUseProgram

			//attributes:
			GLuint vao = 0; //attrib->buffer mapping; passed to glBindVertexArray

			GLenum type = GL_TRIANGLES; //what sort of primitive to draw; passed to glDrawArrays
			GLuint start = 0; //first vertex to draw; passed to glDrawArrays
			GLuint count = 0; //number of vertices to draw; passed to glDrawArrays

			//uniforms:
			GLuint WORLD_FROM_OBJECT_mat4 = -1U;
			GLuint WORLD_FROM_OBJECT_mat4x3 = -1U;
			GLuint CLIP_FROM_OBJECT_mat4 = -1U; //uniform location for object to clip space matrix
			GLuint LIGHT_FROM_OBJECT_mat4x3 = -1U; //uniform location for object to light space (== world space) matrix
			GLuint LIGHT_FROM_NORMAL_mat3 = -1U; //uniform location for normal to light space (== world space) matrix
			GLuint LIGHT_TYPE_int = -1U;
			GLuint LIGHT_LOCATION_vec3 = -1U;
			GLuint LIGHT_DIRECTION_vec3 = -1U;
			GLuint LIGHT_ENERGY_vec3 = -1U;
			GLuint LIGHT_CUTOFF_float = -1U;
			std::function< void() > set_uniforms; //(optional) function to set any other useful uniforms

			//texture objects to bind for the first TextureCount textures:
			enum : uint32_t { TextureCount = 4 };
			struct TextureInfo {
				GLuint texture = 0;
				GLenum target = GL_TEXTURE_2D;
			} textures[TextureCount];
		} pipeline;
	};

	struct Camera {
		//a 'Camera' attaches camera data to a transform:
		Camera(Transform *transform_) : transform(transform_) { assert(transform); }
		Transform * transform;
		//NOTE: cameras are directed along their -z axis

		//perspective camera parameters:
		float fovy = glm::radians(60.0f); //vertical fov (in radians)
		float aspect = 1.0f; //x / y
		float near = 0.01f; //near plane
		//computed from the above:
		glm::mat4 make_projection() const;
	};

	struct Light {
		//a 'Light' attaches light data to a transform:
		Light(Transform *transform_) : transform(transform_) { assert(transform); }
		Transform * transform;
		//NOTE: directional, spot, and hemisphere lights are directed along their -z axis

		enum Type : char {
			Point = 'p',
			Hemisphere = 'h',
			Spot = 's',
			Directional = 'd'
		} type = Point;

		//light energy convolved with our conventional tristimulus spectra:
		//  (i.e., "red, gree, blue" light color)
		glm::vec3 energy = glm::vec3(1.0f);

		//Spotlight specific:
		float spot_fov = glm::radians(45.0f); //spot cone fov (in radians)
	};

	//Scenes, of course, may have many of the above objects:
	std::list< Transform > transforms;
	std::list< Drawable > drawables;
	std::list< Camera > cameras;
	std::list< Light > lights;

	//render scene uv coordinates to cubemap
	void renderToCubemap(Cubemap const &cubemap, Scene::Camera const &camera, Light const& light);
	//inject light using Cubemap, this one should have shadows
	void injectCubemapToLightmap(Cubemap const &cubemap, Lightmap const &lightmap, Light const &light);

	//inject light into the lightmap texture
	//takes Lightmap by const-ref to avoid needing a full definition here
	void injectDirectLighting(Lightmap const &lightmap);

	//The "draw" function provides a convenient way to pass all the things in a scene to OpenGL:
	void draw(Camera const &camera) const;

	//..sometimes, you want to draw with a custom projection matrix and/or light space:
	void draw(glm::mat4 const &clip_from_world, glm::mat4x3 const &light_from_world = glm::mat4x3(1.0f)) const;

	//add transforms/objects/cameras from a scene file to this scene:
	// the 'on_drawable' callback gives your code a chance to look up mesh data and make Drawables:
	// throws on file format errors
	void load(std::string const &filename,
		std::function< void(Scene &, Transform *, std::string const &) > const &on_drawable = nullptr
	);

	//this function is called to read extra chunks from the scene file after the main chunks are read:
	// this is useful if you, e.g., subclassing scene to represent a game level/area
	virtual void load_extra(std::istream &from, std::vector< char > const &str0, std::vector< Transform * > const &xfh0) { }

	//empty scene:
	Scene() = default;

	//load a scene:
	Scene(std::string const &filename, std::function< void(Scene &, Transform *, std::string const &) > const &on_drawable);

	//copy a scene (with proper pointer fixup):
	Scene(Scene const &); //...as a constructor
	Scene &operator=(Scene const &); //...as scene = scene
	//... as a set() function that optionally returns the transform->transform mapping:
	void set(Scene const &, std::unordered_map< Transform const *, Transform * > *transform_map = nullptr);
};
