/*
 * i2c_savekey.h
 *
 * Bus-stuffed I2C driver for SaveKey/AtariVox-style EEPROM devices on the
 * joystick port, ported from i2c_v2.3.inc (Alex Herbert 2004, optimised by
 * Thomas Jentzsch 2006/2008) to run as ARM-side vcsLib bus-stuffing instead
 * of injected 6507 machine code.
 *
 * Provides the page-aware EEPROM read/write API that tinyelf_fs.c sits on
 * top of, and declares the sd*() storage commands (implemented in
 * runtime.c) that main.c's SAVE/LOAD/FORMAT/CATALOG/LIST/ENTER/DELETE use.
 */
#ifndef I2C_SAVEKEY_H
#define I2C_SAVEKEY_H

#include <stdint.h>

/* Standard SaveKey/AtariVox device address (A0=A1=A2=GND -> 0x50) */
#define I2C_SAVEKEY_DEVADDR   0x50u

/* 24LC256/24C512-class page size in bytes - writes are split on this
   boundary so a write never wraps within a page. VERIFY against the
   datasheet of whichever chip is actually populated (24LC256 and 24C512
   both use 64-byte pages; larger/newer parts may differ - the AT24CM02 in
   particular should be checked before relying on this default). */
#ifndef I2C_EEPROM_PAGE_SIZE
#define I2C_EEPROM_PAGE_SIZE  64u
#endif

/* Maximum retries for a whole read/write operation (read_retried()/
   write_retried() in tinyelf_fs.c) before giving up. The *first* I2C op of a session used to need this to ride
   out KeyPortari holding a keypress value on the bus - fixed properly now
   via the KeyPortari chip-enable line (see KEYPORTARI_CE_MASK in
   i2c_savekey.c), so this is rarely the reason for a retry anymore.
   Kept as a last-resort fallback (with i2c_retry_pause() between
   attempts) for genuine bus disturbances - the routine case of a write
   failing because the *previous* write's internal EEPROM write cycle
   hasn't finished yet is now handled properly inside i2c_eeprom_write()
   itself, see I2C_WRITE_ACK_POLL_MAX below, so this should rarely
   trigger anymore. Cheap when unused (a retry only costs anything if
   actually triggered). */
#ifndef I2C_OP_RETRY_MAX
#define I2C_OP_RETRY_MAX 64u
#endif

/* Standard I2C EEPROM "ACK polling": after a page write, the chip is
   internally busy committing it to nonvolatile memory (datasheets quote
   ~5ms typical, up to ~10ms worst case) and simply won't ACK its own
   device address at all until that finishes - not a bus problem, not a
   KeyPortari disturbance, just the chip being busy. i2c_eeprom_write()
   polls for this tightly (no delay, no leaving the bus-stuffing session)
   before starting the *next* page - this bound just needs to outlast the
   worst-case write cycle. Each attempt is one START + one address byte,
   roughly a few hundred bus-stuffed 6502 cycles - a few hundred microsec
   - so even 1000 attempts comfortably fits in a fraction of a frame.
   This was previously missing entirely (a single unbounded-wait-free
   attempt, delegated to the caller's much coarser, multi-frame
   I2C_OP_RETRY_MAX loop instead) - the likely real explanation for SAVE
   and FORMAT (both write-heavy) failing far more often than LOAD (pure
   reads, no write cycle to wait out at all). */
#ifndef I2C_WRITE_ACK_POLL_MAX
#define I2C_WRITE_ACK_POLL_MAX 1000u
#endif

/* Defined in main.c (needs renderDisplayFrame()) - leaves the I2C
   bus-stuffing session for a few normal frames and re-enters it, meant to
   be called between retry attempts of a single write inside a long
   operation (write_retried() in tinyelf_fs.c), not just once at the start
   of a command. See its definition
   for the reasoning (a disturbance recurring *during* a long I2C session,
   not just from the keypress that started it). */
void i2c_retry_pause(void);

/* Low level single-byte primitives, exposed in case something other than
   the page-aware helpers below is needed (e.g. probing a device address). */
void i2c_start(void);

/* Releases both SCL and SDA back to input. Safe/expected to call after
   every transaction, including mid-session (see i2c_stop()'s own comment
   in i2c_savekey.c for a "keep SCL driven" variant that was tried and
   reverted - made things worse on Gopher2600). */
void i2c_stop(void);

/* Redundant with i2c_stop() - kept as a real function so main.c's call
   sites (added when this did something different) don't need touching
   again. See i2c_stop()'s comment in i2c_savekey.c. */
void i2c_release_bus(void);
uint8_t i2c_txbyte(uint8_t data);   /* returns 0 = ACKed, 1 = NAKed */
uint8_t i2c_rxbyte(uint8_t ack);    /* ack: send ACK after this byte (more to come) */

/* Writes len bytes to the EEPROM at devaddr7 (7-bit I2C device address,
   e.g. I2C_SAVEKEY_DEVADDR), starting at the 16-bit memory address memaddr.
   Internally split into I2C_EEPROM_PAGE_SIZE-aligned page writes, each
   followed by ACK-polling for write-cycle completion. Returns the number of
   bytes actually written (equal to len on success). */
uint32_t i2c_eeprom_write(uint8_t devaddr7, uint16_t memaddr, const void *data, uint32_t len);

/* Reads len bytes from the EEPROM at devaddr7, starting at memaddr, via a
   single sequential read. Returns the number of bytes actually read. */
uint32_t i2c_eeprom_read(uint8_t devaddr7, uint16_t memaddr, void *data, uint32_t len);

/* Longest file name the sd*() functions hand back: "NAME.EXT" (8+1+3).
   CATALOG's name buffer in main.c holds SD_FILENAME_MAX + 1 bytes. */
