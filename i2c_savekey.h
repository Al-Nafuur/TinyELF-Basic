/*
 * i2c_savekey.h
 *
 * Bus-stuffed I2C driver for SaveKey/AtariVox-style EEPROM devices on the
 * joystick port, ported from i2c_v2.3.inc (Alex Herbert 2004, optimised by
 * Thomas Jentzsch 2006/2008) to run as ARM-side vcsLib bus-stuffing instead
 * of injected 6507 machine code.
 *
 * Provides a page-aware EEPROM read/write API, plus sys_read()/sys_write()
 * for the UTFS filesystem (see utfs.h) to sit on top of.
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

/* Maximum retries for a whole read/write operation (eedisk_header_read/
   write, eedir_scan_next, eedir_append, FORMAT's blank loop) before
   giving up. The *first* I2C op of a session used to need this to ride
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
   operation (FORMAT's page-by-page blanking, eedir_append()'s header/data
   writes), not just once at the start of a command. See its definition
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

/* capacity in bytes of the currently configured EEPROM. Defaults to the
   24LC256's 32768 bytes (see i2c_savekey.c), but should normally be kept
   in sync with the on-disk disk header (see eedisk_header_read() below)
   rather than relied on directly - it's a RAM variable, reset to the
   default on every reboot/reload, so it's stale until something reads the
   disk header. Used by eedir_append() to refuse writes that would run off
   the end of the chip. */
extern uint32_t i2cEepromSize;

/* Disk header, written once by FORMAT at address 0 and read back by
   eedisk_header_read() - this is what lets SAVE/LOAD/CATALOG know the
   real capacity of whatever chip is actually attached, surviving a
   reboot, instead of trusting i2cEepromSize's default. Uses its own
   identifier (EEDISK_IDENTIFIER), deliberately different from a file
   entry's (EEDIR_IDENTIFIER in i2c_savekey.c / UTFS's 0x1984), so the two
   can never be confused when scanning. The file directory (see below)
   starts right after it, at EEDIR_START. */
#define EEDISK_HEADER_SIZE 8u
#define EEDIR_START EEDISK_HEADER_SIZE

/* Reads the disk header at address 0; on success updates i2cEepromSize
   from it and returns 1. Returns 0 (leaving i2cEepromSize unchanged) if
   there's no valid disk header yet - i.e. FORMAT has never been run on
   this chip. */
uint8_t eedisk_header_read(void);

/* Writes the disk header recording 'capacity' bytes, called by FORMAT
   (xformat() in main.c) - does not touch anything past the header itself,
   FORMAT's caller still has to blank the rest. Returns 1 on success, 0 if
   the write didn't fully go through (caller should retry). */
uint8_t eedisk_header_write(uint32_t capacity);

/* Simple append-only directory of named files on the EEPROM, starting at
   EEDIR_START, using the on-disk header layout UTFS (utfs.c/.h) also
   uses - 0x1984 identifier, version 1, 2-byte signature/reserved, 4-byte
   size, 11-char name - so anything else that understands UTFS's format
   can still read what we write here. Unlike utfs.c's own API (built
   around a fixed, compile-time known set of save slots), this scans the
   disk directly and supports arbitrary names discovered at runtime, which
   is what SAVE/LOAD/CATALOG need. There is no space reclamation: SAVE
   always appends: a later entry with the same name shadows an earlier one
   (found first by eedir_find(), which scans for the LAST match), and
   FORMAT is how you reclaim space.

   NOTE: this constant is ALSO reused by main.c's CATALOG buffer sizing
   for the new tinyelf_fs.c-based format (sdcatalognext() in runtime.c) -
   bumped from 11 to 12 to fit "NAME.EXT" (8+1+3) instead of the old
   11-char concatenated form, with zero main.c changes needed since it
   already parameterizes its buffer/padding width off this define. */
#define EEDIR_MAX_FILENAME 12

typedef struct {
  char name[EEDIR_MAX_FILENAME + 1];
  uint32_t size;
  uint32_t headerPos; /* where this entry's header starts on the EEPROM */
  uint32_t dataPos;   /* where this entry's data starts (headerPos + 24) */
} eedir_entry_t;

