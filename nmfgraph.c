/*
 * nmfgraph.c
 * ==========
 * 
 * Convert a special Noir Music File (NMF) encoding into layer graphs
 * that can be used with the Retro synthesizer.
 * 
 * Syntax
 * ------
 * 
 *   nmfgraph ([gamma])
 * 
 * [gamma] is an optional gamma value used for scaling dynamics to
 * intensity levels.  It must be an integer greater than zero that
 * represents the gamma value multiplied by 1,000, such that 1000 is a
 * gamma value of 1.0.  If not provided, a gamma value of 1.0 is
 * assumed.
 * 
 * An NMF file is read from standard input, and a sequence of Retro
 * layers are written as text to standard output.  See the operation
 * section for further information.
 * 
 * Operation
 * ---------
 * 
 * The NMF file must have a quantum basis of 48,000 or 44,100 quanta per
 * second.  You can use regular NMF utilities such as nmfrate to convert
 * an NMF graph file with a 96 quanta per quarter basis into a
 * fixed-rate basis.  In order for the graphs to be synchronized with
 * the music, use the same operations that are used to convert the
 * quantum basis of the main music.
 * 
 * Within the NMF file in graph format, sections and section numbers are
 * ignored.  In contrast to standard NMF, layer numbers in graph NMF are
 * the same across all sections.  This allows graphs to go across
 * section boundaries.  The rest of this section describes graphs as the
 * composite across all sections, rather than being bound to a
 * particular section.
 * 
 * When an NMF note is encountered with a particular layer ID, nmfgraph
 * will create a graph builder for that layer ID if one has not yet been
 * constructed.  All note events will be routed to the graph builder of
 * their particular layer, sorted in chronological order.
 * 
 * The articulation of a note indicates the particular function:
 * 
 *   ARTICULATION '0':  Constant dynamic
 *   ARTICULATION '1':  Dynamic ramp (cresc. or dim.)
 *   ARTICULATION 'A':  Set high bits of multiplier
 *   ARTICULATION 'B':  Set low bits of multiplier
 * 
 * Articulation keys 'A' and 'B' must have a pitch in numeric range
 * zero up to and including 31.  They are used to set the five most
 * significant or five least significant bits of a 10-bit register that
 * stores one less than the intensity multiplier for the layer (range
 * one up to and including 1024).  For each layer, either exactly one
 * note with articulation key 'A' is present somewhere in the file and
 * exactly one note with articulation key 'B' is present somewhere in
 * the file, or neither articulation key is present for that layer in
 * the file, in which case the multiplier is assumed to be 1024.  The
 * time offsets and durations of notes with articulation keys 'A' and
 * 'B' are irrelevant and ignored.
 * 
 * All other notes must have articulation keys '0' or '1', indicating
 * constant dynamics or dynamic ramps.  Within each layer, the following
 * rules must be observed.
 * 
 *   (1) There must be a dynamic at time t=0.
 * 
 *   (2) The last dynamic must be constant.
 * 
 *   (3) Before any ramp dynamic, there may optionally be a single grace
 *       note at offset -1, which must have articulation key '1'.
 * 
 *   (4) The pitches of each dynamic and grace note must be in the range
 *       [-7, 7], which is F up to g with middle c in the middle.
 * 
 *   (5) There may be at most one dynamic at each t, except for the
 *       special case of grace notes given in (3) above.
 * 
 * Durations of notes are irrelevant and ignored, except for grace note
 * offsets.  The length of ramps is always up to the next dynamic.
 * 
 * For ramps that have a grace note before them, the grace note gives
 * the starting intensity of the ramp and the main note gives the ending
 * intensity of the ramp.  For ramps that have no grace note, the ending
 * intensity is the starting intensity of the next dynamic.
 * 
 * The pitch range [-7, 7] is mapped linearly to range [0, 1.0] (where
 * middle C is then mapped to 0.5).  Then, gamma correction is
 * optionally applied to get a corrected range [0, 1.0].  This is then
 * mapped to integer range [0, 1024] to get the output intensity.
 * 
 * Compilation
 * -----------
 * 
 * Compile with nmf.
 * 
 * The math library may also be required with -lm
 */

