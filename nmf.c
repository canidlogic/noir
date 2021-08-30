/*
 * nmf.c
 * 
 * Implementation of nmf.h
 * 
 * See the header for further information.
 */

#include "nmf.h"
#include <stdlib.h>
#include <string.h>

/*
 * Constants
 * =========
 */

/*
 * Boundaries of the integer types used in the format.
 * 
 * For biased values, this maximum value is for the raw value stored in
 * the file, not for the final numeric value.
 */
#define NMF_MAXUINT32 (UINT32_C(2147483647))
#define NMF_MAXUINT16 (UINT16_C(65535))

#define NMF_MAXBIAS32 (UINT32_C(4294967295))
#define NMF_MAXBIAS16 (UINT16_C(65535))

/*
 * Biases used for the signed integers.
 */
#define NMF_BIAS32 (INT64_C(2147483648))
#define NMF_BIAS16 (INT32_C(32768))

/*
 * The signature values.
 */
#define NMF_SIGPRI (INT32_C(1928196216))
#define NMF_SIGSEC (INT32_C(1313818926))

/*
 * Initial allocation capacities, in elements, of the section and note
 * tables.
 */
#define NMF_SECTALLOC_INIT (16)
#define NMF_NOTEALLOC_INIT (256)

/*
 * Type definitions
 * ================
 */

/*
 * NMF_DATA structure.
 * 
 * Prototype given in header.
 */
struct NMF_DATA_TAG {
  
  /*
   * The number of sections.
   * 
   * Must be in range [1, NMF_MAXSECT].
   */
  int32_t sect_count;
  
  /*
   * The capacity of the section table.
   * 
   * Must be in range [1, NMF_MAXSECT].
   */
  int32_t sect_cap;
  
  /*
   * The number of notes.
   * 
   * Must be in range [1, NMF_MAXNOTE].
   */
  int32_t note_count;
  
  /*
   * The capacity of the note table.
   * 
   * Must be in range [1, NMF_MAXNOTE].
   */
  int32_t note_cap;
  
  /*
   * The quantum basis.
   * 
   * Must be one of the NMF_BASIS constants.
   */
  int32_t basis;
  
  /*
   * Pointer to the section table.
   * 
   * This is a dynamically allocated array with a number of elements
   * given by sect_cap and the total elements used given by sect_count.
   * 
   * Each element is the offset in quanta from the start of the
   * composition.
   * 
   * The first element must be zero, and all subsequent elements must
   * have an offset that is greater than or equal to the previous
   * element's offset.
   */
  int32_t *pSect;
  
  /*
   * Pointer to the note table.
   * 
   * This is a dynamically allocated array with a number of elements
   * given by note_cap and the total elements used given by note_count.
   * 
   * Time offsets of notes must not be less than the time offset of the
   * section that the note belongs to.
   */
  NMF_NOTE *pNote;
};

/*
 * Local functions
 * ===============
 */

/* Prototypes */
static void nmf_writeByte(FILE *pOut, int c);
static void nmf_writeUint32(FILE *pOut, uint32_t v);
static void nmf_writeUint16(FILE *pOut, uint16_t v);
static void nmf_writeBias32(FILE *pOut, int32_t v);
static void nmf_writeBias16(FILE *pOut, int16_t v);

static int nmf_readByte(FILE *pf);
static int nmf_read32(FILE *pf, uint32_t *pv);
static int nmf_read16(FILE *pf, uint16_t *pv);

static int32_t nmf_readUint32(FILE *pf);
static int nmf_readBias32(FILE *pf, int32_t *pv);
static int32_t nmf_readUint16(FILE *pf);
static int nmf_readBias16(FILE *pf, int16_t *pv);

static int nmf_cmp(const void *pva, const void *pvb);

/*
 * Write a byte to the output file.
 * 
 * pOut is the file to write to.  It must be open for writing or
 * undefined behavior occurs.
 * 
 * c is the unsigned byte value to write, in range [0, 255].
 * 
 * Parameters:
 * 
 *   pOut - the output file
 * 
 *   c - the unsigned byte value to write
 */
static void nmf_writeByte(FILE *pOut, int c) {
  
  /* Check parameters */
  if ((pOut == NULL) || (c < 0) || (c > 255)) {
    abort();
  }
  
  /* Write the byte */
  if (putc(c, pOut) != c) {
    abort();  /* I/O error */
  }
}

