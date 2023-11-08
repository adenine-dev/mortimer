#include <assert.h>
#include <stdlib.h>
#include <string.h>

#include "ccVector.h"
#include "log.h"
#include "maths.h"
#include "types.h"

#include "trimesh.h"

typedef struct {
  Aabb bounds;
  vec3 centroid;
  u32 index;
} TriangleInfo;

usize partition(void *arr, usize count, usize stride, void *context,
                bool (*compare)(void *, const void *)) {
  void *first = arr;
  for (usize i = 0; i < count; i++) {
    if (!compare(context, arr + i * stride)) {
      first = arr + i * stride;
      break;
    }
  }
  void *last = arr + stride * count;
  void *temp = malloc(stride);
  for (void *item = first + stride; item != last; item += stride) {
    if (compare(context, item)) {
      memmove(temp, item, stride);
      memmove(item, first, stride);
      memmove(first, temp, stride);
      first += stride;
    }
  }

  free(temp);
  return (first - arr) / stride;
}

typedef struct {
  u32 dim;
  f32 mid;
} PartitionTriangleInfoContext;

bool partition_triangle_infos_fn(void *ctx, const void *data) {
  PartitionTriangleInfoContext *info = ctx;
  const TriangleInfo *triangle_info = data;

  return triangle_info->centroid.v[info->dim] < info->mid;
}

int compare(void *ctx, const void *a, const void *b) {
  PartitionTriangleInfoContext *info = ctx;
  const TriangleInfo *tri_a = a;
  const TriangleInfo *tri_b = b;
  return tri_a->centroid.v[info->dim] < tri_b->centroid.v[info->dim];
}

u32 recursive_split(u32 triangle_count, TriangleInfo *triangle_infos,
                    u32 node_count, u32 *node_idx, BvhNode *nodes,
                    const u32 start, const u32 end) {
  assert(start != end);

  Aabb bounds = triangle_infos[start].bounds;
  for (u32 i = start + 1; i < end; i++) {
    bounds = aabb_union(bounds, triangle_infos[i].bounds);
  }

  u32 n_primitives = end - start;

  if (n_primitives == 1) {
    nodes[(*node_idx)++] = (BvhNode){
        .min = triangle_infos[start].bounds.min,
        .l = triangle_infos[start].index,
        .max = triangle_infos[start].bounds.max,
        .r = triangle_infos[start].index,
    };
  } else {
    Aabb centroid_bounds = aabb_from_vec3(triangle_infos[start].centroid);
    for (u32 i = start + 1; i < end; i++) {
      centroid_bounds =
          aabb_expand(centroid_bounds, triangle_infos[i].centroid);
    }

    // NOTE: this just splits in the middle, ideally we'd be using SAH but
    // that's really complicated for now lol
    PartitionTriangleInfoContext ctx = {
        .dim = aabb_max_extent_idx(centroid_bounds),
        .mid =
            (centroid_bounds.min.v[ctx.dim] + centroid_bounds.max.v[ctx.dim]) *
            0.5,
    };

    u32 middle = start + n_primitives / 2;
    partition(triangle_infos + start, n_primitives, sizeof(TriangleInfo), &ctx,
              partition_triangle_infos_fn);

    u32 a = recursive_split(triangle_count, triangle_infos, node_count,
                            node_idx, nodes, start, middle);
    u32 b = recursive_split(triangle_count, triangle_infos, node_count,
                            node_idx, nodes, middle, end);

    nodes[(*node_idx)++] = (BvhNode){
        .max = bounds.max,
        .min = bounds.min,
        .l = a,
        .r = b,
    };
  }

  return *node_idx - 1;
}

TriangleMesh trimesh_new(Vertex *vertices, usize vertex_count, u32 *indices,
                         u32 index_count) {
  usize triangle_count = index_count / 3;
  u32 bvh_node_count = (triangle_count * 2 - 1);

  TriangleMesh self = {
      .index_count = index_count,
      .indices = indices,
      .vertex_count = vertex_count,
      .vertices = vertices,
      .bvh_node_count = bvh_node_count,
      .bvh_nodes = malloc(sizeof(BvhNode) * bvh_node_count),
  };

  TriangleInfo *triangle_infos = malloc(sizeof(TriangleInfo) * triangle_count);

  for (u32 i = 0; i < triangle_count; ++i) {
    Vertex v0 = vertices[indices[(i * 3) + 0]];
    Vertex v1 = vertices[indices[(i * 3) + 1]];
    Vertex v2 = vertices[indices[(i * 3) + 2]];

    Aabb bounds = aabb_new(
        vec3Subtract(vec3Min(v0.position, vec3Min(v1.position, v2.position)),
                     vec3New(1e-5, 1e-5, 1e-5)),
        vec3Add(vec3Max(v0.position, vec3Max(v1.position, v2.position)),
                vec3New(1e-5, 1e-5, 1e-5)));

    triangle_infos[i] = (TriangleInfo){
        .index = i,
        .centroid = aabb_centroid(bounds),
        .bounds = bounds,
    };
  }

  u32 node_idx = 0;
  recursive_split(triangle_count, triangle_infos, bvh_node_count, &node_idx,
                  self.bvh_nodes, 0, triangle_count);

  free(triangle_infos);

  return self;
}

void trimesh_destroy(TriangleMesh self) {
  free(self.bvh_nodes);
  free(self.vertices);
  free(self.indices);
}