/*
 * tinyelf_fs.c
 *
 * See tinyelf_fs.h. Stage 1 of the migration: layout computation, FORMAT,
 * and MOUNT (header read/validate) only - SAVE/LOAD/CATALOG/DELETE follow
 * in later stages, each verified on real hardware before the next is
 * added (see the project plan - this format broke real hardware boot
 * twice during the *previous* refactor for reasons that had nothing to do
 * with logic correctness - a stray libc symbol, and a vcsLib call
 * ordering issue - so this is being done the same careful way).
 */
#include "i2c_savekey.h"
#include "tinyelf_fs.h"
#include "vcsLib.h"

#include <string.h> /* memset() only - see tinyelf_fs.h's warning */

/* While outside overblank, the real 6507 must be kept busy essentially
   continuously - a stretch of plain ARM computation with no vcsLib call
   at all crashes real hardware. A bare vcsJmp3() does NOT buy any real
   time for this (confirmed directly by the user, who designed this
   mechanism) - only vcsNop2n(x) does, by driving the data bus to a NOP
   opcode for the injected 6502 to chew on, buying x*2 real 6502 cycles of
   safe ARM time.

   First attempt here used one big vcsNop2n() precharge before each
   bounded chunk of ARM-only work (a memset, header field assignments, a
   VTOC bit-twiddling loop, per-sector byte copies) instead of repeating
   it through the loop - still crashed real hardware, meaning that
   single-guess budget was wrong (too small, or the real cost model is
   different than assumed - genuinely not sure which). Rather than
   guessing a bigger flat number again, this calls a small, cheap
   arm_pace() every few loop iterations instead - the max possible gap
   between real budget purchases is then bounded by a handful of loop
   iterations' worth of work, not by however large the whole loop turns
   out to be, so it doesn't matter if the total iteration count is 10 or
   1000, or if a per-iteration cost estimate is off by some factor - it
   fails safe by just buying more often rather than needing one correct
   big guess up front.

   Also resets the injected 6502 PC via vcsJmp3() at the same cadence -
   vcsNop2n() buys real time but does NOT by itself keep the injected PC
   within the valid bus-stuffed ROM window over many repetitions; both
   need to happen together in a long loop, not just the time-buying half. */
static void arm_pace(uint16_t i) {
  if ((i & 7u) == 0u) {
    vcsJmp3();
    vcsNop2n(50);
  }
}

/* Sector layout, cached after a successful tinyelf_mount()/tinyelf_format().
   uint16_t sector counts/indices are enough for the whole range this
   format supports (max 1024 sectors, guaranteed by compute_layout()'s
   capacity-based sector-size tiering regardless of drive size) - but the
   BYTE ADDRESSES computed from them (sector * sectorSize) can now exceed
   65536 for a multi-block drive (see the E1-E8 drive-letter project plan),
   so those are uint32_t at every call site below, translated down to a
   16-bit within-block address by write_retried()/read_retried(). */
typedef struct {
  uint16_t sectorSize;
  uint16_t totalSectors;
  uint16_t vtocStart;
  uint16_t vtocSectors;
  uint16_t dirStart;
  uint16_t dirSectors;
  uint16_t dataStart;
  uint8_t  baseAddr7;  /* I2C 7-bit address of this drive's first 64KiB block */
  uint8_t  mounted;
} tinyelf_layout_t;

static tinyelf_layout_t L;

/* Largest sector size we support (256) - one sector-sized scratch buffer,
   reused across format/mount so we don't need per-call VLAs. */
#define TINYELF_MAX_SECTOR 256u

/* addr is a byte offset within the whole logical (possibly multi-block)
   drive - translated to (I2C device address, 16-bit within-block address)
   before every real I2C call. Never needs to split a single call across
   two blocks: every caller passes a sector-aligned address, and every
   sector size this format uses (64/128/256) evenly divides 65536, so a
   sector never straddles a 64KiB block boundary (confirmed against the
   SaveKey-Manager sibling project's own implementation, which relies on
   the same property for the same reason - see the project plan). */
static uint8_t write_retried(uint32_t addr, const void *buf, uint16_t len) {
  uint8_t tries, ok = 0;
  uint8_t devaddr7 = (uint8_t)(L.baseAddr7 + (addr >> 16));
  uint16_t memaddr = (uint16_t)(addr & 0xFFFFu);
  for (tries = 0; tries < I2C_OP_RETRY_MAX && !ok; tries++) {
    if (tries) i2c_retry_pause();
    ok = (i2c_eeprom_write(devaddr7, memaddr, buf, len) == len);
  }
  return ok;
}

static uint8_t read_retried(uint32_t addr, void *buf, uint16_t len) {
  uint8_t tries;
  uint8_t devaddr7 = (uint8_t)(L.baseAddr7 + (addr >> 16));
  uint16_t memaddr = (uint16_t)(addr & 0xFFFFu);
  for (tries = 0; tries < I2C_OP_RETRY_MAX; tries++) {
    if (i2c_eeprom_read(devaddr7, memaddr, buf, len) == len) return 1;
  }
  return 0;
}

/* ---- drive scan/resolution: E1-E8 = 0x50-0x57, contiguous ACKing
   addresses collapse into one logical drive labelled by the lowest
   address in the run - see the project plan for the full rationale
   (matches the SaveKey-Manager sibling project's own model, confirmed
   directly via its Claude session). ---- */
#define TINYELF_NUM_DRIVES 8u

typedef struct {
  uint8_t present;    /* 1 = a valid, selectable drive letter */
  uint8_t baseAddr7;
  uint8_t numBlocks;  /* contiguous 64KiB blocks, 1-8 */
} tinyelf_drive_t;