/*
 * Write an unsigned 32-bit integer value to the given file in big
 * endian order.
 * 
 * This is a wrapper around nmf_writeByte() which writes the integer
 * value as four bytes with most significant byte first and least
 * significant byte last.
 * 
 * Parameters:
 * 
 *   pOut - output file to write to
 * 
 *   v - the value to write
 */
static void nmf_writeUint32(FILE *pOut, uint32_t v) {
  
  nmf_writeByte(pOut, (int) (v >> 24));
  nmf_writeByte(pOut, (int) ((v >> 16) & 0xff));
  nmf_writeByte(pOut, (int) ((v >> 8) & 0xff));
  nmf_writeByte(pOut, (int) (v & 0xff));
}

/*
 * Write an unsigned 16-bit integer value to the given file in big
 * endian order.
 * 
 * This is a wrapper around nmf_writeByte() which writes the integer
 * value as two bytes with most significant byte first and least
 * significant byte second.
 * 
 * Parameters:
 * 
 *   pOut - output file to write to
 * 
 *   v - the value to write
 */
static void nmf_writeUint16(FILE *pOut, uint16_t v) {
  
  nmf_writeByte(pOut, (int) (v >> 8));
  nmf_writeByte(pOut, (int) (v & 0xff));
}

/*
 * Write a biased 32-bit signed integer value to the given file in big
 * endian order.
 * 
 * This is a wrapper around nmf_writeUint32() which writes a raw value
 * that is the given signed value plus NMF_BIAS32.  This biased value
 * must be in unsigned 32-bit range or a fault occurs.
 * 
 * Parameters:
 * 
 *   pOut - output file to write to
 * 
 *   v - the value to write
 */
static void nmf_writeBias32(FILE *pOut, int32_t v) {
  
  int64_t rv = 0;
  
  /* Compute raw value */
  rv = ((int64_t) v) + NMF_BIAS32;
  
  /* If inside unsigned 32-bit range, pass through; else, fault */
  if ((rv >= 0) && (rv <= UINT32_MAX)) {
    nmf_writeUint32(pOut, (uint32_t) rv);
  } else {
    abort();
  }
}

/*
 * Write a biased 16-bit signed integer value to the given file in big
 * endian order.
 * 
 * This is a wrapper around nmf_writeUint16() which writes a raw value
 * that is the given signed value plus NMF_BIAS16.  This biased value
 * must be in unsigned 16-bit range or a fault occurs.
 * 
 * Parameters:
 * 
 *   pOut - output file to write to
 * 
 *   v - the value to write
 */
static void nmf_writeBias16(FILE *pOut, int16_t v) {
  
  int32_t rv = 0;
  
  /* Compute raw value */
  rv = ((int32_t) v) + NMF_BIAS16;
  
  /* If inside unsigned 16-bit range, pass through; else, fault */
  if ((rv >= 0) && (rv <= UINT16_MAX)) {
    nmf_writeUint16(pOut, (uint16_t) rv);
  } else {
    abort();
  }
}

/*
 * Read an unsigned byte value from the given file.
 * 
 * pf is the file to read from.  It must be open for reading or
 * undefined behavior occurs.
 * 
 * The return value is the byte read, in range 0-255.  -1 is returned if
 * there was an I/O error or the End Of File (EOF) was encountered.
 * 
 * Parameters:
 * 
 *   pf - the file to read from
 * 
 * Return:
 * 
 *   the unsigned byte value read, or -1 if error (or EOF)
 */
static int nmf_readByte(FILE *pf) {
  
  int c = 0;
  
  /* Check parameter */
  if (pf == NULL) {
    abort();
  }
  
  /* Read a character */
  c = getc(pf);
  if (c == EOF) {
    c = -1;
  }
  
  /* Return the byte value (or error) */
  return c;
}

/*
 * Read a raw 32-bit integer value from a file.
 * 
 * pf is the file to read from.  It must be open for reading or
 * undefined behavior occurs.
 * 
 * pv points to the variable to receive the raw 32-bit integer value.
 * The value is undefined if the function fails.
 * 
 * Four bytes are read in big endian order, with the first byte read as
 * the most significant and the last byte read as the least significant.
 * 
 * Parameters:
 * 
 *   pf - the file to read from
 * 
 *   pv - the variable to receive the raw 32-bit integer value
 * 
 * Return:
 * 
 *   non-zero if successful, zero if error
 */
