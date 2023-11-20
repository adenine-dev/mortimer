#include <stdlib.h>

#include "envlight.h"
#include "loader.h"
#include "log.h"
#include "scene.h"
#include "trimesh.h"

Scene scene_new() {
  return (Scene){
      .object_count = 0,
      .materials = NULL,
      .meshes = NULL,
      .envlight = envlight_new_blank_sky(),
  };
}

void scene_add_object(Scene *self, const char *path, Material material) {
  ObjMesh obj = load_obj(path);
  self->materials =
      realloc(self->materials, (self->object_count + 1) * sizeof(Material));
  self->materials[self->object_count] = material;

  self->meshes =
      realloc(self->meshes, (self->object_count + 1) * sizeof(ObjMesh));
  self->meshes[self->object_count] = obj;

  ++self->object_count;
}

void scene_set_envlight(Scene *self, const char *path) {
  envlight_destroy(&self->envlight);
  self->envlight = envlight_new_from_file(path);
}

TriangleMesh scene_create_unified_mesh(Scene *self) {
  u32 total_vertices = 0;
  u32 total_indices = 0;
  for (u32 i = 0; i < self->object_count; ++i) {
    total_indices += self->meshes[i].index_count;
    total_vertices += self->meshes[i].vertex_count;
  }

  Vertex *vertices = malloc(sizeof(Vertex) * total_vertices);
  u32 *indices = malloc(sizeof(u32) * total_indices);

  u32 vertex_offset = 0;
  u32 index_offset = 0;
  for (u32 i = 0; i < self->object_count; ++i) {
    for (usize j = 0; j < self->meshes[i].index_count; j++) {
      indices[index_offset++] = self->meshes[i].indicies[j] + vertex_offset;
    }
    for (usize j = 0; j < self->meshes[i].vertex_count; j++) {
      vertices[vertex_offset++] = (Vertex){
          .position = self->meshes[i].positions[j],
          .object_index = i,
          .normal = self->meshes[i].normals[j],
      };
    }
  }

  return trimesh_new(vertices, total_vertices, indices, total_indices);
}

void scene_destroy(Scene *self) {
  for (u32 i = 0; i < self->object_count; ++i) {
    destroy_obj(&self->meshes[i]);
  }

  free(self->materials);
  free(self->meshes);
  envlight_destroy(&self->envlight);
}