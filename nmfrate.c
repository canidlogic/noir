/*
 * nmfrate.c
 * =========
 * 
 * Convert a Noir Music File (NMF) with a quantum basis of 96 quanta per
 * quarter note to a fixed-duration quanta basis using a constant tempo.
 * 
 * Syntax
 * ------
 * 
 *   nmfrate [srate] [tempo] [qbeat]
 * 
 * [srate] is the fixed rate to use, which must be either 48000 or
 * 44100.
 * 
 * [tempo] is the constant tempo to use, given in beats per 10 minutes
 * (which is ten times the Beat Per Minute BPM rate).
 * 
 * [qbeat] is the number of quanta in a beat.
 * 
 * Operation
 * ---------
 * 
 * An NMF file is read from standard input, which must have a quantum
 * basis of 96 quanta per quarter note.
 * 
 * The t and duration values of each event are computed using the new
 * basis, and the NMF file with constant-rate quanta is written to
 * standard output.
 * 
 * Grace note durations are left as-is without modification.
 * 
 * Compilation
 * -----------
 * 
 * Compile with nmf.
 * 
 * May also need to be compiled with the math library -lm
 */

#include <math.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "nmf.h"

/*
 * Local functions
 * ===============
 */

/* Prototypes */
static int parseInt(const char *pstr, int32_t *pv);

/*
 * Parse the given string as a signed integer.
 * 
 * pstr is the string to parse.
 * 
 * pv points to the integer value to use to return the parsed numeric
 * value if the function is successful.
 * 
 * In two's complement, this function will not successfully parse the
 * least negative value.
 * 
 * Parameters:
 * 
 *   pstr - the string to parse
 * 
 *   pv - pointer to the return numeric value
 * 
 * Return:
 * 
 *   non-zero if successful, zero if failure
 */
static int parseInt(const char *pstr, int32_t *pv) {
  
  int negflag = 0;
  int32_t result = 0;
  int status = 1;
  int32_t d = 0;
  
  /* Check parameters */
  if ((pstr == NULL) || (pv == NULL)) {
    abort();
  }
  
  /* If first character is a sign character, set negflag appropriately
   * and skip it */
  if (*pstr == '+') {
    negflag = 0;
    pstr++;
  } else if (*pstr == '-') {
    negflag = 1;
    pstr++;
  } else {
    negflag = 0;
  }
  
  /* Make sure we have at least one digit */
  if (*pstr == 0) {
    status = 0;
  }
  
  /* Parse all digits */
  if (status) {
    for( ; *pstr != 0; pstr++) {
    
      /* Make sure in range of digits */
      if ((*pstr < '0') || (*pstr > '9')) {
        status = 0;
      }
    
      /* Get numeric value of digit */
      if (status) {
        d = (int32_t) (*pstr - '0');
      }
      
      /* Multiply result by 10, watching for overflow */
      if (status) {
        if (result <= INT32_MAX / 10) {
          result = result * 10;
        } else {
          status = 0; /* overflow */
        }
      }
      
      /* Add in digit value, watching for overflow */
      if (status) {
        if (result <= INT32_MAX - d) {
          result = result + d;
        } else {
          status = 0; /* overflow */
        }
      }
    
      /* Leave loop if error */
      if (!status) {
        break;
      }
    }
  }
  
  /* Invert result if negative mode */
  if (status && negflag) {
    result = -(result);
  }
  
  /* Write result if successful */
  if (status) {
    *pv = result;
  }
  
  /* Return status */
  return status;
}

/*
 * Program entrypoint
 * ==================
 */