#define SD_FILENAME_MAX 12

/* sdsave()/sddelete() result when the "En:" prefix is missing/malformed,
   the drive does not answer or is not formatted - main.c raises EEEPROM
   (23) for it, like CATALOG/LIST/FORMAT do. */
#define SD_NODRIVE 2u

/* SAVE "En:name" (xsave() in main.c) - saves data[0..len) under name,
   replacing a file of the same name. Returns 1 on success, SD_NODRIVE
   (see above), or 0 when the drive is full or a write fails - main.c
   raises EOUTOFMEMORY (14) on 0. */
uint8_t sdsave(const char *name, const void *data, uint32_t len);

/* DELETE "En:name" (xdelete() in main.c) - frees the file's sectors and
   marks its directory entry deleted. Returns 1 on success, SD_NODRIVE,
   or 0 when the file does not exist or a write fails - main.c raises
   EFILE (20) on 0. */
uint8_t sddelete(const char *name);

/* LOAD "name" (xload() in main.c) - loads into data (capacity maxlen),
   filling *outsize on success. Returns 0 for every current failure case
   alike (not found, a size-0 DELETE tombstone, too big for maxlen, or an
   incomplete read) - main.c raises EFILE (or sets er=EFILE silently for
   the autoboot-quiet caller) on any 0. */
uint8_t sdload(const char *name, void *data, uint32_t maxlen, uint32_t *outsize);

/* FORMAT (xformat() in main.c) - drivespec is a mandatory "En:" prefix
   (E1-E8, see the E1-E8 drive-letter project plan/tinyelf_parse_drive() in
   runtime.c); sizeOverride is the optional explicit byte count (0 = auto,
   from the drive's own I2C address-scan-derived capacity). Returns 0 if
   the drive prefix is missing/malformed, the drive isn't present, the
   size override is invalid, or a retried write was exhausted - main.c
   raises EEEPROM on any of those. */
uint8_t sdformat(const char *drivespec, uint32_t sizeOverride);

/* CATALOG's scan (xcatalog() in main.c), split into an iterator so main.c
   can keep doing its own printing between entries (outsc()/outnumber()
   don't touch vcsLib, so that's safe to interleave). sdcatalogopen()
   takes a mandatory "En:" drivespec (see sdformat()'s comment/the E1-E8
   drive-letter project plan), settles the bus, and mounts that drive -
   returns 0 (bus already released, overblank already back on) if the
   drive prefix is missing/malformed or the drive isn't present/formatted,
   so main.c can raise a real error instead of silently showing an empty
   catalog. sdcatalognext() advances *pos (start the caller's own variable at 0)
   and fills namebuf (at least SD_FILENAME_MAX+1 bytes) and *size for the
   next file in the directory - returns 0 once there are no more files.
   The bus-release + overblank re-entry happens at the end of the scan
   inside sdcatalognext() itself - no separate close call. sdcatalogfree()
   returns the free bytes of the drive (pos is no longer used).

   Two rules, both learned from real-hardware crashes that did not
   reproduce on the emulator:
   1. The "visible progress" COLUBK write happens at the START of the
      *resumed* sdcatalognext() call (after the caller's printing), not at
      the end of the call that just found an entry - the other order puts
      the caller's entire (ARM-heavy) printing between two vcsLib calls.
   2. No libc string functions (strncpy()/memcpy()/...) for copying the
      filename - the ELF loader only resolves the vcsLib functions and
      memset (see the "-r" build's undefined-symbol list). Use a
      hand-rolled byte-copy loop instead. */
uint8_t sdcatalogopen(const char *drivespec);
uint8_t sdcatalognext(uint32_t *pos, char *namebuf, uint32_t *size);
uint32_t sdcatalogfree(uint32_t pos);

/* Total formatted disk capacity in bytes, for CATALOG's "Size" line -
   wraps tinyelf_capacity_bytes(); 0 if unformatted/not mounted (main.c
   must call sdcatalogopen() first, same as for sdcatalognext()). */
uint32_t sdcatalogsize(void);

/* LIST "En:file" streaming (xlist() in main.c) - see the LIST/ENTER
   project plan. name is the full "En:FILE.EXT" string, same convention as
   sdsave(). The disabled-blanking window stays OPEN between these two
   calls, across the caller's own listlines() call in between -
   sdlistopen() opens it; the caller sets od=OEEPROM, calls
   listlines(0, maxaddr), restores od, and MUST call sdlistclose() next
   (success or failure) to finalize and close the window. Returns 0 on
   either call if the drive prefix is missing/malformed, the drive isn't
   present/mounted, or a retried write was exhausted - main.c raises
   EEEPROM on either. */
uint8_t sdlistopen(const char *name);
uint8_t sdlistclose(void);

/* ENTER "En:file" streaming (xenter() in main.c). name is the full
   "En:FILE.EXT" string.
   Unlike sdlistopen()/sdlistclose(), the window does NOT stay open across
   calls - sdenteropen() mounts and locates the file (its own complete,
   self-contained open+close cycle). sdenterchunk() then opens
   the window fresh for each chunk, reads one sector-chain-link, and closes
   it again before returning, so the caller can freely do unbounded work
   (line tokenizing, storeline()) between calls with overblank back on.
   Returns bytes filled, 0 = end of chain/error - no separate close call needed on
   this read-only side. */
uint8_t sdenteropen(const char *name);
#define SD_ENTER_ERROR 0xFFFFu /* sdenterchunk() result: broken sector chain / read error (0 = normal end of file) */
uint16_t sdenterchunk(void *buf, uint16_t maxlen);

#endif
