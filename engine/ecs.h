#pragma once

#include "core/alloc.h"
#include "core/log.h"
#include "core/span.h"
#include "core/meta.h"
#include "core/array.h"

#ifndef OVERRIDE_ECS_COMPONENTS
#include "components.h"
#endif

enum ComponentType : uint32_t {
    #define CREATE_ENUM_ENTRY(Type) COMP_##Type,

    FOR_LIST_OF_COMPONENTS(CREATE_ENUM_ENTRY)

    #undef CREATE_ENUM_ENTRY

    MAX_COMPONENTS
};

using namespace std::string_view_literals;

constexpr Array g_component_names = {
    #define CREATE_STRING(Type) #Type##sv,
    FOR_LIST_OF_COMPONENTS(CREATE_STRING)
    #undef CREATE_STRING
};

template <class T> constexpr ComponentType get_component_enum();

constexpr std::string_view get_component_name(ComponentType ctype) {
    return g_component_names[ctype];
}

template <class T>
constexpr std::string_view get_component_name() { return get_component_name(get_component_enum<T>()); }

#define CREATE_GET_COMPONENT_ENUM(Type) \
template <> constexpr ComponentType get_component_enum<Type>() { return COMP_##Type; }

FOR_LIST_OF_COMPONENTS(CREATE_GET_COMPONENT_ENUM)

#undef CREATE_GET_COMPONENT_ENUM

constexpr Array g_component_type_sizes = {
    #define CREATE_SIZEOF(Type) sizeof(Type),
    FOR_LIST_OF_COMPONENTS(CREATE_SIZEOF)
    #undef CREATE_SIZEOF
};

constexpr Array g_component_type_aligns = {
    #define CREATE_ALIGNOF(Type) alignof(Type),
    FOR_LIST_OF_COMPONENTS(CREATE_ALIGNOF)
    #undef CREATE_ALIGNOF
};

constexpr uint32_t get_component_type_size(uint32_t cid) {
    return g_component_type_sizes[cid];
}

constexpr uint32_t get_component_type_align(uint32_t cid) {
    return g_component_type_aligns[cid];
}
struct Entity {
	uint32_t index;
	uint32_t generation;
};

struct ComponentStorage {
	void* dense;
	uint32_t* dense_to_sparse;
	uint32_t* sparse;

	uint32_t dense_size;
	uint32_t dense_capacity;

	uint32_t tsize;
	uint32_t talign;
};

class ECS {
private:
	static constexpr uint32_t NIL = 0xffffffff;

	Entity* _entity_sparse;
	uint32_t* _entity_dense_to_sparse;
	Array<ComponentStorage, MAX_COMPONENTS> _comp_storages;

	uint32_t _free_list_front;
	uint32_t _free_list_back;

	uint32_t _slot_size;
	uint32_t _slot_capacity;

	uint32_t _num_entities;

public:
	ECS(uint32_t _start_entity_capacity = 0) {
		setup(_start_entity_capacity);
	}

	void setup(uint32_t _start_entity_capacity) {
		_free_list_front = NIL;
		_free_list_back = NIL;

		_slot_size = 0;
		_slot_capacity = 0;

		_num_entities = 0;

		_entity_sparse = nullptr;
		_entity_dense_to_sparse = nullptr;

		for (uint32_t ctid = 0; ctid < MAX_COMPONENTS; ctid++) {
			auto& storage = _comp_storages[ctid];
			storage.dense_size = 0;
			storage.dense_capacity = 0;
			storage.tsize = get_component_type_size(ctid);
			storage.talign = get_component_type_align(ctid);
			storage.dense = nullptr;
			storage.dense_to_sparse = nullptr;
			storage.sparse = nullptr;
		}

		if (_start_entity_capacity != 0) {
			resize_sparse(_start_entity_capacity);
		}
	}

	void release() {
		_free_list_front = NIL;
		_free_list_back = NIL;

		_slot_size = 0;
		_slot_capacity = 0;

		_num_entities = 0;

		mem_free(_entity_sparse);
		mem_free(_entity_dense_to_sparse);
		_entity_sparse = nullptr;
		_entity_dense_to_sparse = nullptr;

		for (uint32_t ctid = 0; ctid < MAX_COMPONENTS; ctid++) {
			auto& storage = _comp_storages[ctid];
			mem_free(storage.dense);
			mem_free(storage.dense_to_sparse);
			mem_free(storage.sparse);
			storage.dense = nullptr;
			storage.dense_to_sparse = nullptr;
			storage.sparse = nullptr;
			storage.dense_size = 0;
			storage.dense_capacity = 0;
		}
	}

	Entity add_entity() {
		Entity entity;
		if (_free_list_front == NIL) {
			log_assert(_num_entities == _slot_size);
			entity = {_slot_size, 1};
			if (_slot_size == _slot_capacity) {
				resize_sparse(_slot_capacity == 0 ? 1 : 2 * _slot_capacity);
			}
			_entity_sparse[_slot_size] = entity;
			_slot_size++;
		}
		else {
			Entity& slot = _entity_sparse[_free_list_front];
			uint32_t new_index = _free_list_front;
			_free_list_front = slot.index;
			if (_free_list_front == NIL) {
				_free_list_back = NIL;
			}
			slot.index = _num_entities;
			entity = {new_index, slot.generation};
		}

		_entity_dense_to_sparse[_num_entities] = entity.index;
		_num_entities++;

		return entity;
	}

	template <class Component>
	Span<Component> get_component_array() {
		constexpr uint32_t ctid = (uint32_t)get_component_enum<Component>();
		auto& storage = _comp_storages[ctid];
		return {static_cast<Component*>(storage.dense), storage.dense_size};
	}

	template <class Component, class... Args>
	Component& add_component(Entity entity, Args&&... args) {
		constexpr uint32_t ctid = (uint32_t)get_component_enum<Component>();
		uint32_t eid = get_entity_index(entity);
		auto& storage = _comp_storages[ctid];
		if (storage.dense_size == storage.dense_capacity) {
			uint32_t new_dense_capacity = storage.dense_capacity == 0 ? 1 : 2 * storage.dense_capacity;

			void* new_dense = alloc_array<Component>(new_dense_capacity, 64);
			memcpy(new_dense, storage.dense, sizeof(Component) * storage.dense_capacity);
			mem_free(storage.dense);
			storage.dense = new_dense;

			uint32_t* new_dense_to_sparse = alloc_array<uint32_t>(new_dense_capacity, 64);
			memcpy(new_dense_to_sparse, storage.dense_to_sparse, sizeof(uint32_t) * storage.dense_capacity);
			mem_free(storage.dense_to_sparse);
			storage.dense_to_sparse = new_dense_to_sparse;

			storage.dense_capacity = new_dense_capacity;
		}
		Component* components = static_cast<Component*>(storage.dense);
		uint32_t cid = storage.dense_size;
		auto& component = components[cid];
		storage.sparse[eid] = cid;
		storage.dense_to_sparse[cid] = eid;
		storage.dense_size++;
		new (&component) Component(std::forward<Args>(args)...);
		return component;
	}

	template <class Component>
	void remove_component(Entity entity) {
		constexpr uint32_t ctid = (uint32_t)get_component_enum<Component>();
		uint32_t eid = get_entity_index(entity);
		auto& storage = _comp_storages[ctid];
		uint32_t cid = storage.sparse[eid];
		storage.dense_size--;
		Component* components = static_cast<Component*>(storage.dense);
		components[cid].~Component();
		memmove(components[cid], components[storage.dense_size], sizeof(Component));
		storage.dense_to_sparse[cid] = storage.dense_to_sparse[storage.dense_size];

		// TODO: Add shrinking of buffers when size < capacity / 2
	}

	template <class Component>
	const Component& get_component(Entity entity) const {
		constexpr uint32_t ctid = (uint32_t)get_component_enum<Component>();
		uint32_t eid = get_entity_index(entity);
		const auto& storage = _comp_storages[ctid];
		uint32_t cid = storage.sparse[eid];
		const Component* components = static_cast<const Component*>(storage.dense);
		return components[cid];
	}

	template <class Component>
	Component& get_component(Entity entity) {
		constexpr uint32_t ctid = (uint32_t)get_component_enum<Component>();
		uint32_t eid = get_entity_index(entity);
		auto& storage = _comp_storages[ctid];
		uint32_t cid = storage.sparse[eid];
		Component* components = static_cast<Component*>(storage.dense);
		return components[cid];
	}

	template <class... Component>
	class Query {
	private:
		ECS* _ecs;
	public:
		friend class ECS;

		Query(ECS* ecs) : _ecs(ecs) {}

		template <class Fun>
		void foreach(Fun&& fun) {
			constexpr uint32_t num_components = sizeof...(Component);
			static_assert(num_components >= 1, "Invalid usage of _ecs foreach: no components specified!");
			constexpr Array<uint32_t, num_components> ctids = {get_component_enum<Component>()...};
			uint32_t min_ctid = ctids[0];
			uint32_t min_ctid_size = _ecs->_comp_storages[min_ctid].dense_size;
			for (uint32_t i = 1; i < ctids.size(); i++) {
				uint32_t cur_ctid = ctids[i];
				uint32_t cur_ctid_size = _ecs->_comp_storages[cur_ctid].dense_size;
				if (min_ctid_size < cur_ctid_size) {
					min_ctid = cur_ctid;
					min_ctid_size = cur_ctid_size;
				}
			}
			auto& min_comp_storage = _ecs->_comp_storages[min_ctid];
			Array<void*, num_components> cur_compoments;
			for (uint32_t k = 0; k < num_components; k++) {
				cur_compoments[k] = nullptr;
			}
			for (uint32_t i = 0; i < min_comp_storage.dense_size; i++) {
				uint32_t eid = min_comp_storage.dense_to_sparse[i];
				uint32_t entity_gen = _ecs->_entity_sparse[_ecs->_entity_dense_to_sparse[eid]].generation;
				bool found = true;
				for (uint32_t k = 0; k < num_components; k++) {
					uint32_t ctid = ctids[k];
					if (ctid == min_ctid) {
						cur_compoments[k] = static_cast<char*>(min_comp_storage.dense) + min_comp_storage.tsize * i;
						continue;
					}
					auto& comp_storage = _ecs->_comp_storages[ctid];
					if (comp_storage.sparse == nullptr) {
						found = false; break;
					}
					uint32_t cid = comp_storage.sparse[eid];
					if (cid == NIL) {
						found = false; break;
					}
					cur_compoments[k] = static_cast<char*>(comp_storage.dense) + comp_storage.tsize * cid;
				}
				if (found) {
					Entity entity = {eid, entity_gen};
					call_foreach_fun(std::forward<Fun>(fun), entity, cur_compoments, index_range<0, num_components>());
				}
			}
		}

	private:
		template <class Fun, int... Is>
		void call_foreach_fun(Fun&& fun, Entity entity, const Array<void*, sizeof...(Component)>& comp_ptrs, index_list<Is...>) {
			fun(entity, *static_cast<Component*>(comp_ptrs[Is])...);
		}
	};

	template <class... Component>
	Query<Component...> query() {
		return Query<Component...>(this);
	}

	void check_entity(Entity entity) const {
		log_assert(entity.index < _slot_size);
		auto slot = _entity_sparse[entity.index];
		log_assert(slot.generation == entity.generation);
		log_assert(slot.index < _num_entities);
	}

	uint32_t get_entity_index(Entity entity) const {
		log_assert(entity.index < _slot_size);
		auto slot = _entity_sparse[entity.index];
		log_assert(slot.generation == entity.generation);
		log_assert(slot.index < _num_entities);
		return slot.index;
	}

	uint32_t num_entities() const { return _num_entities; }

private:
	void resize_sparse(uint32_t new_slot_capacity) {
		log_assert(new_slot_capacity > _slot_capacity);

		Entity* new_entity_sparse = alloc_array<Entity>(new_slot_capacity, 64);
		memcpy(new_entity_sparse, _entity_sparse, sizeof(Entity) * _slot_capacity);
		mem_free(_entity_sparse);
		_entity_sparse = new_entity_sparse;

		uint32_t* new_entity_dense_to_sparse = alloc_array<uint32_t>(new_slot_capacity, 64);
		memcpy(new_entity_dense_to_sparse, _entity_dense_to_sparse, sizeof(uint32_t) * _slot_capacity);
		mem_free(_entity_dense_to_sparse);
		_entity_dense_to_sparse = new_entity_dense_to_sparse;

		for (uint32_t i = 0; i < MAX_COMPONENTS; i++) {
			uint32_t* new_sparse = alloc_array<uint32_t>(new_slot_capacity, 64);
			memcpy(new_sparse, _comp_storages[i].sparse, sizeof(uint32_t) * _slot_capacity);
			for (uint32_t k = _slot_capacity; k < new_slot_capacity; k++) {
				new_sparse[k] = NIL;
			}
			mem_free(_comp_storages[i].sparse);
			_comp_storages[i].sparse = new_sparse;
		}

		_slot_capacity = new_slot_capacity;
	}


};
