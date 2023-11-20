#pragma once

#include "maths.h"
#include "types.h"

/// `position` and `object_index` are translated as a vec4 on the gpu
typedef struct {
  vec3 position;
  u32 object_index;
  vec3 normal;
  u32 _pad1;
} Vertex;

/// This is a weird encoding for sending to the gpu, this becomes a tightly
/// packed 256 bit bvh node. `min` and `max` are aabb points, `l` and `r` are
/// indicies into the child nodes, unless `l` and `r` are equal, then the node
/// is a leaf and `l` and `r` is an index into the index buffer that corresponds
/// to the triangle bounded by this aabb.
typedef struct {
  vec3 min;
  u32 l;
  vec3 max;
  u32 r;
} BvhNode;

typedef struct {
  u32 vertex_count;
  Vertex *vertices;
  u32 index_count;
  u32 *indices;
  u32 bvh_node_count;
  BvhNode *bvh_nodes;
} TriangleMesh;

TriangleMesh trimesh_new(Vertex *vertices, usize vertex_count, u32 *indices,
                         u32 index_count);
void trimesh_destroy(TriangleMesh self);