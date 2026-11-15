/*
 * i2c_savekey.c
 *
 * See i2c_savekey.h. Ported from i2c_v2.3.inc (Alex Herbert 2004, optimised
 * by Thomas Jentzsch 2006/2008), which bit-bangs I2C over the joystick port
 * via SWCHA/SWACNT in raw 6507 assembly. Here every "lda #imm; sta abs" pair
 * from the original becomes one vcsWrite6() bus-stuffed call, in the same
 * order, with the same immediate values - including a few (e.g. writing
 * 0x18 where you might expect 0x08) that look odd out of context but are
 * harmless: they land an extra bit on a pin that is still configured as an
 * input at that exact point (SWACNT hasn't been updated yet), so the extra
 * bit has no electrical effect. Preserved verbatim rather than "cleaned up"
 * since that subtlety is very hard to get wrong-but-still-working, and
 * there's no way to test this port without real hardware.
 *
 * Unlike the original, there is no JSR/RTS here at all - every ASM
 * subroutine just becomes a plain C function, since this all runs as
 * ARM-side bus-stuffing rather than injected 6507 machine code.
 */
#include "i2c_savekey.h"
#include "vcsLib.h"

#define I2C_SDA_MASK 0x04u
#define I2C_SCL_MASK 0x08u

/* KeyPortari's chip-enable line (bit7/D7 of the same SWCHA/SWACNT byte the
   ASCII protocol and this I2C driver share). Active-high on KeyPortari's
   side: normally left released (input here, pulled high), and driving it
   LOW disables KeyPortari immediately via its own IRQ - no settle time
   needed (replaces the old i2c_wait_settle() blind ~30-frame wait, see
   its former definition in main.c). Folded into every SWACNT write the
   low-level primitives below make, so it stays asserted for the whole
   transaction (SWACNT is a whole-byte register - a write that omits this
   bit would release CE again mid-transaction). i2c_start() is always the
   first real bus op of any window, so asserting CE here is automatic; the
   final SWACNT=0x00 in i2c_stop()/i2c_release_bus() releases it again the
   same way it releases SDA/SCL. Requires current (7-bit protocol)
   KeyPortari firmware - no fallback for older firmware without this line. */
#define KEYPORTARI_CE_MASK 0x80u

/* I2C_START macro, extended with a preamble the original i2c_v2.3.inc
   version doesn't need: in the original driver, I2C_START is only ever
   used as the very first operation of a transaction (bus otherwise idle).
   This was also briefly used as a REPEATED start (no i2c_stop() first),
   right after reading an ACK bit - see i2c_eeprom_read()'s comment for
   why that turned out to be a bad idea (a real hardware slave keeps
   holding SDA low until it sees SCL fall, and Gopher2600's SaveKey
   emulation doesn't release it at all in that case) and was reverted to
   a real stop+start. i2c_eeprom_read()/write() now only ever call this
   right after i2c_stop() or as the very first op of a transaction, but
   the explicit "drop SCL and release SDA first" preamble below is kept
   regardless - cheap, and still correct/defensive for that case too. */
void i2c_start(void) {
  vcsJmp3();
  vcsWrite6(SWCHA, 0x00);          /* SCL data=0, CE data=0 */
  vcsWrite6(SWACNT, I2C_SCL_MASK | KEYPORTARI_CE_MASK); /* SCL output(low), SDA input(released), CE output(low) - disables KeyPortari */
  vcsJmp3();
  vcsWrite6(SWCHA, I2C_SCL_MASK);  /* SCL high, SDA still input/floating high */
  vcsJmp3();
  vcsWrite6(SWCHA, (uint8_t)((I2C_SCL_MASK | I2C_SDA_MASK) * 2));
  vcsWrite6(SWACNT, I2C_SCL_MASK | I2C_SDA_MASK | KEYPORTARI_CE_MASK); /* drive SDA low too - the actual START edge */
}

/* I2C_TXBIT macro - drives SDA to 'bit' while SCL is low, then pulses SCL
   high to latch it. SDA=1 is sent by releasing the pin (SWACNT bit clear,
   pulled high externally); SDA=0 is sent by driving it (SWACNT bit set,
   SWCHA data bit already 0 from the first write below).

   A push-pull variant of this (always driving SDA as output, for both
   bit values) was tried, reasoning from Gopher2600's actual RIOT-ports
   source (hardware/riot/ports/ports.go) that a released/input bit only
   gets a correct "high" value for one transient instant there, easy to
   miss across many bits. That change, combined with other changes made
   around the same time, made things WORSE on the emulator (neither read
   nor write worked at all afterward, vs. reads at least not corrupting
   data before) - reverted along with the rest back to this last
   confirmed-working point. Worth revisiting with real trace data instead
   of further hand-derivation.

   Every one of these low-level primitives (this, i2c_scl_pulse_sda_released,
   i2c_txack, i2c_start) starts with a vcsJmp3() - a long FORMAT/SAVE
   transaction is many thousands of vcsWrite6()/vcsRead6() calls, and
   without frequent JMPs the injected 6507 "PC" drifts out of ROM range and
   crashes (seen on real hardware, first with the GRAPHICS 3 playfield
   kernel, then again here - once per byte turned out not to be often
   enough, so now it's once per bit/primitive call instead. vcsJmp3() is
   only 3 cycles, and I2C has no maximum clock period, so the extra delay
   this adds is free from a protocol-correctness point of view.) */
