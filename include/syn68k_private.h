/* This file contains information needed by both the compile time and runtime
 * portions of the 68k emulator.
 */

#ifndef _syn68k_private_h_
#define _syn68k_private_h_

/* 	$Id: syn68k_private.h.in 72 2004-12-28 15:57:12Z ctm $	 */

#include "syn68k_public.h"
#include <signal.h>

#define ALL_CCS 0x1F

/* Note: these are _not_ the order in which the bits appear in the ccr.
 * For historical reasons, we keep track of them alphabetically.
 */
#define M68K_CC_NONE 0
#define M68K_CCC     0x10
#define M68K_CCN     0x8
#define M68K_CCV     0x4
#define M68K_CCX     0x2
#define M68K_CCZ     0x1
#define M68K_CC_ALL  0x1F

/* Shorthand for some common combinations. */
#define M68K_CC_CNVZ  (M68K_CCC | M68K_CCN | M68K_CCV |            M68K_CCZ)
#define M68K_CC_CNVXZ (M68K_CCC | M68K_CCN | M68K_CCV | M68K_CCX | M68K_CCZ)


/* For machines with few registers, it is worth our while to put the 68000
 * registers into an array and random access them.
 *
 * It's been a while since this was defined -- if it's not defined, we might
 * not be able to build.
 */
# define M68K_REGS_IN_ARRAY

/* If a machine rounds all divisions towards zero and the remainder always
 * has the same sign as the dividend, #define this for better speed.  It
 * should work even if you leave it undefined though.  If you have problems
 * with divide, try undef'ing this to use the slower but safer version.
 */
#if defined(mc68000) || defined(i386) || defined(__alpha) || defined (powerpc) || defined (__ppc__) || defined(__x86_64)
# define M68K_DIVISION_BEHAVIOR
#endif

#if	defined(__alpha)
/* #define TEMPS_AT_TOP */
#endif


/* Use faster threaded code if the needed gcc extensions are present.*/
#if defined (__GNUC__ ) && (__GNUC__ > 2 || ( __GNUC__ == 2 && __GNUC_MINOR__ >= 4)) && !defined (NONDIRECT)
# define USE_DIRECT_DISPATCH
#endif


/* #define this if support exists for generating native code in some
 * situations.
 */
#if defined (USE_DIRECT_DISPATCH) && defined(i386) && !defined (NONNATIVE)
# define GENERATE_NATIVE_CODE
#endif


#if defined (SYNCHRONOUS_INTERRUPTS) && !defined (GENERATE_NATIVE_CODE)
/* This is easy to fix, but I have more pressing things to work on.
 * The problem is that, without native code, there is no synthetic opcode
 * at the beginning of each block that checks for an interrupt.
 * The proper way to rectify this is to return to the good old days
 * when all synthetic opcodes that could branch backwards would check
 * for an interrupt.  With native code, we check at the beginning of
 * each block, not on branches (it's much easier that way), so you do
 * NOT want to check for interrupts on backwards synthetic branches
 * if GENERATE_NATIVE_CODE is defined.
 *
 * NOTE: I (ctm) fixed this, although I don't guarantee that my fix is
 * as fast as Mat's would have been. 
 */
#endif


#ifdef USE_DIRECT_DISPATCH
extern const void **direct_dispatch_table;
extern void init_dispatch_table();
#endif


#ifdef SYNCHRONOUS_INTERRUPTS
# define BLOCK_INTERRUPTS(save) (save = 0)
# define RESTORE_INTERRUPTS(save)
#else  /* !SYNCHRONOUS_INTERRUPTS */
# ifdef MSDOS
extern int dos_block_interrupts (void);
extern void dos_restore_interrupts (int);
#  define BLOCK_INTERRUPTS(save) (save = dos_block_interrupts ())
#  define RESTORE_INTERRUPTS(save) dos_restore_interrupts (save)
# else  /* Not MSDOS */
#  define SIGNALS_TO_BLOCK (sigmask(SIGALRM) | sigmask(SIGURG) \
	 		  | sigmask(SIGVTALRM) | sigmask(SIGIO))
#  define BLOCK_INTERRUPTS(save) (save = sigblock (SIGNALS_TO_BLOCK));
#  define RESTORE_INTERRUPTS(save) sigsetmask (save)
# endif /* Not MSDOS */
#endif  /* !SYNCHRONOUS_INTERRUPTS */

