#include <assert.h>
#include <math.h>
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

typedef enum {
  SPLIT_METHOD_EQUAL_COUNTS,
  SPLIT_METHOD_SAH,
} SplitMethod;

typedef struct {
  u32 dim;
  f32 mid;
} PartitionTriangleInfoByCentroidContext;

bool partition_triangle_infos_by_centroid_fn(void *ctx, const void *data) {
  PartitionTriangleInfoByCentroidContext *info = ctx;
  const TriangleInfo *triangle_info = data;

  return triangle_info->centroid.v[info->dim] < info->mid;
}

typedef struct {
  u32 dim;
  u32 min_idx;
  Aabb centroid_bounds;
} PartitionTriangleInfoByBucketContext;

const u32 BUCKET_COUNT = 12;
typedef struct {
  u32 cost;
  Aabb bounds;
} Bucket;

u32 get_bucket(Aabb centroid_bounds, const TriangleInfo *info, u32 dim) {
  return min(BUCKET_COUNT - 1,
      (u32)(((f32)BUCKET_COUNT *
             aabb_offset(centroid_bounds, info->centroid).v[dim]))));
}

bool partition_triangle_infos_by_bucket_fn(void *ctx, const void *data) {
  PartitionTriangleInfoByBucketContext *info = ctx;
  const TriangleInfo *triangle_info = data;
  u32 b = get_bucket(info->centroid_bounds, triangle_info, info->dim);
  assert(b < BUCKET_COUNT);
  return b <= info->min_idx;
}

u32 recursive_split(u32 triangle_count, TriangleInfo *triangle_infos,
                    u32 node_count, u32 *node_idx, BvhNode *nodes,
                    const u32 start, const u32 end) {
  assert(start < end);

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
    u32 dim = aabb_max_extent_idx(centroid_bounds);

    u32 middle;
    SplitMethod split_method = SPLIT_METHOD_SAH;
    if (n_primitives == 2) {
      split_method = SPLIT_METHOD_EQUAL_COUNTS;
    }
    switch (split_method) {
    case SPLIT_METHOD_SAH: {
      Bucket buckets[BUCKET_COUNT] = {0};

      for (u32 i = start; i < end; ++i) {
        u32 b = get_bucket(centroid_bounds, &triangle_infos[i], dim);
        buckets[b].bounds =
            aabb_union(buckets[b].bounds, triangle_infos[i].bounds);
        ++buckets[b].cost;
      }

      f32 costs[BUCKET_COUNT - 1] = {0};

      for (u32 i = 0; i < BUCKET_COUNT - 1; ++i) {
        Aabb b0 = buckets[0].bounds;
        Aabb b1 = buckets[i + 1].bounds;
        u32 c0 = buckets[0].cost;
        u32 c1 = buckets[i + 1].cost;

        for (u32 j = 1; j < i + 1; j++) {
          b0 = aabb_union(b0, buckets[j].bounds);
          c0 += buckets[j].cost;
        }

        for (u32 j = i + 1 + 1; j < BUCKET_COUNT; j++) {
          b1 = aabb_union(b1, buckets[j].bounds);
          c1 += buckets[j].cost;
        }

        costs[i] = 1.0 + ((f32)c0 * aabb_surface_area(b0) +
                          (f32)c1 * aabb_surface_area(b1)) /
                             aabb_surface_area(bounds);
      }

      f32 min_cost = costs[0];
      u32 min_idx = 0;
      for (u32 i = 1; i < BUCKET_COUNT - 1; ++i) {
        if (min_cost > costs[i]) {
          min_cost = costs[i];
          min_idx = i;
        }
      }

      PartitionTriangleInfoByBucketContext ctx = {
          .centroid_bounds = centroid_bounds,
          .dim = dim,
          .min_idx = min_idx,
      };
      middle = start + partition(triangle_infos + start, n_primitives,
                                 sizeof(TriangleInfo), &ctx,
                                 partition_triangle_infos_by_bucket_fn);
      if (middle != start) {
        break;
      }
    }
    case SPLIT_METHOD_EQUAL_COUNTS: {
      PartitionTriangleInfoByCentroidContext ctx = {
          .dim = dim,
          .mid = (centroid_bounds.min.v[ctx.dim] +
                  centroid_bounds.max.v[ctx.dim]) *
                 0.5,
      };

      middle = start + n_primitives / 2;
      partition(triangle_infos + start, n_primitives, sizeof(TriangleInfo),
                &ctx, partition_triangle_infos_by_centroid_fn);
    } break;
    default: {
      fatalln("something has gone terribly wrong");
    }
    }

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