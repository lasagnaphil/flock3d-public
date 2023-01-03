#include "boid.h"
#include "ecs.h"

#include <glm/gtc/quaternion.hpp>
#include <glm/gtx/norm.hpp>

#include "nanothread/nanothread.h"

#include "tracy/Tracy.hpp"

inline glm::vec3 quat_log(glm::quat q) {
    constexpr float pi = glm::pi<float>();
    q = glm::normalize(q);
    float a = glm::sqrt(1 - q.w*q.w);
    if (a <= glm::epsilon<float>()) {
        return glm::vec3(0);
    }
    float theta = 2.0f * glm::atan(a, q.w);
    if (theta > pi) {
        theta -= 2*pi;
    }
    else if (theta < -pi) {
        theta += 2*pi;
    }
    glm::vec3 v = theta / a * glm::vec3(q.x, q.y, q.z);
    return v;
}

inline glm::quat quat_exp(glm::vec3 w) {
    float theta = glm::length(w);
    if (theta <= glm::epsilon<float>()) {
        return glm::identity<glm::quat>();
    }
    glm::vec3 u = w / theta;
    return glm::quat(glm::cos(theta/2), glm::sin(theta/2) * u);
}

BoidSystem::BoidSystem(ECS* ecs, Pool* thread_pool, BoidConfig cfg)
	: ecs(ecs), thread_pool(thread_pool), cfg(cfg) {

}