static int nmf_read32(FILE *pf, uint32_t *pv) {
  
  int i = 0;
  int status = 1;
  int c = 0;
  
  /* Check parameters */
  if ((pf == NULL) || (pv == NULL)) {
    abort();
  }
  
  /* Reset return value */
  *pv = 0;
  
  /* Read four bytes */
  for(i = 0; i < 4; i++) {
    
    /* Read a byte */
    c = nmf_readByte(pf);
    if (c < 0) {
      status = 0;
    }
    
    /* Shift result and add in new byte */
    if (status) {
      *pv <<= 8;
      *pv |= (uint32_t) c;
    }
    
    /* Leave loop if error */
    if (!status) {
      break;
    }
  }
  
  /* Return status */
  return status;
}

/*
 * Read a raw 16-bit integer value from a file.
 * 
 * pf is the file to read from.  It must be open for reading or
 * undefined behavior occurs.
 * 
 * pv points to the variable to receive the raw 16-bit integer value.
 * The value is undefined if the function fails.
 * 
 * Two bytes are read in big endian order, with the first byte read as
 * the most significant and the second byte read as the least
 * significant.
 * 
 * Parameters:
 * 
 *   pf - the file to read from
 * 
 *   pv - the variable to receive the raw 16-bit integer value
 * 
 * Return:
 * 
 *   non-zero if successful, zero if error
 */
static int nmf_read16(FILE *pf, uint16_t *pv) {
  
  int i = 0;
  int status = 1;
  int c = 0;
  
  /* Check parameters */
  if ((pf == NULL) || (pv == NULL)) {
    abort();
  }
  
  /* Reset return value */
  *pv = 0;
  
  /* Read two bytes */
  for(i = 0; i < 2; i++) {
    
    /* Read a byte */
    c = nmf_readByte(pf);
    if (c < 0) {
      status = 0;
    }
    
    /* Shift result and add in new byte */
    if (status) {
      *pv <<= 8;
      *pv |= (uint16_t) c;
    }
    
    /* Leave loop if error */
    if (!status) {
      break;
    }
  }
  
  /* Return status */
  return status;
}

/*
 * Read an unsigned 32-bit integer value from a file.
 * 
 * pf is the file to read from.  It must be open for reading or
 * undefined behavior occurs.
 * 
 * A raw 32-bit value is read in big endian order from the file.  The
 * numeric value must be in range [0, NMF_MAXUINT32].  The raw numeric
 * value is then returned as-is.
 * 
 * Parameters:
 * 
 *   pf - the file to read
 * 
 * Return:
 * 
 *   the integer that was read, or -1 if there was an error
 */
static int32_t nmf_readUint32(FILE *pf) {
  
  int status = 1;
  uint32_t rv = 0;
  int32_t retval = 0;
  
  /* Check parameter */
  if (pf == NULL) {
    abort();
  }
  
  /* Read a raw 32-bit value */
  if (!nmf_read32(pf, &rv)) {
    status = 0;
  }
  
  /* Range-check value */
  if (status) {
    if (rv > NMF_MAXUINT32) {
      status = 0;
    }
  }
  
  /* Determine return value */
  if (status) {
    retval = (int32_t) rv;
  } else {
    retval = -1;
  }
  
  /* Return value */
  return retval;
}

/*
 * Read a biased 32-bit integer value from a file.
 * 
 * pf is the file to read from.  It must be open for reading or
 * undefined behavior occurs.
 * 
 * pv points to the variable to receive the numeric value that was read.
 * Its value is undefined if the function fails.
 * 
 * A raw 32-bit value is read in big endian order from the file.  The
 * raw numeric value must be in range [1, NMF_MAXBIAS32].  The raw
 * numeric value is then subtracted by NMF_BIAS32 to yield the final
 * result, which is in range [-2,147,483,647, 2,147,483,647].
 * 
 * Parameters:
 * 
 *   pf - the file to read
 * 
 *   pv - the variable to receive the numeric value
 * 
 * Return:
 * 
 *   non-zero if successful, zero if error
 */
