/*
 * tinyelf_fs.h
 *
 * DOS 2.x-style filesystem for the I2C SaveKey-compatible EEPROM, replacing
 * the earlier append-only eedir_.../eedisk_... format (i2c_savekey.c/.h - kept
 * in place for now, unused once this is fully wired in, removed in a final
 * cleanup pass once this is verified end-to-end on hardware).
 *
 * Format designed and hardware-verified by the SaveKey-Manager sibling
 * project (relayed via a peer Claude session, 2026-09-11); reference
 * implementation there: artifacts/eeprom-explorer/src/lib/tinyelf-format.ts,
 * tinyelf-directory.ts, tinyelf-save.ts.
 *
 * Layout summary (see tinyelf_fs.c for the exact byte-level packing):
 *
 *   sector 0        : boot sector + TinyELF header (magic "TELF" at $18)
 *   sector 1        : VTOC - 1 bit/sector, MSB-first, 1=free/0=allocated
 *   sector 2..N     : directory - 16-byte entries (flags, sector count LE,
 *                     start sector LE, 8-char name, 3-char ext)
 *   sector N+1..end : data sectors, chained via 3 trailing control bytes
 *                     per sector (bytesUsed, fileNumber+nextSector-hi,
 *                     nextSector-lo)
 *
 * Sector size is chosen from capacity (64/128/256 bytes) so that total
 * sectors never exceeds 1024 (fits the 10-bit next-sector field) and the
 * VTOC always fits in exactly 1 sector for the whole 4KiB-256KiB range
 * this project's capacities (32-64KiB) fall well within - no multi-sector
 * VTOC handling implemented, matching the spec's own stated assumption.
 *
 * IMPORTANT (see i2c_savekey.h's sdcatalognext() comment for the story):
 * this file must never call real libc string/mem functions other than
 * memset() (confirmed on this ELF loader's resolved-symbol whitelist,
 * already used elsewhere) - the loader for this "-r" relocatable build
 * only resolves a small fixed set of external symbols at runtime, and an
 * unresolved one (strncpy, memcpy, strcmp, ...) causes an immediate,
 * 100%-reproducible crash at boot. Every string/array operation here is a
 * hand-rolled loop.
 */
#ifndef TINYELF_FS_H
#define TINYELF_FS_H

#include <stdint.h>

/* Max on-disk name/ext field widths (space-padded, NOT NUL-terminated on
   disk). Callers (runtime.c) split a BASIC filename into these before
   calling in, and reassemble "NAME.EXT" from them for display. */
#define TINYELF_NAME_LEN 8
#define TINYELF_EXT_LEN  3

/* 'drive' is 0-7 (E1-E8) - see the E1-E8 drive-letter project plan.
   Resolved via a fresh I2C address scan every call (tinyelf_scan_drives()):
   a contiguous run of ACKing I2C addresses (0x50-0x57) collapses into ONE
   logical drive, labelled by its lowest address, capacity = addressCount *
   65536 - matching the SaveKey-Manager sibling project's own model
   (confirmed directly via its Claude session). Both functions fail
   (return 0) if 'drive' isn't a valid, present drive letter per that scan
   (e.g. drive 5 when only 0x54 ACKs but 0x55-0x57 don't - a mid-group
   address like E6 is never independently selectable, only the group's
   lowest address is). */

/* capacityOverride: 0 = auto (addressCount * 65536 for a multi-address
   drive; 32768 - matching this codebase's old i2cEepromSize default - for
   a single-address drive, since a lone ACKing address is genuinely
   capacity-ambiguous from the scan alone, confirmed by SaveKey-Manager).
   A nonzero override is validated against the drive's known capacity for
   a multi-address drive (rejected if too large) and accepted as-is for a
   single-address drive (no upper bound is knowable there).

   Formats the resolved drive: writes the header sector, the VTOC (all
   sectors free except the header/VTOC/directory sectors themselves, which
   are marked allocated), and the directory sectors (all entries flags=0,
   "never used"). Does NOT touch the data region at all - see the
   wear-leveling note in the project plan/memory. Returns 1 on success, 0
   if the drive isn't present, the capacity override is invalid, or a
   retried write was exhausted (caller raises EEEPROM). */
uint8_t tinyelf_format(uint8_t drive, uint32_t capacityOverride);

/* Reads and validates the header sector (magic "TELF", version) on the
   resolved drive. On success, caches the layout (including which I2C
   address range it lives on) for subsequent tinyelf_* calls this session
   and returns 1. Returns 0 if the drive isn't present, or is
   unformatted/bad magic (layout left unchanged/invalid - subsequent
   tinyelf_* calls will fail safely). Cheap to call at the start of every
   command, same as the old eedisk_header_read() pattern. */
uint8_t tinyelf_mount(uint8_t drive);

/* name8/ext3 are space-padded, exactly TINYELF_NAME_LEN/TINYELF_EXT_LEN
   bytes each, NOT NUL-terminated - runtime.c does the "split on '.', pad
   with spaces" conversion from a BASIC filename string before calling any
   of these; this layer only knows the disk format. All of these require
   tinyelf_mount() (or tinyelf_format()) to have succeeded first in this
   session; return 0 immediately otherwise.

   tinyelf_save() overwrites an existing same-named file (deletes it -
   freeing its sectors - before allocating fresh ones), matching how a
   real filesystem with space reclamation is expected to behave, unlike
   the old append-only format's "shadow, never reclaim" behavior. Returns
   0 if a retried write was exhausted or there wasn't enough free space
   (no partial write is left allocated in that case... except the space
   freed by an overwritten same-named file, which is NOT rolled back if
   the subsequent allocation then fails - matches "delete then re-save"
   semantics, not a transaction). */
