#include "Mode.hpp"

#include "Connection.hpp"
#include "Game.hpp"
#include "Scene.hpp"
#include "Sound.hpp"

#include <glm/glm.hpp>

#include <vector>
#include <deque>
#include <unordered_map>

struct PlayMode : Mode {
	PlayMode(Client &client);
	virtual ~PlayMode();

	//functions called by main loop:
	virtual bool handle_event(SDL_Event const &, glm::uvec2 const &window_size) override;
	virtual void update(float elapsed) override;
	virtual void draw(glm::uvec2 const &drawable_size) override;


	//----- scene -----
	Scene scene;

	

	//camera:
	Scene::Camera* camera = nullptr;

	//----- game state -----

	//the transforms to controll using the game state sent from the server
	Scene::Transform* head= nullptr;
	Scene::Transform* torso	= nullptr;
	Scene::Transform* arms= nullptr;
	Scene::Transform* foot= nullptr;

	struct Opponent{
		Scene::Transform* head	= nullptr;
		Scene::Transform* torso	= nullptr;
		Scene::Transform* arms	= nullptr;
		Scene::Transform* foot	= nullptr;
	};

	std::unordered_map<uint32_t, Opponent> opponents;

	std::unordered_map<uint32_t, bool> rightFeet;

	//input tracking for local player:
	Player::Controls controls;

	//which foot
	bool rightFoot = true;

	//track player numbers
	size_t players = 0;

	//latest game state (from server):
	Game game;

	//last message from server:
	std::string server_message;

	//connection to server:
	Client &client;

};