static int nmf_readBias32(FILE *pf, int32_t *pv) {
  
  int status = 1;
  uint32_t rv = 0;
  int64_t vv = 0;
  
  /* Check parameters */
  if ((pf == NULL) || (pv == NULL)) {
    abort();
  }
  
  /* Read a raw 32-bit value */
  if (!nmf_read32(pf, &rv)) {
    status = 0;
  }
  
  /* Range-check value */
  if (status) {
    if ((rv < 1) || (rv > NMF_MAXBIAS32)) {
      status = 0;
    }
  }
  
  /* Bias value to get final result */
  if (status) {
    vv = ((int64_t) rv) - NMF_BIAS32;
  }
  
  /* Set result */
  if (status) {
    *pv = (int32_t) vv;
  }
  
  /* Return status */
  return status;
}

/*
 * Read an unsigned 16-bit integer value from a file.
 * 
 * pf is the file to read from.  It must be open for reading or
 * undefined behavior occurs.
 * 
 * A raw 16-bit value is read in big endian order from the file.  The
 * numeric value must be in range [0, NMF_MAXUINT16].  The raw numeric
 * value is then returned as-is.
 * 
 * Parameters:
 * 
 *   pf - the file to read
 * 
 * Return:
 * 
 *   the integer that was read, or -1 if there was an error
 */
static int32_t nmf_readUint16(FILE *pf) {
  
  int status = 1;
  uint16_t rv = 0;
  int32_t retval = 0;
  
  /* Check parameter */
  if (pf == NULL) {
    abort();
  }
  
  /* Read a raw 16-bit value */
  if (!nmf_read16(pf, &rv)) {
    status = 0;
  }
  
  /* Range-check value */
  if (status) {
    if (rv > NMF_MAXUINT16) {
      status = 0;
    }
  }
  
  /* Determine return value */
  if (status) {
    retval = (int32_t) rv;
  } else {
    retval = -1;
  }
  
  /* Return value */
  return retval;
}

/*
 * Read a biased 16-bit integer value from a file.
 * 
 * pf is the file to read from.  It must be open for reading or
 * undefined behavior occurs.
 * 
 * pv points to the variable to receive the numeric value that was read.
 * Its value is undefined if the function fails.
 * 
 * A raw 16-bit value is read in big endian order from the file.  The
 * raw numeric value must be in range [1, NMF_MAXBIAS16].  The raw
 * numeric value is then subtracted by NMF_BIAS16 to yield the final
 * result, which is in range [-32,767, 32,767].
 * 
 * Parameters:
 * 
 *   pf - the file to read
 * 
 *   pv - the variable to receive the numeric value
 * 
 * Return:
 * 
 *   non-zero if successful, zero if error
 */
static int nmf_readBias16(FILE *pf, int16_t *pv) {
  
  int status = 1;
  uint16_t rv = 0;
  int32_t vv = 0;
  
  /* Check parameters */
  if ((pf == NULL) || (pv == NULL)) {
    abort();
  }
  
  /* Read a raw 16-bit value */
  if (!nmf_read16(pf, &rv)) {
    status = 0;
  }
  
  /* Range-check value */
  if (status) {
    if ((rv < 1) || (rv > NMF_MAXBIAS16)) {
      status = 0;
    }
  }
  
  /* Bias value to get final result */
  if (status) {
    vv = ((int32_t) rv) - NMF_BIAS16;
  }
  
  /* Set result */
  if (status) {
    *pv = (int16_t) vv;
  }
  
  /* Return status */
  return status;
}

/*
 * Compare two note events to each other.
 * 
 * The comparison is less than, equals, or greater than according to the
 * time offsets of the note events.  Grace notes with equal time offsets
 * are sorted according to grace offset in sequential order, and grace
 * notes come before the main beat.
 * 
 * The function is designed to be used with the qsort() function of the
 * standard library.  The pointers are to NMF_NOTE structures.
 * 
 * Parameters:
 * 
 *   pva - void pointer to the first note event
 * 
 *   pvb - void pointer to the second note event
 * 
 * Return:
 * 
 *   less than, equal to, or greater than zero as note event a is less
 *   than, equal to, or greater than note event b
 */
