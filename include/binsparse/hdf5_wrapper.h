/*
 * SPDX-FileCopyrightText: 2024 Binsparse Developers
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once

#include <assert.h>
#include <binsparse/array.h>
#include <hdf5.h>
#include <string.h>

#include <unistd.h>

#include <binsparse/detail/hdf5_types.h>
#include <binsparse/detail/shm_tools.h>

#if __STDC_VERSION__ >= 201112L
#include <stdatomic.h>
#endif

// Write an array to a dataset / file
// Returns 0 on success, nonzero on error.
static inline bsp_error_t bsp_write_array(hid_t f, const char* label,
                                          bsp_array_t array,
                                          int compression_level) {
  if (array.type == BSP_COMPLEX_FLOAT32 || array.type == BSP_COMPLEX_FLOAT64) {
    bsp_error_t error = bsp_complex_array_to_fp(&array);
    if (error != BSP_SUCCESS) {
      return BSP_ERROR_TYPE;
    }
  }

  hsize_t hsize[1];

  hsize[0] = array.size;

  hid_t hdf5_standard_type = bsp_get_hdf5_standard_type(array.type);
  hid_t fspace = H5Screate_simple(1, hsize, NULL);
  hid_t lcpl = H5Pcreate(H5P_LINK_CREATE);

  hid_t dcpl = H5Pcreate(H5P_DATASET_CREATE);

  if (array.size > 0) {
    // Choose 1 MiB, the default chunk cache size, as our chunk size.
    size_t chunk_size = 1024 * 1024 / bsp_type_size(array.type);

    // If the dataset is smaller than the chunk size, cap the chunk size.
    if (array.size < chunk_size) {
      chunk_size = array.size;
    }

    hsize[0] = chunk_size;
    H5Pset_chunk(dcpl, 1, hsize);

    if (compression_level > 0) {
      H5Pset_deflate(dcpl, compression_level);
    }
  }

  hid_t dset =
      H5Dcreate2(f, label, hdf5_standard_type, fspace, lcpl, dcpl, H5P_DEFAULT);

  if (dset == H5I_INVALID_HID) {
    H5Sclose(fspace);
    H5Pclose(lcpl);
    H5Pclose(dcpl);
    return BSP_ERROR_IO;
  }

  if (array.size > 0) {
    hid_t hdf5_native_type = bsp_get_hdf5_native_type(array.type);
    hid_t r = H5Dwrite(dset, hdf5_native_type, H5S_ALL, fspace, H5P_DEFAULT,
                       array.data);

    if (r < 0) {
      H5Dclose(dset);
      H5Sclose(fspace);
      H5Pclose(lcpl);
      H5Pclose(dcpl);
      return BSP_ERROR_IO;
    }
  }

  H5Sclose(fspace);
  H5Dclose(dset);
  H5Pclose(lcpl);
  H5Pclose(dcpl);

  return BSP_SUCCESS;
}

#if __STDC_VERSION__ >= 201112L
static inline bsp_error_t bsp_read_array_parallel(bsp_array_t* array, hid_t f,
                                                  const char* label,
                                                  int num_threads) {
  hid_t dset = H5Dopen2(f, label, H5P_DEFAULT);

  if (dset == H5I_INVALID_HID) {
    H5Dclose(dset);
    bsp_construct_default_array_t(array);
    return BSP_ERROR_IO;
  }

  hid_t fspace = H5Dget_space(dset);

  if (fspace == H5I_INVALID_HID) {
    H5Sclose(fspace);
    H5Dclose(dset);
    bsp_construct_default_array_t(array);
    return BSP_ERROR_IO;
  }

  hsize_t dims[3];

  int r = H5Sget_simple_extent_dims(fspace, dims, NULL);

  if (r < 0) {
    H5Sclose(fspace);
    H5Dclose(dset);
    bsp_construct_default_array_t(array);
    return BSP_ERROR_IO;
  }

  hid_t hdf5_type = H5Dget_type(dset);

  bsp_type_t type = bsp_get_bsp_type(hdf5_type);

  // Array will be written into a POSIX shared memory.
  bsp_shm_t array_shm = bsp_shm_new(dims[0] * bsp_type_size(type));
  array->type = type;
  array->size = dims[0];
  array->allocator = bsp_shm_allocator;

  bsp_shm_t active_children_shm = bsp_shm_new(sizeof(_Atomic int));

  _Atomic int* active_children = bsp_shm_attach(active_children_shm);
  bsp_shm_delete(active_children_shm);

  *active_children = num_threads - 1;

  pid_t* pids = (pid_t*) malloc(sizeof(pid_t) * num_threads);

  int thread_num = 0;

  for (size_t i = 0; i < num_threads - 1; i++) {
    pid_t new_pid = fork();

    if (new_pid != 0) {
      pids[i] = new_pid;
    } else {
      thread_num = i + 1;
      break;
    }
  }

  array->data = bsp_shm_attach(array_shm);
  if (thread_num == 0) {
    bsp_shm_delete(array_shm);
  }

  hsize_t chunk_size = (array->size + num_threads - 1) / num_threads;
  hsize_t start = (chunk_size * thread_num < array->size)
                      ? chunk_size * thread_num
                      : array->size;
  hsize_t count =
      (start + chunk_size <= array->size) ? chunk_size : array->size - start;

  if (count > 0) {
    H5Sselect_hyperslab(fspace, H5S_SELECT_SET, &start, NULL, &count, NULL);

    hid_t memspace_id = H5Screate_simple(1, &count, NULL);

    H5Dread(dset, bsp_get_hdf5_native_type(type), memspace_id, fspace,
            H5P_DEFAULT, ((char*) array->data) + start * bsp_type_size(type));
    H5Sclose(memspace_id);
  }

  H5Sclose(fspace);
  H5Dclose(dset);

  if (thread_num > 0) {
    atomic_fetch_add_explicit(active_children, -1, memory_order_relaxed);
    bsp_shm_detach(active_children);
    bsp_shm_detach(array->data);
    exit(0);
  }

  free(pids);

  while (atomic_load(active_children) > 0) {
  }
  bsp_shm_detach(active_children);

  return BSP_SUCCESS;
}
#endif

static inline bsp_error_t bsp_read_array_allocator(bsp_array_t* array, hid_t f,
                                                   const char* label,
                                                   bsp_allocator_t allocator) {
  hid_t dset = H5Dopen2(f, label, H5P_DEFAULT);

  if (dset == H5I_INVALID_HID) {
    bsp_construct_default_array_t_allocator(array, allocator);
    return BSP_ERROR_IO;
  }

  hid_t fspace = H5Dget_space(dset);

  if (fspace == H5I_INVALID_HID) {
    H5Sclose(fspace);
    H5Dclose(dset);
    bsp_construct_default_array_t_allocator(array, allocator);
    return BSP_ERROR_IO;
  }

  hsize_t dims[3];

  int r = H5Sget_simple_extent_dims(fspace, dims, NULL);

  if (r < 0) {
    H5Dclose(dset);
    H5Sclose(fspace);
    bsp_construct_default_array_t_allocator(array, allocator);
    return BSP_ERROR_IO;
  }

  hid_t hdf5_type = H5Dget_type(dset);

  bsp_type_t type = bsp_get_bsp_type(hdf5_type);

  bsp_error_t error =
      bsp_construct_array_t_allocator(array, dims[0], type, allocator);
  if (error != BSP_SUCCESS) {
    H5Dclose(dset);
    H5Sclose(fspace);
    bsp_construct_default_array_t_allocator(array, allocator);
    return BSP_ERROR_MEMORY;
  }

  hid_t native_type = bsp_get_hdf5_native_type(type);
  herr_t status =
      H5Dread(dset, native_type, H5S_ALL, H5S_ALL, H5P_DEFAULT, array->data);
  H5Tclose(hdf5_type);

  if (status < 0) {
    bsp_destroy_array_t(array);
    H5Sclose(fspace);
    H5Dclose(dset);
    bsp_construct_default_array_t_allocator(array, allocator);
    return BSP_ERROR_IO;
  }

  H5Sclose(fspace);
  H5Dclose(dset);
  return BSP_SUCCESS;
}

static inline bsp_error_t bsp_read_array(bsp_array_t* array, hid_t f,
                                         const char* label) {
  return bsp_read_array_allocator(array, f, label, bsp_default_allocator);
}

static inline bsp_error_t bsp_write_attribute(hid_t f, const char* label,
                                              const char* string) {
  // Variable-length, like the string datasets Binsparse writes.  High-level
  // bindings expose a fixed-size string attribute as raw bytes rather than as
  // a string.
  hid_t strtype = H5Tcopy(H5T_C_S1);
  H5Tset_size(strtype, H5T_VARIABLE);
  H5Tset_cset(strtype, H5T_CSET_UTF8);
  hid_t dataspace = H5Screate(H5S_SCALAR);

  hid_t attribute =
      H5Acreate2(f, label, strtype, dataspace, H5P_DEFAULT, H5P_DEFAULT);

  // A variable-length write takes the address of the pointer, not the pointer.
  H5Awrite(attribute, strtype, &string);

  H5Tclose(strtype);
  H5Aclose(attribute);
  H5Sclose(dataspace);

  return BSP_SUCCESS;
}

static inline bsp_error_t
bsp_read_attribute_allocator(char** string, hid_t f, const char* label,
                             bsp_allocator_t allocator) {
  hid_t attribute = H5Aopen(f, label, H5P_DEFAULT);

  if (attribute == H5I_INVALID_HID) {
    return BSP_ERROR_FORMAT;
  }

  hid_t strtype = H5Aget_type(attribute);

  // Older files store this attribute as a fixed-size string, handled below.
  // A variable-length one has to be read through a pointer HDF5 allocates,
  // since H5Tget_size would just give the size of that pointer.
  if (H5Tis_variable_str(strtype) > 0) {
    char* buffer = NULL;

    if (H5Aread(attribute, strtype, &buffer) < 0 || buffer == NULL) {
      H5Aclose(attribute);
      H5Tclose(strtype);
      return BSP_ERROR_IO;
    }

    size_t size = strlen(buffer);
    *string = (char*) allocator.malloc(size + 1);
    memcpy(*string, buffer, size + 1);

    // Return HDF5's copy to HDF5; it did not come from `allocator`.
    hid_t space = H5Aget_space(attribute);
    H5Dvlen_reclaim(strtype, space, H5P_DEFAULT, &buffer);
    H5Sclose(space);

    H5Aclose(attribute);
    H5Tclose(strtype);

    return BSP_SUCCESS;
  }

  size_t size = H5Tget_size(strtype);

  *string = (char*) allocator.malloc(size + 1);

  H5Aread(attribute, strtype, *string);
  (*string)[size] = '\0';

  H5Aclose(attribute);
  H5Tclose(strtype);

  return BSP_SUCCESS;
}

static inline bsp_error_t bsp_read_attribute(char** string, hid_t f,
                                             const char* label) {
  return bsp_read_attribute_allocator(string, f, label, bsp_default_allocator);
}