#include <math.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "nmf.h"

/*
 * Constants
 * =========
 */

/*
 * Articulation keys.
 */
#define ARTKEY_CONSTANT   (0)   /* Constant dynamic */
#define ARTKEY_RAMP       (1)   /* Ramp dynamic */
#define ARTKEY_HIGH_MUL   (10)  /* Set high multiplier */
#define ARTKEY_LOW_MUL    (11)  /* Set low multiplier */

/*
 * Multiplier pitch range.
 */
#define MUL_MINPITCH (0)
#define MUL_MAXPITCH (31)

/*
 * Dynamic pitch range.
 */
#define DYN_MINPITCH (-7)
#define DYN_MAXPITCH (7)

/*
 * Type definitions
 * ================
 */

/*
 * A dynamic record.
 */
typedef struct {
  
  /* @@TODO: */
  
} DYNREC;

/*
 * Local functions
 * ===============
 */

/* Prototypes */
static void layerInit(int32_t layer_i);
static int layerSetMul(int32_t layer_i, int32_t val, int high);
static int layerHasGrace(int32_t layer_i);
static int32_t layerGraceTime(int32_t layer_i);
static int layerFirstNote(int32_t layer_i);
static int32_t layerLastTime(int32_t layer_i);
static int layerDynC(int32_t layer_i, int32_t t, int32_t val);
static void layerGrace(int32_t layer_i, int32_t t, int32_t val);
static int layerDynR(int32_t layer_i, int32_t t, int32_t val);

static int layerDanglingMul(void);
static int layerDanglingGrace(void);
static int layerEmpty(void);

static int writeLayers(FILE *pf);

static int parseInt(const char *pstr, int32_t *pv);