static int nmf_cmp(const void *pva, const void *pvb) {
  
  int result = 0;
  const NMF_NOTE *pa = NULL;
  const NMF_NOTE *pb = NULL;
  
  /* Check parameters */
  if ((pva == NULL) || (pvb == NULL)) {
    abort();
  }
  
  /* Get pointers to note events a and b */
  pa = (const NMF_NOTE *) pva;
  pb = (const NMF_NOTE *) pvb;
  
  /* Compare time offsets */
  if (pa->t < pb->t) {
    /* First t less than second t */
    result = -1;
    
  } else if (pa->t > pb->t) {
    /* First t greater than second t */
    result = 1;
    
  } else if (pa->t == pb->t) {
    /* Both events have same t -- check for grace notes */
    if ((pa->dur < 0) && (pb->dur < 0)) {
      /* Both are grace notes */
      if (pa->dur < pb->dur) {
        result = -1;
      
      } else if (pa->dur > pb->dur) {
        result = 1;
      
      } else if (pa->dur == pb->dur) {
        result = 0;
      
      } else {
        abort();  /* shouldn't happen */
      }
    
    } else if (pa->dur < 0) {
      /* A is a grace note, B is not */
      result = -1;
    
    } else if (pb->dur < 0) {
      /* B is a grace note, A is not */
      result = 1;
    
    } else {
      /* Neither are grace notes */
      result = 0;
    }
    
  } else {
    /* Shouldn't happen */
    abort();
  }
  
  /* Return result */
  return result;
}

/*
 * Public function implementations
 * ===============================
 * 
 * See the header for specifications.
 */

/*
 * nmf_alloc function.
 */
NMF_DATA *nmf_alloc(void) {
  
  NMF_DATA *pd = NULL;
  
  /* Allocate data structure */
  pd = (NMF_DATA *) malloc(sizeof(NMF_DATA));
  if (pd == NULL) {
    abort();
  }
  memset(pd, 0, sizeof(NMF_DATA));
  
  /* Allocate the section and note tables with initial capacities */
  pd->pSect = (int32_t *) calloc(NMF_SECTALLOC_INIT, sizeof(int32_t));
  if (pd->pSect == NULL) {
    abort();
  }
  
  pd->pNote = (NMF_NOTE *) calloc(NMF_NOTEALLOC_INIT, sizeof(NMF_NOTE));
  if (pd->pNote == NULL) {
    abort();
  }
  
  /* Set the initial state */
  pd->sect_count = 1;
  pd->sect_cap = NMF_SECTALLOC_INIT;
  pd->note_count = 0;
  pd->note_cap = NMF_NOTEALLOC_INIT;
  pd->basis = (int32_t) NMF_BASIS_Q96;
  (pd->pSect)[0] = 0;
  
  /* Return the new object */
  return pd;
}

/*
 * nmf_parse function.
 */