/* Real I2C - every probe is a fixed, ARM-timed bus-stuffed sequence
   (i2c_start()+i2c_txbyte()), never waiting on real hardware signals, so
   unlike a real I2C peripheral (which can clock-stretch on an absent
   slave) there is no hang risk probing all 8 addresses back-to-back.
   Every probe is closed with its own i2c_stop(): a real EEPROM would
   tolerate a repeated start after an ACKed address, but Gopher2600's
   SaveKey emulation stays in its address/data state after an ACKed probe
   and would swallow the following probe bytes as EEPROM address/data
   (only STOP resets it). Caller must already be inside the
   disabled-blanking window - this does no window management of its own. */
static void tinyelf_scan_drives(tinyelf_drive_t drives[TINYELF_NUM_DRIVES]) {
  uint8_t ack[TINYELF_NUM_DRIVES];
  uint8_t i;

  for (i = 0; i < TINYELF_NUM_DRIVES; i++) {
    i2c_start();
    ack[i] = (uint8_t)(i2c_txbyte((uint8_t)(((I2C_SAVEKEY_DEVADDR + i) << 1) | 0u)) == 0);
    i2c_stop();
  }

  /* All eight addresses answering is not a real bus (Gopher2600's SaveKey
     never NAKs an unknown address): treat it as a single device at 0x50. */
  for (i = 0; i < TINYELF_NUM_DRIVES && ack[i]; i++) ;
  if (i == TINYELF_NUM_DRIVES) {
    for (i = 1; i < TINYELF_NUM_DRIVES; i++) ack[i] = 0;
  }

  /* Grouping below is plain ARM work (max 8 iterations, comparable to
     compute_layout()'s already-established-safe size) - small precharge,
     can't be hoisted out since it depends on ack[] just read above. */
  vcsJmp3(); vcsNop2n(20);
  for (i = 0; i < TINYELF_NUM_DRIVES; i++) drives[i].present = 0;
  i = 0;
  while (i < TINYELF_NUM_DRIVES) {
    if (ack[i]) {
      uint8_t start = i, n = 0;
      while (i < TINYELF_NUM_DRIVES && ack[i]) { n++; i++; }
      drives[start].present = 1;
      drives[start].baseAddr7 = (uint8_t)(I2C_SAVEKEY_DEVADDR + start);
      drives[start].numBlocks = n;
    } else {
      i++;
    }
  }
}

/* Resolves drive (0-7 = E1-E8) via a fresh scan. Returns 0 if out of range
   or not present (including a mid-group address like E6 when only E5's
   group as a whole is a valid drive - see tinyelf_scan_drives()). Real
   I2C - caller must already be inside the disabled-blanking window. */
static uint8_t tinyelf_resolve_drive(uint8_t drive, tinyelf_drive_t *out) {
  tinyelf_drive_t drives[TINYELF_NUM_DRIVES];
  if (drive >= TINYELF_NUM_DRIVES) return 0;
  tinyelf_scan_drives(drives);
  if (!drives[drive].present) return 0;
  *out = drives[drive];
  return 1;
}

/* Sector size by capacity, directory size by capacity (capped so
   dirSectors * floor(sectorSize/16) <= 64 - the file-number field is only
   6 bits) - see tinyelf_fs.h's layout summary. */
static void compute_layout(uint32_t capacity, tinyelf_layout_t *l) {
  uint16_t sectorSize, dirSectors;

  if (capacity <= 32768UL) sectorSize = 64;
  else if (capacity <= 131072UL) sectorSize = 128;
  else sectorSize = 256;

  if (capacity <= 8192UL) dirSectors = 1;
  else if (capacity <= 16384UL) dirSectors = 2;
  else if (capacity <= 32768UL) dirSectors = 4;
  else dirSectors = 8;

  while (dirSectors > 1 && (uint32_t)dirSectors * (sectorSize / 16) > 64) {
    dirSectors = (uint16_t)(dirSectors >> 1);
  }

  l->sectorSize = sectorSize;
  l->totalSectors = (uint16_t)(capacity / sectorSize);
  l->vtocStart = 1;
  l->vtocSectors = 1;
  l->dirStart = 2;
  l->dirSectors = dirSectors;
  l->dataStart = (uint16_t)(2 + dirSectors);
  l->mounted = 0; /* caller sets this once actually written/validated */
}