/* Data types. */
#if	!defined(FALSE) && !defined(TRUE)
typedef enum { FALSE, TRUE, NO = 0, YES = 1 } BOOL;
#else	/* defined(FALSE) || defined(TRUE) */
typedef enum { NO = 0, YES = 1 } BOOL;
#endif	/* defined(FALSE) || defined(TRUE) */

#ifndef NULL
#define NULL ((void *)0)
#endif

#define PTR_BYTES (sizeof (void *))
#define PTR_WORDS (PTR_BYTES / sizeof (uint16))
#define OPCODE_BYTES PTR_BYTES
#define OPCODE_WORDS PTR_WORDS

/* Many 68k opcodes have useful operands which will not be implicit in the
 * synthetic opcode.  The translator will extract these operands, translate
 * them to a useful form, and place them in the interpreted instruction
 * stream (in order).  This struct describes both the location of these
 * operands in the 68k instruction stream and how to translate them to
 * a form useful to the interpreter.  Only 32 bit, word aligned bitfields
 * are allowed to span multiple words.  Note: foo.index == MAGIC_END_INDEX
 * && foo.length == MAGIC_END_LENGTH implies the end of the bitfield list.
 */
#define MAGIC_END_INDEX 126
#define MAGIC_END_LENGTH 30
#define IS_TERMINATING_BITFIELD(p) ((p)->index == MAGIC_END_INDEX \
				    && (p)->length == MAGIC_END_LENGTH)
typedef struct {
  uint16 rev_amode   :1;  /* If 1, swap high 3 bits w/low 3 bits for 6 bit # */
  uint16 index       :7;  /* # of bits into instruction for this bitfield.   */
  uint16 length      :5;  /* Length, in bits, of this bitfield, minus 1.     */
  uint16 sign_extend :1;  /* Boolean: sign extend the resulting number?      */
  uint16 make_native_endian :1;  /*   Swap bytes if we're little endian?     */
  uint16 words       :1;  /* # of words to expand this number into, minus 1. */
} BitfieldInfo;


#define MAX_BITFIELDS 4

/* This struct contains information needed to map from 68k opcodes to
 * synthetic cpu opcodes.  Typically you would have one of these structs
 * for each cc bit variant of each 68k opcode, although of course the
 * point of the design of this structure is that sequences of these can
 * be shared by many distinct 68k opcode bit patterns.
 */
typedef struct {
  uint32 sequence_parity    :1;   /* Identifies contiguous blocks of these.  */

  /* CC bit information. */
  uint32 cc_may_set         :5;   /* CC bits not necessarily left alone.     */
  uint32 cc_may_not_set     :5;   /* CC bits not necessarily changed.        */
  uint32 cc_needed          :5;   /* CC bits which must be valid for this.   */

  /* 68k opcode information. */
  uint32 instruction_words  :4;   /* # of 16 bit words taken by 68k opcode.  */
  uint32 ends_block         :1;   /* Boolean: does this end a basic block?   */
  uint32 next_block_dynamic :1;   /* Boolean: next block known runtime only. */

  /* Are there addressing modes, and did we expand them? */
  uint32 amode_size              :2;   /* 0:none, 1:byte, 2:word, 3:long.    */
  uint32 reversed_amode_size     :2;   /* 0:none, 1:byte, 2:word, 3:long.    */
  uint32 amode_expanded          :1;   /* If has amode, was it expanded?     */
  uint32 reversed_amode_expanded :1;   /* If has reversed_amode, " " " ?     */

  /* Bit pattern mapping information.  This lets us compute the synthetic
   * opcode from the 68k opcode.
   */
  uint32 opcode_shift_count :4;   /* # of bits by which to >> 68k opcode.    */
  uint16 opcode_and_bits;         /* Mask with which to & 68k opcode.        */
  uint16 opcode_add_bits;         /* Bits to add to 68k opcode.              */

  /* Actual bitfield extraction/translation information. */
  BitfieldInfo bitfield[MAX_BITFIELDS]; /* Bitfields to parse & translate.   */

#ifdef GENERATE_NATIVE_CODE
# ifdef SYNGEN
  const char *guest_code_descriptor;   /* We only know its name here. */
# else   /* !SYNGEN */
  const struct _guest_code_descriptor_t *guest_code_descriptor;
# endif  /* !SYNGEN */
#endif /* GENERATE_NATIVE_CODE */
} OpcodeMappingInfo;