NMF_DATA *nmf_parse(FILE *pf) {
  
  int status = 1;
  int32_t i = 0;
  int32_t v = 0;
  NMF_DATA *pd = NULL;
  NMF_NOTE *pn = NULL;
  
  /* Read the primary and secondary signatures */
  if (nmf_readUint32(pf) != NMF_SIGPRI) {
    status = 0;
  }
  if (status) {
    if (nmf_readUint32(pf) != NMF_SIGSEC) {
      status = 0;
    }
  }
  
  /* Allocate a parser structure */
  if (status) {
    pd = (NMF_DATA *) malloc(sizeof(NMF_DATA));
    if (pd == NULL) {
      abort();
    }
    memset(pd, 0, sizeof(NMF_DATA));
    pd->pSect = NULL;
    pd->pNote = NULL;
  }
  
  /* Read the quantum basis */
  if (status) {
    pd->basis = nmf_readUint16(pf);
    if ((pd->basis != NMF_BASIS_Q96) &&
        (pd->basis != NMF_BASIS_44100) &&
        (pd->basis != NMF_BASIS_48000)) {
      status = 0;
    }
  }
  
  /* Read the section count and note count */
  if (status) {
    pd->sect_count = nmf_readUint16(pf);
    if ((pd->sect_count < 1) || (pd->sect_count > NMF_MAXSECT)) {
      status = 0;
    }
  }
  if (status) {
    pd->note_count = nmf_readUint32(pf);
    if ((pd->note_count < 1) || (pd->note_count > NMF_MAXNOTE)) {
      status = 0;
    }
  }
  
  /* Set the capacities equal to the counts */
  if (status) {
    pd->sect_cap = pd->sect_count;
    pd->note_cap = pd->note_count;
  }
  
  /* Allocate the section and note arrays */
  if (status) {
    pd->pSect = (int32_t *) calloc(
                              (size_t) pd->sect_count,
                              sizeof(int32_t));
    if (pd->pSect == NULL) {
      abort();
    }
    
    pd->pNote = (NMF_NOTE *) calloc(
                              (size_t) pd->note_count,
                              sizeof(NMF_NOTE));
    if (pd->pNote == NULL) {
      abort();
    }
  }
  
  /* Read the section table */
  if (status) {
    for(i = 0; i < pd->sect_count; i++) {
      
      /* Read the section value */
      (pd->pSect)[i] = nmf_readUint32(pf);
      if ((pd->pSect)[i] < 0) {
        status = 0;
      }
      
      /* If this is first section, it must have value of zero; else, it
       * must have a value greater than or equal to previous section */
      if (status && (i < 1)) {
        if ((pd->pSect)[i] != 0) {
          status = 0;
        }
      } else if (status) {
        if ((pd->pSect)[i] < (pd->pSect)[i - 1]) {
          status = 0;
        }
      }
      
      /* Leave loop if error */
      if (!status) {
        break;
      }
    }
  }
  
  /* Read the note table */
  if (status) {
    for(i = 0; i < pd->note_count; i++) {
      
      /* Get pointer to current note */
      pn = &((pd->pNote)[i]);
      
      /* Read the fields of the current note */
      pn->t = nmf_readUint32(pf);
      if (pn->t < 0) {
        status = 0;
      }
      
      if (status) {
        if (!nmf_readBias32(pf, &(pn->dur))) {
          status = 0;
        }
      }
      
      if (status) {
        if (!nmf_readBias16(pf, &(pn->pitch))) {
          status = 0;
        }
      }
      
      if (status) {
        v = nmf_readUint16(pf);
        if (v >= 0) {
          pn->art = (uint16_t) v;
        } else {
          status = 0;
        }
      }
      
      if (status) {
        v = nmf_readUint16(pf);
        if (v >= 0) {
          pn->sect = (uint16_t) v;
        } else {
          status = 0;
        }
      }
      
      if (status) {
        v = nmf_readUint16(pf);
        if (v >= 0) {
          pn->layer_i = (uint16_t) v;
        } else {
          status = 0;
        }
      }
      
      /* Range-check fields of current note */
      if (status) {
        if (pn->t < 0) {
          status = 0;
        }
        if ((pn->pitch < NMF_MINPITCH) || (pn->pitch > NMF_MAXPITCH)) {
          status = 0;
        }
        if (pn->art > NMF_MAXART) {
          status = 0;
        }
        if (pn->sect >= pd->sect_count) {
          status = 0;
        }
      }
      
      /* Make sure time offset is greater than or equal to start of
       * note's section */
      if (status) {
        if (pn->t < (pd->pSect)[pn->sect]) {
          status = 0;
        }
      }
      
      /* Leave loop if error */
      if (!status) {
        break;
      }
    }
  }
  
  /* If failure, make sure structure free */
  if ((!status) && (pd != NULL)) {
    if (pd->pSect != NULL) {
      free(pd->pSect);
    }
    if (pd->pNote != NULL) {
      free(pd->pNote);
    }
    free(pd);
    pd = NULL;
  }
  
  /* Return pointer to new structure, or NULL */
  return pd;
}

/*
 * nmf_parse_path function.
 */
NMF_DATA *nmf_parse_path(const char *pPath) {
  
  FILE *fp = NULL;
  NMF_DATA *pResult = NULL;
  
  /* Check parameter */
  if (pPath == NULL) {
    abort();
  }
  
  /* Open the file for reading */
  fp = fopen(pPath, "rb");
  if (fp != NULL) {
    /* Sucessfully opened file, so pass through and close file */
    pResult = nmf_parse(fp);
    fclose(fp);
    fp = NULL;
  
  } else {
    /* Didn't open file successfully, so fail */
    pResult = NULL;
  }
  
  /* Return result */
  return pResult;
}

/*
 * nmf_free function.
 */
void nmf_free(NMF_DATA *pd) {
  if (pd != NULL) {
    free(pd->pSect);
    free(pd->pNote);
    free(pd);
  }
}

/*
 * nmf_basis function.
 */
int nmf_basis(NMF_DATA *pd) {
  if (pd == NULL) {
    abort();
  }
  return (int) pd->basis;
}

/*
 * nmf_sections function.
 */
int32_t nmf_sections(NMF_DATA *pd) {
  if (pd == NULL) {
    abort();
  }
  return pd->sect_count;
}

/*
 * nmf_notes function.
 */
