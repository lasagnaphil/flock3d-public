#pragma once

#include "core/vector.h"
#include "core/map.h"
#include "core/storage.h"

#include "render/renderer.h"

class Renderer;
class TexturedMesh;
class Texture;

class ModelLoader {
private:
	Renderer* _renderer;
	Map<std::string, Model> _models;

public:
  ModelLoader(Renderer *renderer);

  void load(const char *path);
  void unload();

  const Model &get_model(const std::string &name) const { return _models.at(name); }
	Model& get_model(const std::string& name) { return _models.at(name); }
};