static void i2c_txbit(uint8_t bit) {
  vcsJmp3();
  vcsWrite6(SWCHA, 0x00);
  vcsWrite6(SWACNT, (uint8_t)((bit ? I2C_SCL_MASK : (I2C_SCL_MASK | I2C_SDA_MASK)) | KEYPORTARI_CE_MASK));
  vcsWrite6(SWCHA, I2C_SCL_MASK);
}

/* I2C_TXNACK macro - releases SDA (as input) and pulses SCL. Used both to
   actually transmit a NACK and, doubly, as the "release SDA and clock it"
   step when reading a bit back (see i2c_read_one_bit below) - that's the
   original driver's naming, not a copy/paste mistake. */
static void i2c_scl_pulse_sda_released(void) {
  vcsJmp3();
  vcsWrite6(SWCHA, (uint8_t)(I2C_SCL_MASK * 2));
  vcsWrite6(SWACNT, I2C_SCL_MASK | KEYPORTARI_CE_MASK);
  vcsNop2(); /* required for timing, per original source comment */
  vcsWrite6(SWCHA, I2C_SCL_MASK);
}

/* i2c_txack subroutine - drives SDA low for one clock pulse (an ACK bit).
   Also doubles as the bus-release/STOP step at the end of a transaction,
   exactly as the original driver uses it in i2c_stopwrite. */
static void i2c_txack(void) {
  vcsJmp3();
  vcsWrite6(SWCHA, 0x00);
  vcsWrite6(SWACNT, I2C_SCL_MASK | I2C_SDA_MASK | KEYPORTARI_CE_MASK);
  vcsWrite6(SWCHA, (uint8_t)((I2C_SCL_MASK | I2C_SDA_MASK) * 2));
}

/* reads one bit off SDA (used for both data bits and the ack/nack bit).
   vcsRead6() is the old vcsRead4() + vcsNop2() pair in one call (the
   firmware injects the NOP right after the read) - reads are timing-critical
   in a way plain writes aren't (per direct hardware guidance), so nothing may
   go between the read and its nop. vcsRead6 is also what the Chameleon Cart
   requires. */
static uint8_t i2c_read_one_bit(void) {
  uint8_t v;
  i2c_scl_pulse_sda_released(); /* has its own vcsJmp3(), see above */
  v = vcsRead6(SWCHA); /* = old vcsRead4() + vcsNop2(): the firmware injects the NOP itself */
  return (uint8_t)((v >> 2) & 1u);
}

/* Tried keeping SCL actively driven across stop+restart here (releasing
   only SDA, matching https://www.bitbanging.space/posts/bitbang-i2c's
   start()/stop() structure) on the theory that i2c_v2.3.inc's original
   "release both together" (i2c_stopwrite, right before its own rts)
   coupled SCL+SDA into one transition that confused Gopher2600's SaveKey
   emulation. Made things WORSE on the emulator (both read and write
   broke, previously read alone worked) - reverted. In hindsight the
   actual coupled transition Gopher2600 seems to trip over isn't here at
   all: it's the FIRST write of the *following* i2c_start(), which is a
   direct SWCHA write, and Gopher2600 masks any direct SWCHA write by the
   *current* SWACNT (see i2c_txbit()'s comment) - so for SDA, which is
   still input either way at that point, the written value gets masked
   to 0 regardless of what we intend, coupling SCL's intentional drop
   with an unintended SDA drop right there - keeping SCL "output" for
   longer beforehand doesn't change that. Back to releasing both here;
   i2c_release_bus() (below) is now redundant with this but harmless -
   kept so main.c's call sites don't need touching again. */
void i2c_stop(void) {
  i2c_txack();             /* matches i2c_stopwrite's "jsr i2c_txack" */
  vcsWrite6(SWACNT, 0x00); /* return port to input mode - releases the bus, including CE (KeyPortari resumes) */
}

/* Redundant with i2c_stop() now (see its comment) - kept as a real
   function (not removed) so main.c's call sites, added when this was
   still doing something different, don't need to be touched again. */
void i2c_release_bus(void) {
  vcsWrite6(SWACNT, 0x00);
}

/* i2c_txbyte subroutine - transmits one byte MSB-first, returns the slave's
   ack/nack bit (0 = ACK, 1 = NAK). */
uint8_t i2c_txbyte(uint8_t data) {
  int8_t bit;
  for (bit = 7; bit >= 0; bit--) {
    i2c_txbit((uint8_t)((data >> bit) & 1u)); /* has its own vcsJmp3() */
  }
  return i2c_read_one_bit();
}

/* i2c_rxbyte subroutine - receives one byte MSB-first. Pass ack=1 if more
   bytes will follow (sends ACK so the slave keeps clocking out data),
   ack=0 for the last byte of a read (sends NAK instead). */