uint8_t tinyelf_format(uint8_t drive, uint32_t capacityOverride) {
  tinyelf_layout_t nl;
  uint8_t headerBuf[TINYELF_MAX_SECTOR];
  uint8_t vtocBuf[TINYELF_MAX_SECTOR];
  uint8_t zeroBuf[TINYELF_MAX_SECTOR];
  tinyelf_drive_t dr;
  uint32_t capacity, maxCapacity;
  uint16_t s;
  uint8_t ok;

  L.mounted = 0; /* see tinyelf_mount()'s comment on why this is cleared up front, not left stale on failure */

  /* ---- Phase 1: resolve the drive - needs real I2C (a full 8-address
     scan), so this gets its own short disabled-blanking window, kept
     separate from phase 2's writes below so the pure-ARM buffer building
     in between (which needs to know the resolved capacity first) still
     runs with overblank back on, not sandwiched inside the window. ---- */
  vcsEndOverblank();
  vcsWrite5(COLUBK, 0xa0);
  vcsWrite5(VBLANK, 0);
  ok = tinyelf_resolve_drive(drive, &dr);
  i2c_release_bus();
  vcsWrite5(VBLANK, 2);
  vcsStartOverblank();
  if (!ok) return 0;

  /* Capacity: auto (addressCount * 65536) for a multi-block drive, or a
     32768 (24LC256) default for a single-block drive
     (genuinely capacity-ambiguous from the scan alone - confirmed via the
     SaveKey-Manager sibling project, see the project plan) when no
     explicit override is given; an explicit override is only bounds-
     checked against a multi-block drive's known capacity, since a
     single-block drive's real size isn't knowable here at all. */
  maxCapacity = (uint32_t)dr.numBlocks * 65536UL;
  if (capacityOverride == 0) {
    capacity = (dr.numBlocks == 1) ? 32768UL : maxCapacity;
  } else if (dr.numBlocks > 1 && capacityOverride > maxCapacity) {
    return 0; /* explicit size too big for this known-size multi-block drive */
  } else {
    capacity = capacityOverride;
  }

  /* ---- Phase 2 prep: layout computation, all three sector buffers - is
     plain ARM work that never touches the 2600's peripheral bus (no I2C to
     the SaveKey needed for any of it). None of it has to happen while
     overblank is disabled, so none of it does here either - same
     reasoning as before drive letters existed, just now downstream of the
     phase-1 scan instead of the function's own first statement. ---- */
  compute_layout(capacity, &nl);
  if (nl.sectorSize > TINYELF_MAX_SECTOR) return 0; /* out of range, shouldn't happen */

  /* --- header sector (sector 0) --- */
  memset(headerBuf, 0, nl.sectorSize);
  headerBuf[0x00] = 1; /* BFLAG - not bootable, deliberately not 0 */
  headerBuf[0x11] = (uint8_t)(nl.sectorSize - 3); /* BLDISP */
  headerBuf[0x14] = (uint8_t)(nl.sectorSize >> 8);
  headerBuf[0x15] = (uint8_t)(nl.sectorSize & 0xFFu);
  headerBuf[0x16] = (uint8_t)(nl.totalSectors >> 8);
  headerBuf[0x17] = (uint8_t)(nl.totalSectors & 0xFFu);
  headerBuf[0x18] = 'T';
  headerBuf[0x19] = 'E';
  headerBuf[0x1A] = 'L';
  headerBuf[0x1B] = 'F';
  headerBuf[0x1C] = 1; /* FS version */
  headerBuf[0x1D] = 0; /* flags */
  headerBuf[0x1E] = (uint8_t)(nl.vtocStart >> 8);
  headerBuf[0x1F] = (uint8_t)(nl.vtocStart & 0xFFu);
  headerBuf[0x20] = (uint8_t)(nl.vtocSectors >> 8);
  headerBuf[0x21] = (uint8_t)(nl.vtocSectors & 0xFFu);
  headerBuf[0x22] = (uint8_t)(nl.dirStart >> 8);
  headerBuf[0x23] = (uint8_t)(nl.dirStart & 0xFFu);
  headerBuf[0x24] = (uint8_t)(nl.dirSectors >> 8);
  headerBuf[0x25] = (uint8_t)(nl.dirSectors & 0xFFu);

  /* --- VTOC (sector nl.vtocStart): 1=free, MSB-first. Everything free
     except the header/VTOC/directory sectors (0..dataStart-1) and any
     padding bits past totalSectors in the last VTOC byte. --- */
  memset(vtocBuf, 0xFF, nl.sectorSize);
  for (s = 0; s < nl.dataStart; s++) {
    vtocBuf[s >> 3] = (uint8_t)(vtocBuf[s >> 3] & ~(0x80u >> (s & 7)));
  }
  for (s = nl.totalSectors; s < (uint16_t)(nl.sectorSize * 8u); s++) {
    vtocBuf[s >> 3] = (uint8_t)(vtocBuf[s >> 3] & ~(0x80u >> (s & 7)));
  }

  /* --- directory sectors: every entry flags=0 ("never used"); all
     dirSectors share this same all-zero content --- */
  memset(zeroBuf, 0, nl.sectorSize);

  /* ---- Phase 2: NOW, and only now, do we need the 2600's I2C bus again -
     enter a fresh bus-stuffed window for just the real writes. L.baseAddr7
     is set here (not deferred to the final L=nl below) since
     write_retried() needs it for every call in this phase. ---- */
  vcsEndOverblank();
  vcsWrite5(COLUBK, 0xa0);
  vcsWrite5(VBLANK, 0); /* disable blanking */

  L.baseAddr7 = dr.baseAddr7;
  ok = write_retried(0, headerBuf, nl.sectorSize);
  if (ok) ok = write_retried((uint32_t)nl.vtocStart * nl.sectorSize, vtocBuf, nl.sectorSize);
  if (ok) {
    for (s = 0; s < nl.dirSectors && ok; s++) {
      uint32_t addr = (uint32_t)(nl.dirStart + s) * nl.sectorSize;
      ok = write_retried(addr, zeroBuf, nl.sectorSize); /* write_retried() itself keeps the bus busy */
    }
  }
  /* data region deliberately untouched - wear-leveling, see tinyelf_fs.h */

  i2c_release_bus();
  vcsWrite5(VBLANK, 2); /* enable blanking */
  vcsStartOverblank();

  if (!ok) return 0;

  nl.mounted = 1;
  nl.baseAddr7 = dr.baseAddr7;
  L = nl;
  return 1;
}