int32_t nmf_notes(NMF_DATA *pd) {
  if (pd == NULL) {
    abort();
  }
  return pd->note_count;
}

/*
 * nmf_offset function.
 */
int32_t nmf_offset(NMF_DATA *pd, int32_t sect_i) {
  if (pd == NULL) {
    abort();
  }
  if ((sect_i < 0) || (sect_i >= pd->sect_count)) {
    abort();
  }
  return (pd->pSect)[sect_i];
}

/*
 * nmf_get function.
 */
void nmf_get(NMF_DATA *pd, int32_t note_i, NMF_NOTE *pn) {
  if ((pd == NULL) || (pn == NULL)) {
    abort();
  }
  if ((note_i < 0) || (note_i >= pd->note_count)) {
    abort();
  }
  memcpy(pn, &((pd->pNote)[note_i]), sizeof(NMF_NOTE));
}

/*
 * nmf_set function.
 */
void nmf_set(NMF_DATA *pd, int32_t note_i, const NMF_NOTE *pn) {
  
  /* Check parameters */
  if ((pd == NULL) || (pn == NULL)) {
    abort();
  }
  if ((note_i < 0) || (note_i >= pd->note_count)) {
    abort();
  }
  
  /* Check fields of note parameter */
  if (pn->dur < -(INT32_MAX)) {
    abort();
  }
  if ((pn->pitch < NMF_MINPITCH) || (pn->pitch > NMF_MAXPITCH)) {
    abort();
  }
  if (pn->art > NMF_MAXART) {
    abort();
  }
  if (pn->sect >= pd->sect_count) {
    abort();
  }
  if (pn->t < nmf_offset(pd, pn->sect)) {
    abort();
  }
  
  /* Copy new note data in */
  memcpy(&((pd->pNote)[note_i]), pn, sizeof(NMF_NOTE));
}

/*
 * nmf_sect function.
 */
int nmf_sect(NMF_DATA *pd, int32_t offset) {
  
  int status = 1;
  int32_t newcap = 0;
  
  /* Check parameters */
  if ((pd == NULL) || (offset < 0)) {
    abort();
  }
  
  /* Make sure offset is greater than or equal to offset of current last
   * section (the first section is defined automatically right away, so
   * there will always be at least one section) */
  if (offset < nmf_offset(pd, pd->sect_count - 1)) {
    abort();
  }
  
  /* Only proceed if room for another section; otherwise, fail */
  if (pd->sect_count < NMF_MAXSECT) {
    
    /* We have room for another section -- check if capacity of section
     * table needs to be expanded */
    if (pd->sect_count >= pd->sect_cap) {
      
      /* We need to expand capacity -- first, try doubling */
      newcap = pd->sect_cap * 2;
      
      /* If doubled capacity is less than initial allocation, set to
       * initial allocation */
      if (newcap < NMF_SECTALLOC_INIT) {
        newcap = NMF_SECTALLOC_INIT;
      }
      
      /* If doubling exceeds maximum section count, set to maximum
       * section count */
      if (newcap > NMF_MAXSECT) {
        newcap = NMF_MAXSECT;
      }
      
      /* Reallocate with the new capacity */
      pd->pSect = (int32_t *) realloc(
                          pd->pSect,
                          newcap * sizeof(int32_t));
      if (pd->pSect == NULL) {
        abort();
      }
      memset(
          &((pd->pSect)[pd->sect_cap]),
          0,
          (newcap - pd->sect_cap) * sizeof(int32_t));
      
      /* Update capacity variable */
      pd->sect_cap = newcap;
    }
    
    /* Add the new section to the table */
    (pd->pSect)[pd->sect_count] = offset;
    (pd->sect_count)++;
    
  } else {
    /* Section table is full */
    status = 0;
  }
  
  /* Return status */
  return status;
}

/*
 * nmf_append function.
 */