#ifndef QUADALIGN
# define READUL_UNSWAPPED(addr) (*(uint32 *) SYN68K_TO_US(addr))
# define READUL_UNSWAPPED_US(addr) (*(uint32 *) (addr))
#else
# ifdef BIGENDIAN
#  define READUL_UNSWAPPED(addr) \
({ const uint16 *_p = (const uint16 *)SYN68K_TO_US(addr); (uint32)(_p[0] << 16) | _p[1]; })
#  define READUL_UNSWAPPED_US(addr) \
({ const uint16 *_p = (const uint16 *)(addr); (uint32)(_p[0] << 16) | _p[1]; })
# else

#if	defined(__GNUC__)

typedef struct { uint32 l __attribute__((packed)); } hidden_l;

#define READUL_UNSWAPPED(addr)   (((hidden_l *)(SYN68K_TO_US(addr)))->l)

#define READUL_UNSWAPPED_US(addr)   (((hidden_l *)(addr))->l)

#else
static uint32 _readul_unswapped(const uint16 *p)
{
    return (uint32)(p[1] << 16) | p[0];
}

#define READUL_UNSWAPPED(addr) _readul_unswapped((const uint16 *) (SYN68K_TO_US(addr)))
#define READUL_UNSWAPPED_US(addr) _readul_unswapped((const uint16 *) (addr))
#endif

# endif
#endif
#define READSL_UNSWAPPED(addr) ((int32) READUL_UNSWAPPED (addr))

#ifndef QUADALIGN
# define WRITEUL_UNSWAPPED(p,v) (*(uint32 *)(p) = (v))
#else
# ifdef BIGENDIAN
#  define WRITEUL_UNSWAPPED(p, v)  \
   ({                              \
     uint32 _tmp = (v);            \
     uint16 *_x = (uint16 *)(p);   \
     _x[0] = _tmp >> 16;           \
     _x[1] = _tmp;                 \
     _tmp;                         \
   })
# else

#if	defined(__GNUC__)
#define WRITEUL_UNSWAPPED(addr, v)   (((hidden_l *)(addr))->l = (v))
#else
static uint32 _writeul_unswapped(uint16 *p, uint32 tmp)
{
    p[0] = tmp;
    p[1] = tmp >> 16;
    return tmp;
}
#define WRITEUL_UNSWAPPED(p, v) _writeul_unswapped((uint16 *) (p), (v))
#endif

# endif
#endif

#define WRITESL_UNSWAPPED(p,v) ((int32) WRITEUL_UNSWAPPED (p, v))


/* FIXME - could be more efficient going one byte at a time on
 * LITTLEENDIAN QUADALIGN machines.
 */
#define WRITE_LONG(p,v) (WRITEUL_UNSWAPPED ((p), SWAPUL_IFLE (v)))
#define WRITE_WORD(p,v) ((void) (*(uint16 *)(p) = SWAPUW_IFLE (v)))

#ifndef QUADALIGN
# define WRITE_PTR(p,v) (*(char **)(p) = (char *)(v))
#else
# define WRITE_PTR(p,v) \
{ void *ptr = (v); memcpy ((uint16 *)(p), &ptr, sizeof ptr); }
#endif

#define COMPUTE_SR_FROM_CPU_STATE() \
  (cpu_state.sr | (cpu_state.ccc != 0) | ((cpu_state.ccv != 0) << 1) \
   | ((cpu_state.ccnz == 0) << 2) | ((cpu_state.ccn != 0) << 3) \
   | ((cpu_state.ccx != 0) << 4))


/* #if	!defined(__alpha) */
#if SIZEOF_CHAR_P != 8
#define	DEREF(typ, addr)	(*(typ *)addr)
#else

typedef struct {  int8  n __attribute__((packed)); } HIDDEN_int8;
typedef struct { uint8  n __attribute__((packed)); } HIDDEN_uint8;
typedef struct {  int16 n __attribute__((packed)); } HIDDEN_int16;
typedef struct { uint16 n __attribute__((packed)); } HIDDEN_uint16;
typedef struct {  int32 n __attribute__((packed)); } HIDDEN_int32;
typedef struct { uint32 n __attribute__((packed)); } HIDDEN_uint32;

#define	DEREF(typ, addr)	(((HIDDEN_ ## typ *)addr)->n)
#endif

#endif  /* Not _syn68k_private_h_ */