uint8_t tinyelf_mount(uint8_t drive) {
  uint8_t buf[38]; /* through the last header field, $00-$25 */
  tinyelf_drive_t dr;

  /* Cleared up front, not just left alone on failure - L is a static, so
     without this a failed mount (e.g. the disk has an older, pre-TinyELF
     format's header, or isn't formatted at all)
     could leave L.mounted stuck at 1 from an earlier successful
     mount/format this session, and every other tinyelf_* call would then
     silently keep using that stale layout instead of correctly failing -
     found via CATALOG appearing to show old-format files after a FORMAT
     that didn't actually complete. */
  L.mounted = 0;

  /* Real I2C (a scan), but no large ARM-only work follows it before the
     header read below (also real I2C) - unlike tinyelf_format(), this
     doesn't need its own separate window; the caller (runtime.c's sd*()
     wrappers) already has one open. */
  if (!tinyelf_resolve_drive(drive, &dr)) return 0;
  L.baseAddr7 = dr.baseAddr7;

  if (!read_retried(0, buf, sizeof(buf))) return 0;
  if (!(buf[0x18] == 'T' && buf[0x19] == 'E' && buf[0x1A] == 'L' && buf[0x1B] == 'F')) return 0;
  if (buf[0x1C] != 1) return 0; /* unknown version */

  /* The 7 field-extraction assignments below are plain ARM work, zero
     vcsLib calls - this function must cover its own gap rather than
     relying on whatever the caller happens to do next (that was an
     accidental dependency: tinyelf_split_name()'s own precharge used to
     run right after this returned, incidentally covering this gap too,
     for every caller of mount() - SAVE/LOAD/DELETE all broke together
     when that call moved out from behind vcsEndOverblank(), see runtime.c). */
  vcsJmp3(); vcsNop2n(20);
  L.sectorSize = (uint16_t)((buf[0x14] << 8) | buf[0x15]);
  L.totalSectors = (uint16_t)((buf[0x16] << 8) | buf[0x17]);
  L.vtocStart = (uint16_t)((buf[0x1E] << 8) | buf[0x1F]);
  L.vtocSectors = (uint16_t)((buf[0x20] << 8) | buf[0x21]);
  L.dirStart = (uint16_t)((buf[0x22] << 8) | buf[0x23]);
  L.dirSectors = (uint16_t)((buf[0x24] << 8) | buf[0x25]);
  L.dataStart = (uint16_t)(L.dirStart + L.dirSectors);

  if (L.sectorSize == 0 || L.sectorSize > TINYELF_MAX_SECTOR) return 0;

  L.mounted = 1;
  return 1;
}

/* ---- directory entries: 16 bytes each, byte-addressable (no need to
   read/preserve a whole sector to touch one entry - the EEPROM is
   byte-addressable, unlike flash's erase-block constraints) ---- */
#define TINYELF_ENTRY_SIZE 16u
#define TDIR_FLAG_NEVERUSED 0x00u
#define TDIR_FLAG_DELETED   0x80u
#define TDIR_FLAG_INUSE     0x40u

/* "Present" (a real, visible entry) is NOT "exactly equals TDIR_FLAG_INUSE"
   - other real, meaningful bits can ride along (LOCKED 0x20, OPENED_FOR_
   OUTPUT 0x01, and the SaveKey-Manager sibling project's own
   CREATED_BY_DOS2 0x02, which it unconditionally ORs into every upload's
   flags byte - confirmed directly via its Claude session, after a real
   interop bug: our exact-match check silently hid every file uploaded via
   that tool from CATALOG/LOAD/SAVE-overwrite). Matches that project's own
   parseDirectoryEntry() convention exactly: present = not (never-used or
   deleted), not a positive match against one specific "in-use" byte. */
static uint8_t tdir_entry_present(uint8_t flags) {
  return (uint8_t)(flags != TDIR_FLAG_NEVERUSED && !(flags & TDIR_FLAG_DELETED));
}

static uint16_t entries_per_sector(void) {
  return (uint16_t)(L.sectorSize / TINYELF_ENTRY_SIZE);
}

static uint32_t max_files(void) {
  return (uint32_t)L.dirSectors * entries_per_sector();
}

static uint32_t dir_entry_addr(uint32_t slot) {
  uint16_t eps = entries_per_sector();
  uint16_t sector = (uint16_t)(L.dirStart + slot / eps);
  uint16_t offset = (uint16_t)((slot % eps) * TINYELF_ENTRY_SIZE);
  return (uint32_t)sector * L.sectorSize + offset;
}

static uint8_t dir_read(uint32_t slot, uint8_t *entry16) {
  return read_retried(dir_entry_addr(slot), entry16, (uint16_t)TINYELF_ENTRY_SIZE);
}

static uint8_t dir_write(uint32_t slot, const uint8_t *entry16) {
  return write_retried(dir_entry_addr(slot), entry16, (uint16_t)TINYELF_ENTRY_SIZE);
}

static uint8_t name_matches(const uint8_t *entry16, const char *name8, const char *ext3) {
  uint8_t i;
  /* ~11-byte compare, no vcsLib call in the loops themselves, so it
     needs this precharge (see the comment at the top). Only reachable once a
     directory has an in-use entry to compare against, which FORMAT/SAVE's
     testing on an all-never-used fresh directory never exercised - found
     via a real hardware crash on the first LOAD against a saved file. */
  vcsJmp3(); vcsNop2n(200);
  for (i = 0; i < TINYELF_NAME_LEN; i++) {
    if (entry16[5 + i] != (uint8_t)name8[i]) return 0;
  }
  for (i = 0; i < TINYELF_EXT_LEN; i++) {
    if (entry16[13 + i] != (uint8_t)ext3[i]) return 0;
  }
  return 1;
}

static uint8_t find_entry(const char *name8, const char *ext3, uint32_t *slotOut, uint8_t *entry16) {
  uint32_t slot, n = max_files();
  for (slot = 0; slot < n; slot++) {
    if (!dir_read(slot, entry16)) return 0;
    if (tdir_entry_present(entry16[0]) && name_matches(entry16, name8, ext3)) {
      *slotOut = slot;
      return 1;
    }
  }
  return 0;
}

