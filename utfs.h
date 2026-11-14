/***********************************************************************
* UTFS Library
*
* Copyright (c) 2025 CLI Systems LLC <contact@clisystems.com>
* https://clisystems.com
*
* SPDX-License-Identifier: MIT
* See the LICENSE file in the project root for full license text.
*
* Vendored verbatim from https://github.com/clisystems/utfs for TinyELF
* Basic - see utfs.c for the (few) adaptations needed for this freestanding
* ELF-cart environment.
***********************************************************************/
#ifndef __UTFS_H__
#define __UTFS_H__

// Constants
// ----------------------------------------------------------------------------
#define UTFS_MAX_FILES      5
#define UTFS_MAX_FILENAME   11
//#define UTFS_ENABLE_LOG_VPRINTF
//#define UTFS_ENABLE_LOG_PRINTF

// Types
// ----------------------------------------------------------------------------
typedef enum{
    RES_OK = 0,
    RES_FILE_NOT_FOUND,
    RES_READ_ERROR,
    RES_WRITE_ERROR,
    RES_PARAM_ERROR,
    RES_FILENAME_EXISTS,
    RES_FILESYSTEM_FULL,
    RES_INVALID_FS,
}utfs_result_e;

typedef struct{
    char filename[UTFS_MAX_FILENAME+1];
    uint16_t signature;
    uint16_t flags;
    uint32_t size;
    uint32_t size_loaded;
#ifdef UTFS_ENABLE_EXT_ATTR
    uint32_t attr[4];
#endif
    void * data;
}utfs_file_t;

// Flags related to files
//   UTS_EXT_ATTR (Experimental) - Header has extended attributes
//   UTS_LOAD_EXPLICIT (Experimental) - Only load the file with a call from utfs_load_file()
//   UTS_SAVE_EXPLICIT (Experimental) - Only save the file with a call from utfs_save_file() or utfs_save_flush()
typedef enum{
	UTFS_NOFLAGS			= 0,
#ifdef UTFS_ENABLE_FLAGS
#ifdef UTFS_ENABLE_EXT_ATTR
    UTFS_EXT_ATTR       = 0x0001
#endif
    UTFS_LOAD_EXPLICIT  = 0x0100,
    UTFS_SAVE_EXPLICIT  = 0x0200,
#endif
}utfs_flags_e;


// Options for managing files.
//   UTFS_OPT_REPLACE - Replace an entry in the file list, if it already exists
typedef enum{
    UTFS_NOOPT           = 0,
    UTFS_OPT_REPLACE     = 0x01,
}utfs_options_e;

// Functions
// ----------------------------------------------------------------------------
#ifdef __cplusplus
extern "C"
{
#endif

utfs_result_e utfs_init(bool verbose);

utfs_result_e utfs_baseaddress_set(uint32_t baseaddr);

utfs_result_e utfs_register(utfs_file_t * f, utfs_flags_e flags, utfs_options_e options);
utfs_result_e utfs_unregister(utfs_file_t * f);

utfs_result_e utfs_load();
utfs_result_e utfs_save();

// Write all files, including those with SAVE_EXPLICIT
utfs_result_e utfs_save_flush();

utfs_result_e utfs_load_file(utfs_file_t * f);
utfs_result_e utfs_save_file(utfs_file_t * f);


/// Utility functions
utfs_result_e utfs_set(utfs_file_t * f,char * name, void * data, uint32_t size);
utfs_result_e utfs_set_filename(utfs_file_t * f,char * name);
utfs_result_e utfs_set_data(utfs_file_t *f,void * data, uint32_t size);

uint16_t utfs_file_signature(utfs_file_t * f);
utfs_result_e utfs_file_signature_set(utfs_file_t * f, uint16_t sig);

const char * utfs_result_str(utfs_result_e res);

/// Debug
utfs_result_e utfs_status();

#ifdef __cplusplus
}
#endif

#endif