uint8_t tinyelf_save(const char *name8, const char *ext3, const void *data, uint32_t len);

/* Loads into data (capacity maxlen), filling *outsize with the exact byte
   count (sum of bytesUsed across the file's sector chain). Returns 0 if
   not found, or if the file doesn't fit in maxlen, or a read failed. */
uint8_t tinyelf_load(const char *name8, const char *ext3, void *data, uint32_t maxlen, uint32_t *outsize);

/* Marks the entry deleted ($80) and frees its sector chain's VTOC bits.
   Returns 0 if not found or a write failed. */
uint8_t tinyelf_delete(const char *name8, const char *ext3);

/* Directory iterator - *slot is an opaque cursor, the caller starts it at
   0. Fills name8/ext3 (TINYELF_NAME_LEN/TINYELF_EXT_LEN bytes each, not
   NUL-terminated) and *sectors (the entry's on-disk sector count - NOT an
   exact byte size; the spec deliberately keeps CATALOG cheap by not
   walking every file's chain just to list it, matching real DOS 2.x
   catalog behavior of showing sector counts) for the next in-use entry,
   advancing *slot past it. Returns 0 once there are no more entries. */
uint8_t tinyelf_dir_next(uint32_t *slot, char *name8, char *ext3, uint32_t *sectors);

/* Free space in bytes, computed from the VTOC's free-sector count times
   the per-sector payload (sectorSize-3) - an upper bound, not exact,
   since the last sector of each file is normally only partially used. */
uint32_t tinyelf_free_bytes(void);

/* Total formatted capacity in bytes (totalSectors * sectorSize), for
   CATALOG's "Size" line - replaces the old format's i2cEepromSize global,
   which this filesystem never touches. Returns 0 if not mounted. */
uint32_t tinyelf_capacity_bytes(void);

/* ---- streaming SAVE, for content whose total length isn't known upfront
   (LIST "En:file" streaming a detokenized program out one character at a
   time - see the LIST/ENTER project plan). Same overwrite/allocation
   semantics as tinyelf_save() (deletes an existing same-named file first,
   "delete then re-save", not a transaction), but driven incrementally:

     if (!tinyelf_save_begin(name8, ext3)) ...fail...;
     cap = tinyelf_chunk_size();  // sectorSize-3, valid after begin()
     ...accumulate up to 'cap' bytes at a time, calling tinyelf_save_chunk()
        on each full (or final, shorter) chunk...
     if (!tinyelf_save_end()) ...fail...;

   Chaining is resolved by patching the PREVIOUS sector's control bytes
   once the NEXT one is allocated (byte-addressable EEPROM, no full-sector
   rewrite needed) rather than the whole-length lookahead tinyelf_save()
   uses - see tinyelf_fs.c's comment on save_stream_write_sector().

   NOTE (2026-09-12): a prior attempt at wiring this into LIST caused an
   unexplained, 100%-reproducible real-hardware boot crash, isolated via
   careful bisection to the mere presence of calls to tinyelf_save_chunk()/
   tinyelf_save_end() together, EVEN THOUGH THE CALLING CODE WAS PROVABLY
   NEVER EXECUTED - see project memory before re-wiring these into any
   caller. tinyelf_save_begin()/tinyelf_chunk_size() were individually
   confirmed safe; re-test tinyelf_save_chunk() and tinyelf_save_end() as
   separately as possible, one hardware test per increment. */
uint8_t tinyelf_save_begin(const char *name8, const char *ext3);
uint8_t tinyelf_save_chunk(const void *data, uint16_t len); /* 1 <= len <= tinyelf_chunk_size() */
uint8_t tinyelf_save_end(void);
uint16_t tinyelf_chunk_size(void); /* sectorSize - 3; 0 if not mounted */

/* ---- streaming LOAD, for ENTER "En:file" - reads a text file back one
   sector-chain-link at a time instead of into one whole-file buffer (see
   the LIST/ENTER project plan: there's no second RAM buffer big enough to
   hold a whole incoming file before tokenizing it either).

     if (!tinyelf_load_begin(name8, ext3)) ...not found...;
     for (;;) {
       uint16_t got = tinyelf_load_chunk(buf, sizeof(buf));
       if (got == 0) break; // end of chain
       ...process 'got' bytes...
     }

   Unlike the SAVE stream, there's no explicit *_end() - reaching end of
   chain (tinyelf_load_chunk() returning 0) is itself the completion
   signal, nothing left to finalize on this (read-only) side. */
uint8_t tinyelf_load_begin(const char *name8, const char *ext3);
#define TINYELF_LOAD_ERROR 0xFFFFu /* broken chain / read failure - distinct from 0 = normal end of file */
uint16_t tinyelf_load_chunk(void *buf, uint16_t maxlen); /* returns bytes filled, 0 = end of chain, TINYELF_LOAD_ERROR = error */

#endif