static uint8_t find_free_slot(uint32_t *slotOut, uint8_t *entry16) {
  uint32_t slot, n = max_files();
  for (slot = 0; slot < n; slot++) {
    if (!dir_read(slot, entry16)) return 0;
    if (!tdir_entry_present(entry16[0])) {
      *slotOut = slot;
      return 1;
    }
  }
  return 0;
}

/* ---- VTOC: read-modify-write the whole (single) sector - simplest way
   to flip a handful of bits, and matches the spec's own "rewrite VTOC,
   whole, always 1 sector" wording ---- */
static uint8_t vtoc_read(uint8_t *buf) {
  return read_retried((uint32_t)L.vtocStart * L.sectorSize, buf, L.sectorSize);
}

static uint8_t vtoc_write(const uint8_t *buf) {
  return write_retried((uint32_t)L.vtocStart * L.sectorSize, buf, L.sectorSize);
}

static uint8_t vtoc_get(const uint8_t *buf, uint16_t sector) {
  return (uint8_t)((buf[sector >> 3] >> (7 - (sector & 7))) & 1u);
}

static void vtoc_set(uint8_t *buf, uint16_t sector, uint8_t isFree) {
  uint8_t mask = (uint8_t)(0x80u >> (sector & 7));
  if (isFree) buf[sector >> 3] = (uint8_t)(buf[sector >> 3] | mask);
  else buf[sector >> 3] = (uint8_t)(buf[sector >> 3] & (uint8_t)~mask);
}

/* Packs/unpacks the 10-bit next-sector value split across a data sector's
   trailing control bytes 1 (top 2 bits, bits 6-7) and 2 (bottom 8 bits) -
   see tinyelf_fs.h's layout summary. */
static uint16_t decode_next_sector(uint8_t ctrl1, uint8_t ctrl2) {
  return (uint16_t)(((uint16_t)(ctrl1 & 0xC0u) << 2) | ctrl2);
}

/* Reading a data sector: LOAD/ENTER check only the ACK bits of the I2C
   transfer, never the data. A single misread bit in the control bytes (seen
   as Error 20 on a 7800 with long files, where the same bytes load fine on
   a 2600) used to abort the whole load. So a sector whose control bytes are
   implausible (byte count larger than the payload, next sector outside the
   data area) is simply read again. Data bytes cannot be checked. */
#define SECTOR_READ_TRIES 8u

static uint8_t sector_ctrl_plausible(const uint8_t *buf) {
  uint16_t next = decode_next_sector(buf[L.sectorSize - 2], buf[L.sectorSize - 1]);
  if (buf[L.sectorSize - 3] > (uint16_t)(L.sectorSize - 3)) return 0;
  if (next != 0 && (next < L.dataStart || next >= L.totalSectors)) return 0;
  return 1;
}

static uint8_t read_sector_checked(uint16_t sector, uint8_t *buf) {
  uint8_t tries;
  for (tries = 0; tries < SECTOR_READ_TRIES; tries++) {
    if (tries) i2c_retry_pause();
    if (read_retried((uint32_t)sector * L.sectorSize, buf, L.sectorSize) && sector_ctrl_plausible(buf)) return 1;
  }
  return 0;
}

/* Walks a file's sector chain starting at 'startSector', marking every
   sector in it free in vtocBuf (in RAM only - caller writes vtocBuf back
   with vtoc_write()). Used by tinyelf_delete() and by tinyelf_save() when
   overwriting an existing same-named file. */
static uint8_t free_chain(uint16_t startSector, uint8_t *vtocBuf) {
  uint16_t sector = startSector;
  uint16_t guard;
  for (guard = 0; sector != 0 && guard < L.totalSectors; guard++) {
    uint8_t ctrl[3];
    uint16_t nextSector;
    if (!read_retried((uint32_t)sector * L.sectorSize + (L.sectorSize - 3), ctrl, 3)) return 0;
    nextSector = decode_next_sector(ctrl[1], ctrl[2]);
    vtoc_set(vtocBuf, sector, 1);
    sector = nextSector;
  }
  return 1;
}

static uint8_t delete_entry(uint32_t slot, const uint8_t *entry) {
  uint8_t vtocBuf[TINYELF_MAX_SECTOR];
  uint16_t startSector = (uint16_t)(entry[3] | (entry[4] << 8));
  uint8_t newEntry[TINYELF_ENTRY_SIZE];
  uint8_t k;

  if (!vtoc_read(vtocBuf)) return 0;
  if (!free_chain(startSector, vtocBuf)) return 0;
  if (!vtoc_write(vtocBuf)) return 0;

  vcsJmp3(); vcsNop2n(200); /* plain ARM work below (16-byte entry copy), no vcsLib call in the loop itself */
  for (k = 0; k < TINYELF_ENTRY_SIZE; k++) newEntry[k] = entry[k];
  newEntry[0] = (uint8_t)(entry[0] | TDIR_FLAG_DELETED); /* set the deleted bit, don't clobber any other flag bits (LOCKED, a peer tool's own bits, ...) */
  return dir_write(slot, newEntry);
}

uint8_t tinyelf_delete(const char *name8, const char *ext3) {
  uint32_t slot;
  uint8_t entry[TINYELF_ENTRY_SIZE];

  if (!L.mounted) return 0;
  if (!find_entry(name8, ext3, &slot, entry)) return 0;
  return delete_entry(slot, entry);
}