uint8_t i2c_rxbyte(uint8_t ack) {
  uint8_t data = 0;
  uint8_t bit;
  for (bit = 0; bit < 8; bit++) {
    data = (uint8_t)((data << 1) | i2c_read_one_bit());
  }
  if (ack) {
    i2c_txack();
  } else {
    (void)i2c_read_one_bit(); /* NAK: one more released clock pulse, value discarded */
  }
  return data;
}

/* sends START + device address (7-bit devaddr7, rw=0 write/1 read).
   Single attempt, matching Thomas Jentzsch's reference usage exactly
   (SetupSaveKey: "jsr i2c_startwrite; bcc .noSKfound" - one try, no
   retry loop at all). An earlier version of this function busy-polled up
   to a large fixed count, on the (unverified, invented) theory
   that the EEPROM needs to be waited out after a write - that retry loop
   doesn't exist anywhere in the original driver or its example, and
   reads were completely broken the whole time it was in here. Returns 1
   if the device ACKed, 0 if not. */
static uint8_t i2c_addr_ack_poll(uint8_t devaddr7, uint8_t rw) {
  i2c_start();
  return i2c_txbyte((uint8_t)((devaddr7 << 1) | rw)) == 0;
}

uint32_t i2c_eeprom_write(uint8_t devaddr7, uint16_t memaddr, const void *data, uint32_t len) {
  const uint8_t *p = (const uint8_t *)data;
  uint32_t written = 0;

  while (written < len) {
    uint16_t addr = (uint16_t)(memaddr + written);
    uint16_t pageRemain = (uint16_t)(I2C_EEPROM_PAGE_SIZE - (addr % I2C_EEPROM_PAGE_SIZE));
    uint32_t chunk = len - written;
    uint32_t i;
    uint16_t poll;
    uint8_t acked = 0;

    if (chunk > pageRemain) chunk = pageRemain;

    /* ACK-poll for the *previous* write's completion - see
       I2C_WRITE_ACK_POLL_MAX's comment in i2c_savekey.h. Tight loop, no
       delay: each attempt is short enough that even the full bound
       finishes well within one frame. */
    for (poll = 0; poll < I2C_WRITE_ACK_POLL_MAX && !acked; poll++) {
      acked = i2c_addr_ack_poll(devaddr7, 0);
    }
    if (!acked) break; /* device stuck busy - give up */

    i2c_txbyte((uint8_t)(addr >> 8));
    i2c_txbyte((uint8_t)(addr & 0xFF));
    for (i = 0; i < chunk; i++) {
      i2c_txbyte(p[written + i]);
    }
    i2c_stop();
    written += chunk;
  }
  return written;
}

uint32_t i2c_eeprom_read(uint8_t devaddr7, uint16_t memaddr, void *data, uint32_t len) {
  uint8_t *p = (uint8_t *)data;
  uint32_t i;

  if (len == 0) return 0;

  /* Back to a real STOP + fresh START between the "fake write" address-set
     and the read, matching Thomas Jentzsch's reference
     (example_i2c_Savekey.asm: i2c_stopwrite then i2c_startread) - an
     earlier version of this function used a REPEATED start instead
     (i2c_start() again without a stop in between), because on real
     hardware a full stop+start used to fail ACK outright while repeated
     start at least got an ACK, just with data reading back as constant 0
     from that point on. That symptom turned out to be two now-fixed bugs
     (the still-settling RETURN keypress that used to need the old
     i2c_wait_settle() blind wait, since replaced by the KeyPortari
     chip-enable line - see KEYPORTARI_CE_MASK above - and the missing
     write-cycle ACK-poll in i2c_eeprom_write()) - not a flaw in stop+start
     itself. Confirmed via Gopher2600's own SaveKey emulation source
     (hardware/peripherals/savekey/savekey.go): its Step() function never
     releases SDA when SCL falls (real EEPROMs do - i2c_start()'s comment
     used to describe relying on exactly that), so a repeated start's SDA
     transition-while-SCL-already-high goes undetected there, desyncing
     its bit counter by one and explaining the constant-0 reads on the
     emulator too. A real stop+start doesn't depend on that release-on-fall
     behavior at all, so it should be correct on both real hardware and
     the emulator. */
  if (!i2c_addr_ack_poll(devaddr7, 0)) return 0;
  i2c_txbyte((uint8_t)(memaddr >> 8));
  i2c_txbyte((uint8_t)(memaddr & 0xFF));

  i2c_stop();
  /* The ACK of the read-address byte is deliberately not evaluated (the
     reference driver, example_i2c_Savekey.asm, ignores it too): Gopher2600
     reports it as a NAK. The device is known to be present - the address-set
     transaction above was ACKed, and callers validate what they read. */
  (void)i2c_addr_ack_poll(devaddr7, 1);
  for (i = 0; i < len; i++) {
    p[i] = i2c_rxbyte((uint8_t)(i + 1 < len));
  }
  i2c_stop();
  return len;
}