/* @@TODO: */
static void layerInit(int32_t layer_i) {
  /* @@TODO: */
}
static int layerSetMul(int32_t layer_i, int32_t val, int high) {
  /* @@TODO: */
  return 1;
}
static int layerHasGrace(int32_t layer_i) {
  /* @@TODO: */
  return 0;
}
static int32_t layerGraceTime(int32_t layer_i) {
  /* @@TODO: */
  return 0;
}
static int layerFirstNote(int32_t layer_i) {
  /* @@TODO: */
  return 0;
}
static int32_t layerLastTime(int32_t layer_i) {
  /* @@TODO: */
  return 0;
}
static int layerDynC(int32_t layer_i, int32_t t, int32_t val) {
  /* @@TODO: */
  return 1;
}
static void layerGrace(int32_t layer_i, int32_t t, int32_t val) {
  /* @@TODO: */
}
static int layerDynR(int32_t layer_i, int32_t t, int32_t val) {
  /* @@TODO: */
  return 1;
}
static int layerDanglingMul(void) {
  /* @@TODO: */
  return 0;
}
static int layerDanglingGrace(void) {
  /* @@TODO: */
  return 0;
}
static int layerEmpty(void) {
  /* @@TODO: */
  return 0;
}
static int writeLayers(FILE *pf) {
  /* @@TODO: */
  return 1;
}

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
  const char *pModule = NULL;
  double g = 1.0;
  int32_t g_i = 0;
  NMF_DATA *pd = NULL;
  int basis = 0;
  int32_t i = 0;
  int32_t notes = 0;
  NMF_NOTE n;
  
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
    pModule = "nmfgraph";
  }
  
  /* We can have at most one additional argument */
  if ((argc > 1) && (argc != 2)) {
    status = 0;
    fprintf(stderr, "%s: Wrong number of parameters!\n", pModule);
  }
  
  /* Check if we got the optional argument */
  if (status && (argc > 1)) {
    
    /* Make sure argument is present */
    if (argv == NULL) {
      abort();
    }
    if (argv[0] == NULL) {
      abort();
    }
    if (argv[1] == NULL) {
      abort();
    }
    
    /* Parse the gamma integer value */
    if (!parseInt(argv[1], &g_i)) {
      status = 0;
      fprintf(stderr, "%s: Can't parse argument as integer!\n",
        pModule);
    }
    
    /* Range-check gamma value */
    if (status && (g_i < 1)) {
      status = 0;
      fprintf(stderr, "%s: Gamma value out of range!\n", pModule);
    }
    
    /* Compute gamma value */
    if (status && (g_i == 1000)) {
      g = 1.0;
      
    } else if (status) {
      g = ((double) g_i) / 1000.0;
      if (!isfinite(g)) {
        abort();  /* shouldn't happen */
      }
    }
  }
  
  /* Parse input */
  if (status) {
    pd = nmf_parse(stdin);
    if (pd == NULL) {
      status = 0;
      fprintf(stderr, "%s: Can't parse input as NMF!\n", pModule);
    }
  }
  
  /* Make sure basis is correct */
  if (status) {
    basis = nmf_basis(pd);
    if ((basis != NMF_BASIS_44100) && (basis != NMF_BASIS_48000)) {
      status = 0;
      fprintf(stderr, "%s: NMF file has wrong basis!\n", pModule);
    }
  }
  
  /* Sort all the events */
  if (status) {
    nmf_sort(pd);
  }
  
  /* Process all the notes */
  if (status) {
    notes = nmf_notes(pd);
    for(i = 0; i < notes; i++) {
      
      /* Get the current note */
      nmf_get(pd, i, &n);
      
      /* First of all, initialize layer if necessary */
      layerInit(n.layer_i);
      
      /* Determine what to do based on articulation */
      if (n.art == ARTKEY_CONSTANT) {
        /* Constant dynamic -- check pitch range */
        if ((n.pitch < DYN_MINPITCH) || (n.pitch > DYN_MAXPITCH)) {
          status = 0;
          fprintf(stderr, "%s: Invalid dynamic range!\n", pModule);
        }
        
        /* Make sure duration is not grace note */
        if (status && (n.dur < 0)) {
          status = 0;
          fprintf(stderr, "%s: Invalid grace note!\n", pModule);
        }
        
        /* Make sure no grace note is buffered */
        if (status) {
          if (layerHasGrace(n.layer_i)) {
            status = 0;
            fprintf(stderr, "%s: Grace note before constant dynamic!\n",
                    pModule);
          }
        }
        
        /* If this is first note, make sure it has t=0; otherwise, make
         * sure t value of this note is greater than last time value */
        if (status) {
          if (layerFirstNote(n.layer_i)) {
            if (n.t != 0) {
              status = 0;
              fprintf(stderr, "%s: Missing t=0 dynamic!\n", pModule);
            }
          } else {
            if (n.t <= layerLastTime(n.layer_i)) {
              status = 0;
              fprintf(stderr, "%s: Simultaneous dynamics!\n", pModule);
            }
          }
        }
        
        /* Report constant dynamic */
        if (status) {
          if (!layerDynC(n.layer_i, n.t, n.pitch)) {
            status = 0;
            fprintf(stderr, "%s: Layer is too long!\n");
          }
        }
        
      } else if (n.art == ARTKEY_RAMP) {
        /* Ramp dynamic -- check pitch range */
        if ((n.pitch < DYN_MINPITCH) || (n.pitch > DYN_MAXPITCH)) {
          status = 0;
          fprintf(stderr, "%s: Invalid dynamic range!\n", pModule);
        }
        
        /* If this is first note, make sure it has t=0; otherwise, make
         * sure t value of this note is greater than last time value */
        if (status) {
          if (layerFirstNote(n.layer_i)) {
            if (n.t != 0) {
              status = 0;
              fprintf(stderr, "%s: Missing t=0 dynamic!\n", pModule);
            }
          } else {
            if (n.t <= layerLastTime(n.layer_i)) {
              status = 0;
              fprintf(stderr, "%s: Simultaneous dynamics!\n", pModule);
            }
          }
        }
        
        /* Handling depends on whether a grace note or not */
        if (status && (n.dur >= 0)) {
          /* Not a grace note -- if a grace note is buffered, make sure
           * its t value matches that of this note */
          if (layerHasGrace(n.layer_i)) {
            if (layerGraceTime(n.layer_i) != n.t) {
              status = 0;
              fprintf(stderr, "%s: Grace note missing beat!\n",
                      pModule);
            }
          }
          
          /* Report ramp dynamic */
          if (status) {
            if (!layerDynR(n.layer_i, n.t, n.pitch)) {
              status = 0;
              fprintf(stderr, "%s: Layer is too long!\n", pModule);
            }
          }
          
        } else if (status && (n.dur == -1)) {
          /* Grace note -- verify no grace note buffered */
          if (layerHasGrace(n.layer_i)) {
            status = 0;
            fprintf(stderr, "%s: Multiple grace notes!\n", pModule);
          }
          
          /* Buffer grace note */
          if (status) {
            layerGrace(n.layer_i, n.t, n.pitch);
          }
          
        } else if (status) {
          /* Grace note offset greater than one -- not allowed */
          status = 0;
          fprintf(stderr, "%s: Grace offset greater than one!\n",
                  pModule);
        }
        
      } else if (n.art == ARTKEY_HIGH_MUL) {
        /* Set high multiplier -- check range */
        if ((n.pitch < MUL_MINPITCH) || (n.pitch > MUL_MAXPITCH)) {
          status = 0;
          fprintf(stderr, "%s: Invalid multiplier range!\n", pModule);
        }
        
        /* Set high multiplier */
        if (status) {
          if (!layerSetMul(n.layer_i, n.pitch, 1)) {
            status = 0;
            fprintf(stderr, "%s: High multiplier already set!\n",
              pModule);
          }
        }
        
      } else if (n.art == ARTKEY_LOW_MUL) {
        /* Set low multiplier -- check range */
        if ((n.pitch < MUL_MINPITCH) || (n.pitch > MUL_MAXPITCH)) {
          status = 0;
          fprintf(stderr, "%s: Invalid multiplier range!\n", pModule);
        }
        
        /* Set low multiplier */
        if (status) {
          if (!layerSetMul(n.layer_i, n.pitch, 0)) {
            status = 0;
            fprintf(stderr, "%s: Low multiplier already set!\n",
              pModule);
          }
        }
        
      } else {
        /* Unrecognized articulation key */
        status = 0;
        fprintf(stderr, "%s: Unrecognized articulation key!\n",
                pModule);
      }
      
      /* Leave loop if error */
      if (!status) {
        break;
      }
    }
  }
  
  /* Make sure no dangling multipliers */
  if (status) {
    if (layerDanglingMul()) {
      status = 0;
      fprintf(stderr, "%s: Multipliers not well defined!\n", pModule);
    }
  }
  
  /* Make sure no dangling grace notes */
  if (status) {
    if (layerDanglingGrace()) {
      status = 0;
      fprintf(stderr, "%s: Dangling grace note!\n", pModule);
    }
  }
  
  /* Make sure no empty layers */
  if (status) {
    if (layerEmpty()) {
      status = 0;
      fprintf(stderr, "%s: Empty layer!\n", pModule);
    }
  }
  
  /* Write layers to output */
  if (status) {
    if (!writeLayers(stdout)) {
      status = 0;
      fprintf(stderr, "%s: Dangling ramp!\n", pModule);
    }
  }
  
  /* Free data if allocated */
  nmf_free(pd);
  pd = NULL;
  
  /* Invert status and return */
  if (status) {
    status = 0;
  } else {
    status = 1;
  }
  return status;
}