uint8_t tinyelf_save(const char *name8, const char *ext3, const void *data, uint32_t len) {
  const uint8_t *p = (const uint8_t *)data;
  uint8_t vtocBuf[TINYELF_MAX_SECTOR];
  uint8_t sectorBuf[TINYELF_MAX_SECTOR];
  uint8_t entry[TINYELF_ENTRY_SIZE];
  uint32_t existingSlot, freeSlot, written;
  uint16_t payload, needSectors, i;
  uint16_t scanCursor, thisSector = 0, nextSector;
  uint16_t firstSector = 0;
  uint8_t haveThis = 0;

  if (!L.mounted) return 0;

  /* SAVE overwrites: a real filesystem with space reclamation is
     expected to replace an existing same-named file, not shadow it (see
     tinyelf_fs.h). */
  if (find_entry(name8, ext3, &existingSlot, entry)) {
    if (!delete_entry(existingSlot, entry)) return 0;
  }

  if (!find_free_slot(&freeSlot, entry)) return 0;

  payload = (uint16_t)(L.sectorSize - 3);
  needSectors = (uint16_t)((len + payload - 1) / payload);
  if (needSectors == 0) needSectors = 1; /* an empty (0-byte) save still gets one sector */

  if (!vtoc_read(vtocBuf)) return 0;

  scanCursor = L.dataStart;
  written = 0;

  for (i = 0; i < needSectors; i++) {
    uint16_t chunk, k;

    if (!haveThis) {
      while (scanCursor < L.totalSectors && !vtoc_get(vtocBuf, scanCursor)) {
        arm_pace(scanCursor);
        scanCursor++;
      }
      if (scanCursor >= L.totalSectors) return 0; /* out of space */
      thisSector = scanCursor++;
      haveThis = 1;
    }

    if ((uint16_t)(i + 1) < needSectors) {
      while (scanCursor < L.totalSectors && !vtoc_get(vtocBuf, scanCursor)) {
        arm_pace(scanCursor);
        scanCursor++;
      }
      if (scanCursor >= L.totalSectors) return 0; /* out of space */
      nextSector = scanCursor++;
    } else {
      nextSector = 0;
    }

    vcsJmp3(); vcsNop2n(200);
    chunk = (uint16_t)((len - written) > payload ? payload : (len - written));
    for (k = 0; k < chunk; k++) { arm_pace(k); sectorBuf[k] = p[written + k]; }
    for (; k < payload; k++) { arm_pace(k); sectorBuf[k] = 0; }
    sectorBuf[L.sectorSize - 3] = (uint8_t)chunk;
    sectorBuf[L.sectorSize - 2] = (uint8_t)((freeSlot & 0x3Fu) | (((nextSector >> 8) & 0x03u) << 6));
    sectorBuf[L.sectorSize - 1] = (uint8_t)(nextSector & 0xFFu);

    if (!write_retried((uint32_t)thisSector * L.sectorSize, sectorBuf, L.sectorSize)) return 0;
    vtoc_set(vtocBuf, thisSector, 0);
    if (i == 0) firstSector = thisSector;
    written = (uint32_t)(written + chunk);

    thisSector = nextSector;
  }

  vcsJmp3(); vcsNop2n(200); /* plain ARM work below (entry field assignments + name/ext copy), no vcsLib call in between */
  entry[0] = TDIR_FLAG_INUSE;
  entry[1] = (uint8_t)(needSectors & 0xFFu);
  entry[2] = (uint8_t)(needSectors >> 8);
  entry[3] = (uint8_t)(firstSector & 0xFFu);
  entry[4] = (uint8_t)(firstSector >> 8);
  {
    uint8_t k;
    for (k = 0; k < TINYELF_NAME_LEN; k++) entry[5 + k] = (uint8_t)name8[k];
    for (k = 0; k < TINYELF_EXT_LEN; k++) entry[13 + k] = (uint8_t)ext3[k];
  }

  if (!dir_write(freeSlot, entry)) return 0;
  if (!vtoc_write(vtocBuf)) return 0;
  return 1;
}

uint8_t tinyelf_load(const char *name8, const char *ext3, void *data, uint32_t maxlen, uint32_t *outsize) {
  uint8_t *out = (uint8_t *)data;
  uint8_t entry[TINYELF_ENTRY_SIZE];
  uint8_t sectorBuf[TINYELF_MAX_SECTOR];
  uint32_t slot, total;
  uint16_t sector, guard, payload;

  if (!L.mounted) return 0;
  if (!find_entry(name8, ext3, &slot, entry)) return 0;

  sector = (uint16_t)(entry[3] | (entry[4] << 8));
  payload = (uint16_t)(L.sectorSize - 3);
  total = 0;

  for (guard = 0; sector != 0 && guard < L.totalSectors; guard++) {
    uint8_t bytesUsed;
    uint16_t nextSector, k;

    if (!read_sector_checked(sector, sectorBuf)) return 0;
    bytesUsed = sectorBuf[L.sectorSize - 3];
    if (bytesUsed > payload) return 0; /* corrupt control byte */
    nextSector = decode_next_sector(sectorBuf[L.sectorSize - 2], sectorBuf[L.sectorSize - 1]);

    if (total + bytesUsed > maxlen) return 0; /* doesn't fit */
    for (k = 0; k < bytesUsed; k++) { arm_pace(k); out[total + k] = sectorBuf[k]; }
    total += bytesUsed;

    sector = nextSector;
  }

  *outsize = total;
  return 1;
}