/* Reads the header at 'pos'; on success (valid UTFS identifier/version)
   fills *entry and returns the offset just past this entry's data (i.e.
   where the next header would start). Returns 0 if 'pos' is not a valid
   header - the end of the directory. */
uint32_t eedir_scan_next(uint32_t pos, eedir_entry_t *entry);

/* Finds the most recent entry named 'name' (case-sensitive, matches
   EEDIR_MAX_FILENAME chars). Returns 1 and fills *entry if found, 0 if
   not. */
uint8_t eedir_find(const char *name, eedir_entry_t *entry);

/* Returns 1 if an entry named 'name' appears again anywhere from fromPos
   onward - i.e. whether an entry just scanned is "shadowed" by a later
   same-named one (see the append-only/no-reclaim note above). Used by
   CATALOG to skip printing dead entries; pass the entry's own 'next' scan
   position (not its headerPos) as fromPos so the entry doesn't match
   itself. */
uint8_t eedir_superseded(const char *name, uint32_t fromPos);

/* Offset just past the last valid directory entry (starting the scan from
   EEDIR_START) - where the next eedir_append() will write. */
uint32_t eedir_end(void);

/* Appends a new header+data entry at eedir_end(). name is truncated to
   EEDIR_MAX_FILENAME chars. Returns 1 on success, 0 if it wouldn't fit
   within i2cEepromSize. */
uint8_t eedir_append(const char *name, const void *data, uint32_t size);

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
   the autoboot-quiet caller) on any 0. Fourth function moved out of
   main.c; see project memory before adding more. */
uint8_t sdload(const char *name, void *data, uint32_t maxlen, uint32_t *outsize);

/* FORMAT (xformat() in main.c) - drivespec is a mandatory "En:" prefix
   (E1-E8, see the E1-E8 drive-letter project plan/tinyelf_parse_drive() in
   runtime.c); sizeOverride is the optional explicit byte count (0 = auto,
   from the drive's own I2C address-scan-derived capacity). Returns 0 if
   the drive prefix is missing/malformed, the drive isn't present, the
   size override is invalid, or a retried write was exhausted - main.c
   raises EEEPROM on any of those. Fifth function moved out of main.c; see
   project memory before adding more. */
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
   and fills namebuf (must be at least EEDIR_MAX_FILENAME+1 bytes) and
   *size for the next entry CATALOG actually shows (size-0 tombstones and
   superseded/shadowed entries are skipped internally) - returns 0 once
   there are no more visible entries, at which point *pos sits at the
   true physical end of the directory, so sdcatalogfree() afterward is
   accurate (returns i2cEepromSize - pos). The bus-release + overblank
   re-entry happens at the end of the scan inside sdcatalognext() itself -
   no separate close call.

   Two real bugs found and fixed across two earlier attempts at this
   extraction, both real-hardware boot crashes that didn't reproduce on
   the emulator - worth knowing about if this ever needs touching again:
   1. The "visible progress" COLUBK write must happen at the START of the
      *resumed* sdcatalognext() call (after the caller's printing), not
      at the end of the call that just found an entry (before the
      caller's printing) - the original inline loop always printed
      before its own COLUBK write, immediately followed by the next
      eedir_scan_next() call; the other order stretches the real elapsed
      time between two vcsLib calls across the caller's entire
      (ARM-heavy) printing.
   2. sdcatalognext()'s implementation must NOT call the real libc
      strncpy()/memcpy()/etc for copying the filename - this project's
      ELF loader only resolves a small whitelist of external symbols at
      runtime (vcsLib functions, ColorLookup, memset - see the "-r"
      relocatable build's undefined-symbol list), and a real strncpy()
      call is NOT on it (see utfs.c's own strncpy/strncmp macro-redirect
      to a local replacement, done for exactly this reason - it should
      have been the tip-off). Use a hand-rolled byte-copy loop instead.
   Sixth and last function moved out of main.c in this extraction pass. */
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

/* ENTER "En:file" streaming (xenter() in main.c, not yet written) - see
   the LIST/ENTER project plan. name is the full "En:FILE.EXT" string.
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