void BoidSystem::update(float dt) {
	ZoneScoped;

	auto boids = ecs->get_component_array<Boid>();
	const int num_boids = boids.ssize();

	auto& camera = ecs->get_component<Camera>(target);
	auto target_pos = camera.position;

// #define BOID_PROXIMITY_NAIVE

	{
		ZoneScopedN("BoidProximityDetection");
		const float nearby_dist_sq = cfg.nearby_dist * cfg.nearby_dist;
		const float avoid_dist_sq = cfg.avoid_dist * cfg.avoid_dist;

		for (int i = 0; i < num_boids; i++) {
			boids[i].nearby_boids.clear();
			boids[i].avoid_boids.clear();
		}

#ifdef BOID_PROXIMITY_NAIVE
		for (int i = 0; i < num_boids; i++) {
			for (int j = i + 1; j < num_boids; j++) {
				float dist_sq = glm::length2(boids[i].pos - boids[j].pos);
				if (dist_sq < nearby_dist_sq) {
					boids[i].nearby_boids.push_back(j);
					boids[j].nearby_boids.push_back(i);
				}
				if (dist_sq < avoid_dist_sq) {
					boids[i].avoid_boids.push_back(j);
					boids[j].avoid_boids.push_back(i);
				}
			}
		}
#else
		cell_map.clear();
		{
			ZoneScopedN("InsertBoids");
			for (int i = 0; i < num_boids; i++) {
				auto& boid = boids[i];

				// Find current cell that this boid reside in
				auto coords = glm::ivec3(glm::floor(boid.pos / cfg.cell_size));
				auto [it, inserted] = cell_map.insert({coords, {}});
				it->second.boid_indices.push_back(i);
			}
		}

		{
			ZoneScopedN("GatherBoidProximity");
			drjit::parallel_for(drjit::blocked_range<int>(0, num_boids, 8), [&](auto range) {
				ZoneScopedN("GatherBoidProximityBlock");
				for (int i : range) {
					auto& boid = boids[i];
					auto coords_center = glm::ivec3(glm::floor(boid.pos / cfg.cell_size));

					auto nearby_coords_min = glm::ivec3(glm::floor((boid.pos - cfg.nearby_dist) / cfg.cell_size));
					auto nearby_coords_max = glm::ivec3(glm::floor((boid.pos + cfg.nearby_dist) / cfg.cell_size));
					for (int a = nearby_coords_min.x; a <= nearby_coords_max.x; a++) {
						for (int b = nearby_coords_min.y; b <= nearby_coords_max.y; b++) {
							for (int c = nearby_coords_min.z; c <= nearby_coords_max.z; c++) {
								auto coord = glm::ivec3(a, b, c);
								glm::vec3 cell_dist = glm::max(
									glm::abs(glm::vec3(coord) - boid.pos), 
									glm::abs(glm::vec3(coord + 1) - boid.pos));
								if (glm::length2(cell_dist) > nearby_dist_sq) {
									continue;
								}
								auto it = cell_map.find(coord);
								if (it == cell_map.end()) continue;
								for (int j : it->second.boid_indices) {
									if (j == i) continue;
									float dist_sq = glm::length2(boid.pos - boids[j].pos);
									if (dist_sq < avoid_dist_sq) {
										boid.nearby_boids.push_back(j);
									}					
								}
							}
						}
					}

					auto avoid_coords_min = glm::ivec3(glm::floor((boid.pos - cfg.avoid_dist) / cfg.cell_size));
					auto avoid_coords_max = glm::ivec3(glm::floor((boid.pos + cfg.avoid_dist) / cfg.cell_size));
					for (int a = avoid_coords_min.x; a <= avoid_coords_max.x; a++) {
						for (int b = avoid_coords_min.y; b <= avoid_coords_max.y; b++) {
							for (int c = avoid_coords_min.z; c <= avoid_coords_max.z; c++) {
								auto coord = glm::ivec3(a, b, c);
								glm::vec3 cell_dist = glm::max(
									glm::abs(glm::vec3(coord) - boid.pos), 
									glm::abs(glm::vec3(coord + 1) - boid.pos));
								if (glm::length2(cell_dist) > avoid_dist_sq) {
									continue;
								}
								auto it = cell_map.find(coord);
								if (it == cell_map.end()) continue;
								for (int j : it->second.boid_indices) {
									if (j == i) continue;
									float dist_sq = glm::length2(boid.pos - boids[j].pos);
									if (dist_sq < avoid_dist_sq) {
										boid.avoid_boids.push_back(j);
									}					
								}
							}
						}
					}
				}
			}, thread_pool);
		}
	#endif
	}

	{
		ZoneScopedN("BoidApplyForces");
		for (int i = 0; i < num_boids; i++) {
			auto& boid = boids[i];

			if (!boid.nearby_boids.empty()) {
				// adjust velocity towards the average pos/vel of the other nearby birds
				auto com = glm::vec3(0);
				auto avg_vel = glm::vec3(0);
				const int num_nearby_boids = boid.nearby_boids.ssize();
				for (int j = 0; j < num_nearby_boids; j++) {
					auto& other_boid = boids[boid.nearby_boids[j]];
					com += other_boid.pos;
					avg_vel += other_boid.vel;
				}
				com /= num_nearby_boids;
				avg_vel /= num_nearby_boids;
				boid.vel += (cfg.pos_match_factor * (com - boid.pos) + cfg.vel_match_factor * (avg_vel - boid.vel));
			}

			if (!boid.avoid_boids.empty()) {
				// move away from other boids that are too close
				for (int j = 0; j < boid.avoid_boids.ssize(); j++) {
					auto& other_boid = boids[boid.avoid_boids[j]];
					glm::vec3 dx = boid.pos - other_boid.pos;
					float dist = glm::length(dx);
					float dl = cfg.avoid_dist - dist;
					boid.vel += (cfg.avoid_factor * dl / dist) * dx;
				}
			}

			boid.vel += cfg.target_follow_factor * (target_pos - boid.pos);

			float cur_vel_sq = glm::length2(boid.vel);
			if (cur_vel_sq > cfg.vel_limit * cfg.vel_limit) {
				boid.vel *= (cfg.vel_limit / glm::sqrt(cur_vel_sq));
			}

			boid.pos += dt * boid.vel;
		}
			
	}

	{
		ZoneScopedN("BoidApplyTransforms");
		ecs->query<Boid, Transform>().foreach([&](Entity entity, Boid& boid, Transform& transform) {
			auto dir = glm::normalize(boid.vel);
			auto q_target = glm::rotation(glm::vec3(0, 1, 0), dir);

			// TODO: lerp orientation so that it turns around slowly and more naturally
			auto q_current = transform.rotation;
			auto q_diff = glm::conjugate(q_current) * q_target;
			auto w = quat_log(q_diff) / dt;
			auto w_len = glm::length(w);
			if (w_len > 1e-6) {
				if (w_len > cfg.angvel_limit) {
					w = cfg.angvel_limit * (w / w_len * dt);
					transform.rotation = q_current * quat_exp(w);
				}
				else {
					transform.rotation = q_target;
				}
			}
			transform.translation = boid.pos;
		});
	}
}