uint8_t tinyelf_dir_next(uint32_t *slot, char *name8, char *ext3, uint32_t *sectors) {
  uint32_t s = *slot;
  uint32_t n = max_files();
  uint8_t entry[TINYELF_ENTRY_SIZE];

  if (!L.mounted) return 0;

  while (s < n) {
    if (!dir_read(s, entry)) { *slot = s; return 0; }
    s++;
    if (tdir_entry_present(entry[0])) {
      uint8_t k;
      /* same ~11-byte-copy pacing gap as name_matches() - no vcsLib call
         in this loop itself. */
      vcsJmp3(); vcsNop2n(200);
      for (k = 0; k < TINYELF_NAME_LEN; k++) name8[k] = (char)entry[5 + k];
      for (k = 0; k < TINYELF_EXT_LEN; k++) ext3[k] = (char)entry[13 + k];
      *sectors = (uint32_t)(entry[1] | (entry[2] << 8));
      *slot = s;
      return 1;
    }
  }
  *slot = s;
  return 0;
}

uint32_t tinyelf_free_bytes(void) {
  uint8_t vtocBuf[TINYELF_MAX_SECTOR];
  uint16_t s, freeSectors = 0;

  /* Don't rely on the caller having just made a vcsLib call (CATALOG calls
     tinyelf_capacity_bytes() right before this one, which itself has no
     vcsLib calls at all) - every function accounts for its own entry. */
  vcsJmp3(); vcsNop2n(20);
  if (!L.mounted) return 0;
  if (!vtoc_read(vtocBuf)) return 0;

  /* Up to ~1000 iterations of plain ARM bit-testing for a large disk, no
     vcsLib call anywhere in the loop - same class of bug that crashed
     FORMAT before arm_pace() was added there. */
  for (s = L.dataStart; s < L.totalSectors; s++) {
    arm_pace((uint16_t)(s - L.dataStart));
    if (vtoc_get(vtocBuf, s)) freeSectors++;
  }
  return (uint32_t)freeSectors * (uint32_t)(L.sectorSize - 3);
}

uint32_t tinyelf_capacity_bytes(void) {
  /* Whole body is plain ARM work, zero I2C/vcsLib calls needed - unlike
     tinyelf_free_bytes(), this never has to run inside the disabled-
     blanking window at all (see xcatalog()'s STM32 branch in main.c,
     which deliberately calls this only after overblank is back on). No
     pacing needed here for that reason - don't add it back without also
     re-checking every call site stays outside the window. */
  if (!L.mounted) return 0;
  return (uint32_t)L.totalSectors * (uint32_t)L.sectorSize;
}

/* ---- streaming SAVE - see tinyelf_fs.h's comment for the public contract
   and the LIST/ENTER project plan for why this exists (a detokenized
   listing's total length isn't known upfront, and there's no second RAM
   buffer big enough to capture it all before saving). Module-static state,
   same pattern as L - only one stream can be open at a time, matching how
   only one drive can be mounted at a time already.

   NOTE (2026-09-12): re-added after a full revert following an unexplained
   boot crash when tinyelf_save_chunk()+tinyelf_save_end() were both wired
   into a caller (even an unreached one) - see project memory. Being
   re-introduced incrementally this time, one function at a time, with a
   real hardware boot test after each. ---- */
static struct {
  uint8_t  active;
  uint8_t  vtocBuf[TINYELF_MAX_SECTOR];
  uint32_t freeSlot;
  uint16_t scanCursor;
  uint16_t prevSector;   /* 0 = no sector written yet this stream */
  uint16_t firstSector;  /* 0 = not yet known (set on the first sector) */
  uint16_t sectorCount;
  char     name8[TINYELF_NAME_LEN];
  char     ext3[TINYELF_EXT_LEN];
} SS;

uint8_t tinyelf_save_begin(const char *name8, const char *ext3) {
  uint8_t entry[TINYELF_ENTRY_SIZE];
  uint32_t existingSlot;
  uint8_t k;

  SS.active = 0;
  if (!L.mounted) return 0;

  /* overwrite semantics, matching tinyelf_save() */
  if (find_entry(name8, ext3, &existingSlot, entry)) {
    if (!delete_entry(existingSlot, entry)) return 0;
  }
  if (!find_free_slot(&SS.freeSlot, entry)) return 0;
  if (!vtoc_read(SS.vtocBuf)) return 0;

  vcsJmp3(); vcsNop2n(200); /* plain ARM work below (11-byte copy), no vcsLib call in the loops themselves */
  for (k = 0; k < TINYELF_NAME_LEN; k++) SS.name8[k] = name8[k];
  for (k = 0; k < TINYELF_EXT_LEN; k++) SS.ext3[k] = ext3[k];

  SS.scanCursor = L.dataStart;
  SS.prevSector = 0;
  SS.firstSector = 0;
  SS.sectorCount = 0;
  SS.active = 1;
  return 1;
}

/* Allocates and writes ONE more sector (len bytes of real payload, 0 <
   len <= sectorSize-3, zero-padded past that). nextSector is written as
   the placeholder 0 - once the FOLLOWING sector is allocated (by the next
   call, or none if this was the last), the PREVIOUS sector's control
   bytes get patched in place with the real value (a 2-byte write, EEPROM
   is byte-addressable) - see tinyelf_fs.h's comment. The final sector of
   a stream is simply never patched, so it keeps nextSector=0 for free -
   no separate end-of-chain step needed. Same per-iteration pacing shape
   as tinyelf_save()'s own sector loop. */
