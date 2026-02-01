/*
miniz.h - minimal stub of miniz zip reader API used by ft2-dxm

This file provides a very small subset of the miniz API surface used in
ft2-dxm's Dexed integration. It is intentionally minimal: it implements
lightweight stubs that allow code to compile and will gracefully indicate
absence of zip support at runtime.

The real miniz (https://github.com/richgel999/miniz) provides a full-featured
zip reader. If you want builtin_pgm.zip extraction to work, replace this
stub with the upstream miniz single-file implementation (or add a real
zip extraction library).

This header exposes the following symbols used by ft2-dxm:
 - mz_zip_archive (opaque struct)
 - mz_zip_archive_file_stat (file metadata)
 - mz_uint (unsigned int)
 - mz_zip_reader_init_file(...)
 - mz_zip_reader_get_num_files(...)
 - mz_zip_reader_file_stat(...)
 - mz_zip_reader_extract_to_heap(...)
 - mz_free(...)
 - mz_zip_reader_end(...)

The stub implementation here will return "no entries" for ZIP files,
causing the wrapper to fallback to scanning Dexed_01.syx. Replace with
real miniz for full functionality.
*/

#include <stdlib.h>

#ifndef FT2_MINIZ_H
#define FT2_MINIZ_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Public types */
typedef unsigned int mz_uint;

/* Opaque archive struct (we keep a minimal internal representation). */
typedef struct {
    void *user_data; /* reserved for potential future use */
    /* internal: set when init_file() succeeds */
    char opened;     /* 0 = not opened / invalid, 1 = opened */
    /* optionally store path and size (for diagnostics) */
    const char *path;
    size_t file_size;
} mz_zip_archive;

/* File stat struct */
typedef struct {
    const char *m_filename;      /* pointer into internal buffer; may be NULL in stub */
    mz_uint    m_uncomp_size;    /* uncompressed size (stub: 0) */
    mz_uint    m_comp_size;      /* compressed size (stub: 0) */
    mz_uint    m_method;         /* compression method (stub: 0) */
    mz_uint    m_crc32;          /* CRC32 (stub: 0) */
    /* other fields omitted */
} mz_zip_archive_file_stat;

/* Initialize a zip reader from a file path.
 * Returns non-zero on success, zero on failure.
 *
 * NOTE: This stub will mark the archive as opened but will not parse
 * the zip structure. It allows callers to query num files (which will be 0).
 */
static inline int mz_zip_reader_init_file(mz_zip_archive *zip, const char *path, unsigned int flags)
{
    if (!zip || !path) return 0;
    zip->user_data = NULL;
    zip->opened = 1;
    zip->path = path;
    zip->file_size = 0;
    /* We do not parse the ZIP; indicate success but zero entries. */
    (void)flags;
    return 1;
}

/* Return number of entries in the archive. Stub returns 0. */
static inline mz_uint mz_zip_reader_get_num_files(mz_zip_archive *zip)
{
    if (!zip || !zip->opened) return 0;
    /* Stub: no parsed entries available */
    return 0u;
}

/* Fill file stat for a given entry index. Returns non-zero on success.
 * In this stub the function always fails (no entries). */
static inline int mz_zip_reader_file_stat(mz_zip_archive *zip, mz_uint file_index, mz_zip_archive_file_stat *stat)
{
    (void)zip;
    (void)file_index;
    if (!stat) return 0;
    /* Stub: nothing to report */
    stat->m_filename = NULL;
    stat->m_uncomp_size = 0;
    stat->m_comp_size = 0;
    stat->m_method = 0;
    stat->m_crc32 = 0;
    return 0;
}

/* Extract an entry to a heap allocation and return pointer. Caller must free
 * using mz_free(). On failure returns NULL.
 *
 * Stub implementation always returns NULL (no extraction). */
static inline void *mz_zip_reader_extract_to_heap(mz_zip_archive *zip, mz_uint file_index, size_t *out_len, unsigned int flags)
{
    (void)zip;
    (void)file_index;
    (void)flags;
    if (out_len) *out_len = 0;
    return NULL;
}

/* Free memory returned by mz_zip_reader_extract_to_heap.
 * Stub forwards to free(). */
static inline void mz_free(void *p)
{
    if (p) { free(p); }
}

/* Tear down the zip reader. Returns non-zero on success */
static inline int mz_zip_reader_end(mz_zip_archive *zip)
{
    if (!zip) return 0;
    zip->opened = 0;
    zip->user_data = NULL;
    zip->path = NULL;
    zip->file_size = 0;
    return 1;
}

#ifdef __cplusplus
} /* extern \"C\" */
#endif

#endif /* FT2_MINIZ_H */
