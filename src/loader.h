#pragma once

#include "types.h"
#include <ccVector.h>

typedef struct {
  usize vertex_count;
  usize index_count;
  vec3 *positions;
  vec3 *normals;
  vec2 *uvs;
  u32 *indicies;
} ObjMesh;

ObjMesh load_obj(const char *filename);

void destroy_obj(ObjMesh *);