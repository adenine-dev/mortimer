//! Broadly adapted from
//! https://github.com/syoyo/tinyobjloader-c/blob/master/examples/viewer/viewer.c

#include <assert.h>
#define TINYOBJ_LOADER_C_IMPLEMENTATION
#include "tinyobj_loader_c.h"

#include "loader.h"
#include "log.h"

#include "ccVector.h"

#include <math.h>

#ifdef _WIN64
#define atoll(S) _atoi64(S)
#include <windows.h>
#else
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#endif

static char *mmap_file(size_t *len, const char *filename) {
#ifdef _WIN64
  HANDLE file =
      CreateFileA(filename, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING,
                  FILE_ATTRIBUTE_NORMAL | FILE_FLAG_SEQUENTIAL_SCAN, NULL);

  if (file == INVALID_HANDLE_VALUE) { /* E.g. Model may not have materials. */
    return NULL;
  }

  HANDLE fileMapping = CreateFileMapping(file, NULL, PAGE_READONLY, 0, 0, NULL);
  assert(fileMapping != INVALID_HANDLE_VALUE);

  LPVOID fileMapView = MapViewOfFile(fileMapping, FILE_MAP_READ, 0, 0, 0);
  char *fileMapViewChar = (char *)fileMapView;
  assert(fileMapView != NULL);

  DWORD file_size = GetFileSize(file, NULL);
  (*len) = (size_t)file_size;

  return fileMapViewChar;
#else

  struct stat sb;
  char *p;
  int fd;

  fd = open(filename, O_RDONLY);
  if (fd == -1) {
    perror("open");
    return NULL;
  }

  if (fstat(fd, &sb) == -1) {
    perror("fstat");
    return NULL;
  }

  if (!S_ISREG(sb.st_mode)) {
    fprintf(stderr, "%s is not a file\n", filename);
    return NULL;
  }

  p = (char *)mmap(0, sb.st_size, PROT_READ, MAP_SHARED, fd, 0);

  if (p == MAP_FAILED) {
    perror("mmap");
    return NULL;
  }

  if (close(fd) == -1) {
    perror("close");
    return NULL;
  }

  (*len) = sb.st_size;

  return p;

#endif
}

static void get_file_data(void *ctx, const char *filename, const int is_mtl,
                          const char *obj_filename, char **data, size_t *len) {
  // NOTE: If you allocate the buffer with malloc(),
  // You can define your own memory management struct and pass it through `ctx`
  // to store the pointer and free memories at clean up stage(when you quit an
  // app)
  // This example uses mmap(), so no free() required.
  (void)ctx;

  if (!filename) {
    fprintf(stderr, "null filename\n");
    (*data) = NULL;
    (*len) = 0;
    return;
  }

  size_t data_len = 0;

  *data = mmap_file(&data_len, filename);
  (*len) = data_len;
}

ObjMesh load_obj(const char *filename) {
  tinyobj_attrib_t attrib;
  tinyobj_shape_t *shapes = NULL;
  usize shape_count;
  tinyobj_material_t *materials = NULL;
  usize material_count;
  unsigned int flags = TINYOBJ_FLAG_TRIANGULATE;
  const int ret =
      tinyobj_parse_obj(&attrib, &shapes, &shape_count, &materials,
                        &material_count, filename, get_file_data, NULL, flags);

  if (ret != TINYOBJ_SUCCESS) {
    fatalln("failed to load `%s`", filename);
  }

  ObjMesh mesh = {0};
  {
    mesh.index_count = attrib.num_face_num_verts * 3;
    mesh.vertex_count = attrib.num_vertices;

    mesh.positions = malloc(sizeof(vec3) * mesh.vertex_count);
    mesh.normals = malloc(sizeof(vec3) * mesh.vertex_count);
    mesh.indicies = malloc(sizeof(u32) * mesh.index_count);

    for (u32 i = 0; i < mesh.vertex_count; i++) {
      mesh.positions[i] =
          vec3New(attrib.vertices[i * 3 + 0], attrib.vertices[i * 3 + 1],
                  attrib.vertices[i * 3 + 2]);
    }

    usize face_offset = 0;
    for (unsigned int i = 0; i < attrib.num_face_num_verts; i++) {
      if (attrib.face_num_verts[i] % 3 != 0) {
        fatalln("non triangular face in obj file `%s`", filename);
      }

      for (int j = 0; j < attrib.face_num_verts[i] / 3; ++j) {
        tinyobj_vertex_index_t idxs[] = {
            attrib.faces[face_offset + 3 * j + 0],
            attrib.faces[face_offset + 3 * j + 1],
            attrib.faces[face_offset + 3 * j + 2],
        };
        for (u32 k = 0; k < 3; ++k) {
          mesh.indicies[face_offset + k] = idxs[k].v_idx;
          mesh.normals[idxs[k].v_idx] =
              vec3Normalize(vec3New(attrib.normals[3 * idxs[k].vn_idx + 0],
                                    attrib.normals[3 * idxs[k].vn_idx + 1],
                                    attrib.normals[3 * idxs[k].vn_idx + 2]));
        }

        face_offset += 3;
      }
    }
  }
  //  else {
  //   fatalln("can't load obj");
  // }

  tinyobj_attrib_free(&attrib);
  tinyobj_shapes_free(shapes, shape_count);
  tinyobj_materials_free(materials, material_count);

  return mesh;
}

void destroy_obj(ObjMesh *mesh) {
  if (mesh->positions)
    free(mesh->positions);
  if (mesh->normals)
    free(mesh->normals);
  if (mesh->uvs)
    free(mesh->uvs);
  if (mesh->indicies)
    free(mesh->indicies);
}