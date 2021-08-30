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
   * The number of notes.
   * 
   * Must be in range [1, NMF_MAXNOTE].
   */
  int32_t note_count;
  
  /*
   * Pointer to the section table.
   * 
   * This is a dynamically allocated array with a number of elements
   * given by sect_count.
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
   * given by note_count.
   * 
   * The notes must be sorted primarily by ascending time offset, and
   * then have a secondary sort of ascending duration.
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
static int nmf_readByte(FILE *pf);
static int nmf_read32(FILE *pf, uint32_t *pv);
static int nmf_read16(FILE *pf, uint16_t *pv);

static int32_t nmf_readUint32(FILE *pf);
static int nmf_readBias32(FILE *pf, int32_t *pv);
static int32_t nmf_readUint16(FILE *pf);
static int nmf_readBias16(FILE *pf, int16_t *pv);

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
 * Public function implementations
 * ===============================
 * 
 * See the header for specifications.
 */

/*
 * nmf_parse function.
 */
NMF_DATA *nmf_parse(FILE *pf) {
  
  int status = 1;
  int32_t i = 0;
  int32_t v = 0;
  NMF_DATA *pd = NULL;
  NMF_NOTE *pn = NULL;
  NMF_NOTE *pnp = NULL;
  
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
  
  /* Read the section count and note count */
  if (status) {
    pd->sect_count = nmf_readUint32(pf);
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
      
      /* Get pointer to current note and previous note (unless first
       * note) */
      pn = &((pd->pNote)[i]);
      if (i > 0) {
        pnp = &((pd->pNote)[i - 1]);
      }
      
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
      
      /* If not the first note, make sure it is ordered correctly */
      if (status && (i > 0)) {
        if (pn->t < pnp->t) {
          /* t offsets out of order */
          status = 0;
        
        } else if (pn->t == pnp->t) {
          if (pn->dur < pnp->dur) {
            /* durations out of order */
            status = 0;
          }
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
