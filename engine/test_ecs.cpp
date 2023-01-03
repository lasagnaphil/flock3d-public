#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"

struct A {
	uint32_t x, y;
};

struct B {
	uint32_t z;
};

struct C {
	uint32_t w;
};

struct D {
	float f;
};

#define FOR_LIST_OF_COMPONENTS(X) \
    X(A) \
    X(B) \
    X(C) \
    X(D) \

#include "ecs.h"

TEST_CASE("ECS basic test") {
	ECS ecs;

	Entity e1 = ecs.add_entity();
	A& e1_a = ecs.add_component<A>(e1);
	B& e1_b = ecs.add_component<B>(e1);

	Entity e2 = ecs.add_entity();
	A& e2_a = ecs.add_component<A>(e2);
	C& e2_c = ecs.add_component<C>(e2);

	Entity e3 = ecs.add_entity();
	A& e3_a = ecs.add_component<A>(e3);
	D& e3_c = ecs.add_component<D>(e3);

	Entity e4 = ecs.add_entity();
	B& e4_b = ecs.add_component<B>(e4);
	C& e4_c = ecs.add_component<C>(e4);
	D& e4_d = ecs.add_component<D>(e4);

	Entity e5 = ecs.add_entity();
	A& e5_a = ecs.add_component<A>(e5);
	B& e5_b = ecs.add_component<B>(e5);
	C& e5_c = ecs.add_component<C>(e5);
	D& e5_d = ecs.add_component<D>(e5);

	CHECK(ecs.get_component_array<A>().size() == 4);
	CHECK(ecs.get_component_array<B>().size() == 3);
	CHECK(ecs.get_component_array<C>().size() == 3);
	CHECK(ecs.get_component_array<D>().size() == 3);
}