int main(int argc, char *argv[]) {
  
  int status = 1;
  int32_t x = 0;
  const char *pModule = NULL;
  
  int32_t srate = 0;
  int32_t tempo = 0;
  int32_t qbeat = 0;
  
  NMF_DATA *pd = NULL;
  NMF_DATA *pdo = NULL;
  NMF_NOTE n;
  
  double qdur = 0.0;
  double f = 0.0;
  int32_t notes = 0;
  int32_t sections = 0;
  int32_t newval = 0;
  
  /* Initialize structures */
  memset(&n, 0, sizeof(NMF_NOTE));
  
  /* Get module name */
  if (argc > 0) {
    if (argv != NULL) {
      if (argv[0] != NULL) {
        pModule = argv[0];
      }
    }
  }
  if (pModule == NULL) {
    pModule = "nmfrate";
  }
  
  /* We need exactly three parameters past module name */
  if (argc != 4) {
    status = 0;
    fprintf(stderr, "%s: Wrong number of parameters!\n", pModule);
  }
  
  /* Make sure arguments are present */
  if (status) {
    if (argv == NULL) {
      abort();
    }
    for(x = 0; x < argc; x++) {
      if (argv[x] == NULL) {
        abort();
      }
    }
  }
  
  /* Parse the arguments */
  if (status) {
    if (!parseInt(argv[1], &srate)) {
      status = 0;
      fprintf(stderr, "%s: Can't parse srate parameter!\n", pModule);
    }
  }
  
  if (status) {
    if (!parseInt(argv[2], &tempo)) {
      status = 0;
      fprintf(stderr, "%s: Can't parse tempo parameter!\n", pModule);
    }
  }
  
  if (status) {
    if (!parseInt(argv[3], &qbeat)) {
      status = 0;
      fprintf(stderr, "%s: Can't parse qbeat parameter!\n", pModule);
    }
  }
  
  /* Range-check parameters */
  if (status && (srate != 48000) && (srate != 44100)) {
    status = 0;
    fprintf(stderr, "%s: Invalid sampling rate!\n", pModule);
  }
  
  if (status && (tempo < 1)) {
    status = 0;
    fprintf(stderr, "%s: Invalid tempo!\n", pModule);
  }
  
  if (status && (qbeat < 1)) {
    status = 0;
    fprintf(stderr, "%s: Invalid beat!\n", pModule);
  }
  
  /* Parse input as NMF */
  if (status) {
    pd = nmf_parse(stdin);
    if (pd == NULL) {
      status = 0;
      fprintf(stderr, "%s: Can't parse input as NMF!\n", pModule);
    }
  }
  
  /* Allocate blank object for transformed data and rebase it */
  if (status) {
    pdo = nmf_alloc();
    if (srate == 48000) {
      nmf_rebase(pdo, NMF_BASIS_48000);
    
    } else if (srate == 44100) {
      nmf_rebase(pdo, NMF_BASIS_44100);
      
    } else {
      abort();  /* shouldn't happen */
    }
  }
  
  /* Compute the duration of each quanta in the target sample rate */
  if (status) {
    qdur = ((600.0 / ((double) tempo)) * ((double) srate)) /
              ((double) qbeat);
  }
  
  /* Transfer sections, adjusting their offsets */
  if (status) {
    sections = nmf_sections(pd);
    for(x = 1; x < sections; x++) {
      
      /* Compute new section offset */
      f = qdur * ((double) nmf_offset(pd, x));
      if (!isfinite(f)) {
        status = 0;
        fprintf(stderr, "%s: Computation error!\n", pModule);
      }
      if (status) {
        if ((f < (double) INT32_MIN) || (f > (double) INT32_MAX)) {
          status = 0;
          fprintf(stderr, "%s: Computation error!\n", pModule);
        }
      }
      if (status) {
        newval = (int32_t) f;
        if (newval < 0) {
          newval = 0;
        }
      }
      
      /* Output new section */
      if (!nmf_sect(pdo, newval)) {
        abort();  /* shouldn't happen */
      }
    }
  }
  
  
  /* Transfer notes, adjusting t and dur */
  if (status) {
    notes = nmf_notes(pd);
    for(x = 0; x < notes; x++) {
      
      /* Get the current note */
      nmf_get(pd, x, &n);
      
      /* Compute the new offset of the note */
      f = qdur * ((double) n.t);
      if (!isfinite(f)) {
        status = 0;
        fprintf(stderr, "%s: Computation error!\n", pModule);
      }
      if (status) {
        if ((f < (double) INT32_MIN) || (f > (double) INT32_MAX)) {
          status = 0;
          fprintf(stderr, "%s: Computation error!\n", pModule);
        }
      }
      if (status) {
        newval = (int32_t) f;
        if (newval < 0) {
          newval = 0;
        }
      }
      if (status) {
        n.t = newval;
      }
      
      /* If duration of note is greater than zero, recompute it */
      if (n.dur > 0) {
        f = qdur * ((double) n.dur);
        if (!isfinite(f)) {
          status = 0;
          fprintf(stderr, "%s: Computation error!\n", pModule);
        }
        if (status) {
          if ((f < (double) INT32_MIN) || (f > (double) INT32_MAX)) {
            status = 0;
            fprintf(stderr, "%s: Computation error!\n", pModule);
          }
        }
        if (status) {
          newval = (int32_t) f;
          if (newval < 1) {
            newval = 1;
          }
        }
        if (status) {
          n.dur = newval;
        }
      }
      
      /* Transfer note to new file */
      if (status) {
        if (!nmf_append(pdo, &n)) {
          abort();  /* shouldn't happen */
        }
      }
      
      /* Leave loop if error */
      if (!status) {
        break;
      }
    }
  }
  
  /* Serialize the data to output */
  if (status) {
    if (!nmf_serialize(pdo, stdout)) {
      abort();  /* shouldn't happen */
    }
  }
  
  /* Free data if allocated */
  nmf_free(pd);
  pd = NULL;
  nmf_free(pdo);
  pdo = NULL;
  
  /* Invert status and return */
  if (status) {
    status = 0;
  } else {
    status = 1;
  }
  return status;
}
