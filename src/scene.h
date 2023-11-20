#pragma once

#include "ccVector.h"

#include "envlight.h"
#include "loader.h"
#include "trimesh.h"
#include "types.h"

typedef struct {
  vec3 albedo;
  u32 _pad0;
} Material;

typedef struct {
  u32 object_count;
  Material *materials;
  ObjMesh *meshes;

  EnvironmentLight envlight;
} Scene;

Scene scene_new();
void scene_add_object(Scene *self, const char *path, Material material);
void scene_set_envlight(Scene *self, const char *path);
TriangleMesh scene_create_unified_mesh(Scene *self);

void scene_destroy(Scene *self);