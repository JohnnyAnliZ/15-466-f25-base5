#include "PlayMode.hpp"

#include "LitColorTextureProgram.hpp"
#include "Registry.hpp"

#include "DrawLines.hpp"
#include "gl_errors.hpp"
#include "data_path.hpp"
#include "Mesh.hpp"
#include "Load.hpp"
#include "hex_dump.hpp"

#include <glm/gtc/type_ptr.hpp>
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/string_cast.hpp>

#include <random>
#include <array>
#include <unordered_map>






PlayMode::PlayMode(Client &client_) : client(client_),scene(*sumo_scene){


	//std::cout << "no camera" << game.players.size() << std::endl;
	
	//get pointer to camera for convenience:
	if (scene.cameras.size() != 1) throw std::runtime_error("Expecting scene to have exactly one camera, but it has " + std::to_string(scene.cameras.size()));
	camera = &scene.cameras.front();

	//std::cout << "got camera" <<game.players.size()<< std::endl;

}

PlayMode::~PlayMode() {
}




bool PlayMode::handle_event(SDL_Event const &evt, glm::uvec2 const &window_size) {
	

	if (evt.type == SDL_EVENT_KEY_DOWN) {
		if (evt.key.repeat) {
			//ignore repeats
		} else if (evt.key.key == SDLK_A) {
			controls.left.downs += 1;
			controls.left.pressed = true;
			return true;
		} else if (evt.key.key == SDLK_D) {
			controls.right.downs += 1;
			controls.right.pressed = true;
			return true;
		} else if (evt.key.key == SDLK_W) {
			controls.up.downs += 1;
			controls.up.pressed = true;
			return true;
		} else if (evt.key.key == SDLK_S) {
			controls.down.downs += 1;
			controls.down.pressed = true;
			return true;
		} else if (evt.key.key == SDLK_SPACE) {
			controls.jump.downs += 1;
			controls.jump.pressed = true;
			return true;
		}
	} else if (evt.type == SDL_EVENT_KEY_UP) {
		if (evt.key.key == SDLK_A) {
			controls.left.pressed = false;
			return true;
		} else if (evt.key.key == SDLK_D) {
			controls.right.pressed = false;
			return true;
		} else if (evt.key.key == SDLK_W) {
			controls.up.pressed = false;
			return true;
		} else if (evt.key.key == SDLK_S) {
			controls.down.pressed = false;
			return true;
		} else if (evt.key.key == SDLK_SPACE) {
			controls.jump.pressed = false;
			return true;
		}
	}

	return false;
}