int nmf_append(NMF_DATA *pd, const NMF_NOTE *pn) {
  
  int status = 1;
  int32_t newcap = 0;
  
  /* Check parameters */
  if ((pd == NULL) || (pn == NULL)) {
    abort();
  }
  
  /* Check fields of note parameter */
  if (pn->dur < -(INT32_MAX)) {
    abort();
  }
  if ((pn->pitch < NMF_MINPITCH) || (pn->pitch > NMF_MAXPITCH)) {
    abort();
  }
  if (pn->art > NMF_MAXART) {
    abort();
  }
  if (pn->sect >= pd->sect_count) {
    abort();
  }
  if (pn->t < nmf_offset(pd, pn->sect)) {
    abort();
  }
  
  /* Only proceed if room for another note; otherwise, fail */
  if (pd->note_count < NMF_MAXNOTE) {
    
    /* We have room for another note -- check if capacity of note table
     * needs to be expanded */
    if (pd->note_count >= pd->note_cap) {
      
      /* We need to expand capacity -- first, try doubling */
      newcap = pd->note_cap * 2;
      
      /* If doubling is less than initial capacity, set to initial
       * capacity */
      if (newcap < NMF_NOTEALLOC_INIT) {
        newcap = NMF_NOTEALLOC_INIT;
      }
      
      /* If doubling exceeds maximum note count, set to maximum note
       * count */
      if (newcap > NMF_MAXNOTE) {
        newcap = NMF_MAXNOTE;
      }
      
      /* Reallocate with the new capacity */
      pd->pNote = (NMF_NOTE *) realloc(
                          pd->pNote,
                          newcap * sizeof(NMF_NOTE));
      if (pd->pNote == NULL) {
        abort();
      }
      memset(
          &((pd->pNote)[pd->note_cap]),
          0,
          (newcap - pd->note_cap) * sizeof(NMF_NOTE));
      
      /* Update capacity variable */
      pd->note_cap = newcap;
    }
    
    /* Add the new note to the table */
    memcpy(&((pd->pNote)[pd->note_count]), pn, sizeof(NMF_NOTE));
    (pd->note_count)++;
    
  } else {
    /* Note table is full */
    status = 0;
  }
  
  /* Return status */
  return status;
}

/*
 * nmf_rebase function.
 */
void nmf_rebase(NMF_DATA *pd, int basis) {
  
  /* Check parameters */
  if (pd == NULL) {
    abort();
  }
  if ((basis != NMF_BASIS_Q96) &&
      (basis != NMF_BASIS_44100) &&
      (basis != NMF_BASIS_48000)) {
    abort();
  }
  
  /* Change the basis */
  pd->basis = (int32_t) basis;
}

/*
 * nmf_sort function.
 */
void nmf_sort(NMF_DATA *pd) {
  
  /* Check parameter */
  if (pd == NULL) {
    abort();
  }
  
  /* Only proceed if at least two notes */
  if (pd->note_count > 1) {
  
    /* Sort the array of note events */
    qsort(
        (void *) pd->pNote,
        (size_t) pd->note_count,
        sizeof(NMF_NOTE),
        &nmf_cmp);
  }
}

/*
 * nmf_serialize function.
 */
int nmf_serialize(NMF_DATA *pd, FILE *pf) {
  
  int status = 1;
  int32_t i = 0;
  NMF_NOTE *pn = NULL;
  
  /* Check parameters */
  if ((pd == NULL) || (pf == NULL)) {
    abort();
  }
  
  /* Only proceed if at least one note */
  if (pd->note_count > 0) {
    
    /* Write the NMF signatures */
    nmf_writeUint32(pf, (uint32_t) NMF_SIGPRI);
    nmf_writeUint32(pf, (uint32_t) NMF_SIGSEC);
    
    /* Write the quantum basis */
    nmf_writeUint16(pf, (uint16_t) pd->basis);
    
    /* Write the section count and note counts */
    nmf_writeUint16(pf, (uint16_t) pd->sect_count);
    nmf_writeUint32(pf, (uint32_t) pd->note_count);
    
    /* Write the section table */
    for(i = 0; i < pd->sect_count; i++) {
      nmf_writeUint32(pf, (uint32_t) ((pd->pSect)[i]));
    }
    
    /* Write each note */
    for(i = 0; i < pd->note_count; i++) {
      
      /* Get pointer to current note */
      pn = &((pd->pNote)[i]);
      
      /* Serialize the structure */
      nmf_writeUint32(pf, (uint32_t) pn->t);
      nmf_writeBias32(pf, pn->dur);
      nmf_writeBias16(pf, pn->pitch);
      nmf_writeUint16(pf, pn->art);
      nmf_writeUint16(pf, pn->sect);
      nmf_writeUint16(pf, pn->layer_i);
    }
  
  } else {
    /* No notes have been defined */
    status = 0;
  }
  
  /* Return status */
  return status;
}