static uint8_t save_stream_write_sector(const void *data, uint16_t len) {
  uint8_t sectorBuf[TINYELF_MAX_SECTOR];
  uint16_t payload = (uint16_t)(L.sectorSize - 3);
  uint16_t thisSector, k;
  const uint8_t *p = (const uint8_t *)data;

  while (SS.scanCursor < L.totalSectors && !vtoc_get(SS.vtocBuf, SS.scanCursor)) {
    arm_pace(SS.scanCursor);
    SS.scanCursor++;
  }
  if (SS.scanCursor >= L.totalSectors) return 0; /* out of space */
  thisSector = SS.scanCursor++;
  vtoc_set(SS.vtocBuf, thisSector, 0);

  vcsJmp3(); vcsNop2n(200);
  for (k = 0; k < len; k++) { arm_pace(k); sectorBuf[k] = p[k]; }
  for (; k < payload; k++) { arm_pace(k); sectorBuf[k] = 0; }
  sectorBuf[L.sectorSize - 3] = (uint8_t)len;
  sectorBuf[L.sectorSize - 2] = (uint8_t)(SS.freeSlot & 0x3Fu); /* nextSector placeholder = 0 */
  sectorBuf[L.sectorSize - 1] = 0;

  if (!write_retried((uint32_t)thisSector * L.sectorSize, sectorBuf, L.sectorSize)) return 0;

  if (SS.prevSector != 0) {
    uint8_t ctrl[2];
    ctrl[0] = (uint8_t)((SS.freeSlot & 0x3Fu) | (((thisSector >> 8) & 0x03u) << 6));
    ctrl[1] = (uint8_t)(thisSector & 0xFFu);
    if (!write_retried((uint32_t)SS.prevSector * L.sectorSize + (L.sectorSize - 2), ctrl, 2)) return 0;
  } else {
    SS.firstSector = thisSector;
  }

  SS.prevSector = thisSector;
  SS.sectorCount++;
  return 1;
}

uint8_t tinyelf_save_chunk(const void *data, uint16_t len) {
  if (!SS.active) return 0;
  if (len == 0 || len > (uint16_t)(L.sectorSize - 3)) return 0;
  if (!save_stream_write_sector(data, len)) { SS.active = 0; return 0; }
  return 1;
}

uint8_t tinyelf_save_end(void) {
  uint8_t entry[TINYELF_ENTRY_SIZE];
  uint8_t k;

  if (!SS.active) return 0;
  SS.active = 0;

  if (SS.sectorCount == 0) {
    uint8_t zero = 0;
    if (!save_stream_write_sector(&zero, 0)) return 0; /* empty save still gets one sector, matches tinyelf_save() */
  }

  vcsJmp3(); vcsNop2n(200); /* plain ARM work below (entry field assignments + name/ext copy), no vcsLib call in between */
  entry[0] = TDIR_FLAG_INUSE;
  entry[1] = (uint8_t)(SS.sectorCount & 0xFFu);
  entry[2] = (uint8_t)(SS.sectorCount >> 8);
  entry[3] = (uint8_t)(SS.firstSector & 0xFFu);
  entry[4] = (uint8_t)(SS.firstSector >> 8);
  for (k = 0; k < TINYELF_NAME_LEN; k++) entry[5 + k] = (uint8_t)SS.name8[k];
  for (k = 0; k < TINYELF_EXT_LEN; k++) entry[13 + k] = (uint8_t)SS.ext3[k];

  if (!dir_write(SS.freeSlot, entry)) return 0;
  if (!vtoc_write(SS.vtocBuf)) return 0;
  return 1;
}

uint16_t tinyelf_chunk_size(void) {
  if (!L.mounted) return 0;
  return (uint16_t)(L.sectorSize - 3);
}

/* ---- streaming LOAD - see tinyelf_fs.h's comment for the public
   contract. Module-static state, same pattern as L/SS above. */
static struct {
  uint8_t  active;
  uint16_t sector; /* next sector to read, 0 = end of chain */
  uint16_t count;  /* sectors read so far - a chain longer than the disk has a loop */
} LS;

uint8_t tinyelf_load_begin(const char *name8, const char *ext3) {
  uint8_t entry[TINYELF_ENTRY_SIZE];
  uint32_t slot;

  LS.active = 0;
  if (!L.mounted) return 0;
  if (!find_entry(name8, ext3, &slot, entry)) return 0;

  LS.sector = (uint16_t)(entry[3] | (entry[4] << 8));
  LS.count = 0;
  LS.active = 1;
  return 1;
}

uint16_t tinyelf_load_chunk(void *buf, uint16_t maxlen) {
  uint8_t sectorBuf[TINYELF_MAX_SECTOR];
  uint8_t *out = (uint8_t *)buf;
  uint8_t bytesUsed;
  uint16_t payload, nextSector, k;

  if (!LS.active || LS.sector == 0) { LS.active = 0; return 0; }

  /* A broken chain (a sector outside the data area, or more sectors than
     the disk has = a loop) must not be read forever: report it as an error
     (TINYELF_LOAD_ERROR), not as a normal end of file. */
  if (LS.sector < L.dataStart || LS.sector >= L.totalSectors || LS.count >= L.totalSectors) {
    LS.active = 0;
    return TINYELF_LOAD_ERROR;
  }
  LS.count++;

  payload = (uint16_t)(L.sectorSize - 3);
  if (!read_sector_checked(LS.sector, sectorBuf)) { LS.active = 0; return TINYELF_LOAD_ERROR; }
  bytesUsed = sectorBuf[L.sectorSize - 3];
  if (bytesUsed > payload || bytesUsed > maxlen) { LS.active = 0; return TINYELF_LOAD_ERROR; } /* corrupt control byte, or caller's buffer too small */
  nextSector = decode_next_sector(sectorBuf[L.sectorSize - 2], sectorBuf[L.sectorSize - 1]);

  for (k = 0; k < bytesUsed; k++) { arm_pace(k); out[k] = sectorBuf[k]; }

  LS.sector = nextSector;
  return bytesUsed;
}