void PlayMode::update(float elapsed) {
	//send/receive data:
	client.poll([this](Connection *c, Connection::Event event){
		if (event == Connection::OnOpen) {
			std::cout << "[" << c->socket << "] opened" << std::endl;
		} else if (event == Connection::OnClose) {
			std::cout << "[" << c->socket << "] closed (!)" << std::endl;
			throw std::runtime_error("Lost connection to server!");
		} else { assert(event == Connection::OnRecv);
			//std::cout << "[" << c->socket << "] recv'd data. Current buffer:\n" << hex_dump(c->recv_buffer); std::cout.flush(); //DEBUG
			bool handled_message;
			try {
				do {
					handled_message = false;
					if (game.recv_state_message(c)) handled_message = true;
				} while (handled_message);
			} catch (std::exception const &e) {
				std::cerr << "[" << c->socket << "] malformed message from server: " << e.what() << std::endl; 
				//quit the game:
				throw e;
			}
		}
	}, 0.0);

	//clean up opponents that are no longer present
	for(auto it = opponents.begin(); it != opponents.end(); ) {
		bool found = false;
		for (auto const& player : game.players) {
			if (player.id == it->first) {
				found = true;
				break;
			}
		}
		if (!found) {
			std::cout << "removing opponent " << it->first <<"new size is "<<players<<std::endl;
			it = opponents.erase(it);
			players--;
			
		} else {
			++it;
		}
	}

	

	//if the size of players is larger than current number
	if (game.players.size() > players) {
		size_t newOnes = game.players.size() - players;//by how much?
		if (newOnes != 1) {
			std::cout << "more than one at a time" << std::endl;
		}
		auto end = game.players.end();
		//player iterator to the new one
		auto pit = &game.players.back();
		end--;
		if (pit != &(*end))std::cout << "the back doesn't match end-1" << std::endl;
		
		std::cout << "adding " << newOnes << " additional player" << std::endl;
		for (uint32_t i = 0; i < newOnes; i++) {
			//create new transform for the player based on game state sent from server
			
			/*Scene::Transform h;
			Scene::Transform a;
			Scene::Transform f;
			Scene::Transform t;*/

			
			scene.transforms.emplace_back();
			pit->head = &scene.transforms.back();
			scene.transforms.emplace_back();
			pit->arms = &scene.transforms.back();
			scene.transforms.emplace_back();
			pit->foot = &scene.transforms.back();
			scene.transforms.emplace_back();
			pit->torso = &scene.transforms.back();



			if (pit->head == nullptr)std::cout << "still no head" << std::endl;
			if (pit->arms == nullptr)std::cout << "still no arms" << std::endl;
			if (pit->foot == nullptr)std::cout << "still no foot" << std::endl;
			if (pit->torso == nullptr)std::cout << "still no torso" << std::endl;

			
			//initialize positions
			pit->head->position = glm::vec3(0, -0.86f, 3.4f);
			pit->arms->position = glm::vec3(0);



			//just like in the update view rendering function, compute foot and torso position based on the player's yaw
			float s2 = pit->rightFoot ? 1.0f : -1.0f;
			rightFoot = pit->rightFoot;
			if (!rightFoot && &game.players.front() == pit) {
				std::cout << "new player doesn't start with rightFoot" << std::endl;
			}
			pit->foot->position.z = - pit->TTFOZ;
			pit->foot->position.x = pit->position.x + s2 * pit->TTFOXY * std::cos(glm::radians(pit->yaw));
			pit->foot->position.y = pit->position.y - s2 * pit->TTFOXY * std::sin(glm::radians(pit->yaw));
			pit->torso->position.z = pit->TTFOZ;
			pit->torso->position.x = -1.0f * s2 * pit->TTFOXY;

			


			if (&game.players.front() == pit) {
				std::cout << "assigning playmode transforms to be the character " << pit->id << "'s transform" << std::endl;
				assert(pit->foot != nullptr);
				arms = pit->arms;
				head = pit->head;
				torso = pit->torso;
				foot = pit->foot;

				arms->parent = torso;
				head->parent = torso;
				torso->parent = foot;
			}
			
			
			else {
				opponents[pit->id] = Opponent();
				opponents[pit->id].arms = pit->arms;
				opponents[pit->id].head = pit->head;
				opponents[pit->id].torso = pit->torso;
				opponents[pit->id].foot = pit->foot;
				rightFeet[pit->id] = pit->rightFoot;

				opponents[pit->id].arms->parent = opponents[pit->id].torso;
				opponents[pit->id].head->parent = opponents[pit->id].torso;
				opponents[pit->id].torso->parent = opponents[pit->id].foot;
				if (opponents[pit->id].foot == nullptr) {
					std::cout << "this player " << pit->id << " has no foot" << std::endl;
				}
			}

			
			
			//add the drawables to the scene
			auto addDrawables = [&](Scene& scene, Scene::Transform* transform, std::string const& mesh_name) {
				Mesh const& mesh = sumo_character_meshes->lookup(mesh_name);
 				
				scene.drawables.emplace_back(transform);
				Scene::Drawable& drawable = scene.drawables.back();
				transform->name = mesh_name;
				drawable.pipeline = lit_color_texture_program_pipeline;
				drawable.pipeline.vao = sumo_character_meshes_for_lit_color_texture_program;
				drawable.pipeline.type = mesh.type;
				drawable.pipeline.start = mesh.start;
				drawable.pipeline.count = mesh.count;
				};
			addDrawables(scene, pit->arms, "ArmsM");
			addDrawables(scene, pit->head, "HeadM");
			addDrawables(scene, pit->torso, "TorsoM");
			addDrawables(scene, pit->foot, "FootM");

			end--;
			pit = &(*end);
			players++;
		}
	}
	
	if (opponents.find(game.players.back().id) != opponents.end()) {
		//if(opponents[game.players.back().id].foot != nullptr)std::cout << "not null" << std::endl;
	}


	//using the received data, do something to the transforms to make the view happen
	for (auto const& player : game.players) {
		if (&player == &game.players.front()) {//if player is me, get my transform and change it 
			if (foot == nullptr) {
				std::cout << "no foot" << std::endl;
			}
			//rotate the foot about z
			glm::quat rot = glm::normalize(glm::angleAxis(glm::radians(player.yaw), glm::vec3(0.0f, 0.0f, 1.0f)) * glm::quat(1.0f, 0.0f, 0.0f, 0.0f));
			//rotate the foot about local y
			rot = glm::normalize(rot * glm::angleAxis(glm::radians(player.roll), glm::vec3(0.0f, 1.0f, 0.0f)));
			foot->rotation = rot;
			//player data received from server
			glm::vec3 pos = player.position;
			float Z = player.TTFOZ;
			float XY= player.TTFOXY;
			float sign = player.rightFoot ? 1.0f : -1.0f;

			//the foot position shifts when the sign changes
			if (rightFoot != player.rightFoot) {
				rightFoot = player.rightFoot;
				foot->position.z = pos.z - Z;
				foot->position.x = foot->position.x + 2*sign* XY * std::cos(glm::radians(player.yaw));
				foot->position.y = foot->position.y + 2*sign * XY * std::sin(glm::radians(player.yaw));
			}

			//set the torso's local transform based on which foot it's on
			torso->position.z = Z;
			torso->position.x = -1.0f * sign * XY;
		}
		//update other players
		else {	
			if (opponents[player.id].foot == nullptr) {
				std::cout << "other player " << player.id << " no foot" << std::endl;
			}
			//rotate the foot about z
			glm::quat rot = glm::normalize(glm::angleAxis(glm::radians(player.yaw), glm::vec3(0.0f, 0.0f, 1.0f)) * glm::quat(1.0f, 0.0f, 0.0f, 0.0f));

			//rotate the foot about local y

			rot = glm::normalize(rot * glm::angleAxis(glm::radians(player.roll), glm::vec3(0.0f, 1.0f, 0.0f)));


			opponents[player.id].foot->rotation = rot;

			//data received from server
			glm::vec3 pos = player.position;
			float Z = player.TTFOZ;
			float XY = player.TTFOXY;
			float sign = player.rightFoot ? 1.0f : -1.0f;

			if (rightFeet[player.id] != player.rightFoot) {
				rightFeet[player.id] = player.rightFoot;
				opponents[player.id].foot->position.z = pos.z - Z;
				opponents[player.id].foot->position.x = opponents[player.id].foot->position.x + 2 * sign * XY * std::cos(glm::radians(player.yaw));
				opponents[player.id].foot->position.y = opponents[player.id].foot->position.y + 2 * sign * XY * std::sin(glm::radians(player.yaw));
			}
			//set the torso's local transform based on what foot it's on
			opponents[player.id].torso->position.z = Z;
			opponents[player.id].torso->position.x = -1.0f * sign * XY;
		}
	}

	{//update camera to follow the torso based on player.yaw and world position	
		// glm::vec3 torso_pos = torso->make_world_from_local() * glm::vec4(0.0f, 0.0f, 0.0f, 1.0f);
		// float yaw = game.players.front().yaw;
		// camera->transform->position = torso_pos + glm::vec3(0.0f, 0.0f, 5) + glm::vec3(10.0 * std::sin(glm::radians(yaw)), -10.0f * std::cos(glm::radians(yaw)), 0.0f);
		// camera->transform->rotation = glm::angleAxis(glm::radians(yaw), glm::vec3(0.0f, 0.0f, 1.0f)) * glm::quat(0.801f, 0.599f, 0.0f, 0.0f);
	}

	{//controls
		controls.send_controls_message(&client.connection);
		//reset button press counters:
		controls.left.downs = 0;
		controls.right.downs = 0;
		controls.up.downs = 0;
		controls.down.downs = 0;
		controls.jump.downs = 0;
	}

	client.connection.send(Message::C2S_Vec3);
	if(game.players.size() ==0) {
		std::cout << "no players to send position for" << std::endl;
		client.connection.send(glm::vec4(0));
	}
	else{//send the position of the torso calculated using foot and offset
		float ZO = game.players.front().TTFOZ;
		float XYO = game.players.front().TTFOXY;
		float s = game.players.front().rightFoot ? -1.0f : 1.0f;
		float yaw = game.players.front().yaw;
		glm::vec3 offSet = glm::vec3(s * std::cos(glm::radians(yaw)) * XYO, -s * std::sin(glm::radians(yaw)) * XYO, ZO);
		glm::vec3 snd = foot->position + offSet;
		client.connection.send(snd);
		client.connection.send(game.players.front().id);
		//std::cout << "position sent" << snd.x << snd.y << snd.z << std::endl;
		//std::cout << "foot position" << foot->position.x << foot->position.y << foot->position.z << std::endl;
	}
}

void PlayMode::draw(glm::uvec2 const &drawable_size) {
	//GL_ERRORS();
		//update camera aspect ratio for drawable:
	camera->aspect = float(drawable_size.x) / float(drawable_size.y);

	//set up light type and position for lit_color_texture_program:
	// TODO: consider using the Light(s) in the scene to do this
	glUseProgram(lit_color_texture_program->program);
	glUniform1i(lit_color_texture_program->LIGHT_TYPE_int, 1);
	glUniform3fv(lit_color_texture_program->LIGHT_DIRECTION_vec3, 1, glm::value_ptr(glm::vec3(0.0f, 0.0f, -1.0f)));
	glUniform3fv(lit_color_texture_program->LIGHT_ENERGY_vec3, 1, glm::value_ptr(glm::vec3(1.0f, 1.0f, 0.95f)));
	glUseProgram(0);

	glClearColor(0.5f, 0.5f, 0.5f, 1.0f);
	glClearDepth(1.0f); //1.0 is actually the default value to clear the depth buffer to, but FYI you can change it.
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	glEnable(GL_DEPTH_TEST);
	glDepthFunc(GL_LESS); //this is the default depth comparison function, but FYI you can change it.

	scene.draw(*camera);


	GL_ERRORS();
}
