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
 * the music, use the same operation(s) that are used to convert the
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
 * 
 * All notes must have one of these two articulations.  In addition, the
 * last note in each layer must have articulation '0'.
 * 
 * The pitch of a note selects the dynamic level, as shown below:
 * 
 *   Letter | Number | Dynamic | Linear
 *   =======|========|=========|=======
 *      D   |    -10 |   fff   |     1
 *      C   |    -12 |   ff    |   8/9
 *      B   |     -1 |   f     |   7/9
 *      A   |     -3 |   mf    |   2/3
 *      e   |      4 |   (-)   |   5/9
 *      a   |      9 |   mp    |   4/9
 *      b   |     11 |   p     |   1/3
 *      c   |      0 |   pp    |   2/9
 *      d   |      2 |   ppp   |   1/9
 * 
 * Each note must have one of these pitches, selecting a dynamic.  The
 * dynamics will be scaled linear in range [0.0, 1.0], as shown in the
 * table above.  The linear range will then be optionally gamma-
 * corrected according to the gamma value passed as a parameter to this
 * program.  The resulting gamma-corrected range [0.0, 1.0] will be
 * mapped to integer range [0, 1024] to yield the intensity values used
 * in the graph.
 * 
 * The duration of the note is ignored, unless it is a grace note.  Only
 * a grace note offset of zero (duration -1) is allowed; other grace
 * note offsets are illegal.  Grace notes must have an articulation key
 * of '1'.  Within each layer, grace notes must have the same "t" offset
 * as a non-grace note that has articulation key '1', and there may be
 * at most one grace note for each "t" offset.
 * 
 * The "t" value of the note indicates when the dynamic occurs.  There
 * must be a note at time t=0 in each layer.  Within each layer, there
 * may be at most one non-grace note at each "t" offset.
 * 
 * For ramp dynamics, the length of the ramp is up to the next dynamic
 * in the layer.  (There will always be a next dynamic because the last
 * dynamic in each layer must be constant.)  If the ramp dynamic does
 * not have an attached grace note, then the pitch of the ramp dynamic
 * gives the starting intensity, and the ending intensity is the
 * starting intensity of the next dynamic.  If the ramp dynamic has an
 * attached grace note, the grace note pitch gives the starting
 * intensity and the non-grace pitch gives the ending intensity.
 * 
 * The LAYER_MAX and LAYER_MAXDYN constants impose limits on how many
 * layers there may be and how many dynamics may be in each layer.
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

/*
 * Range of decoded dynamic levels.
 */
#define DYNL_MIN  (1)
#define DYNL_MAX  (9)

/*
 * The maximum number of dynamics that may be within a layer.
 */
#define LAYER_MAXDYN (4000)

/*
 * The maximum zero-based layer index.
 */
#define LAYER_MAX (255)

/*
 * Type definitions
 * ================
 */

/*
 * DYNREC, a dynamic record.
 * 
 * A structure prototype proceeds the definition so that the structure
 * can be referred to within the definition.
 */
struct DYNREC_TAG;
typedef struct DYNREC_TAG DYNREC;
struct DYNREC_TAG {
  
  /*
   * Pointer to the next dynamic record in the layer, or NULL if this
   * is the last dynamic record.
   */
  DYNREC *pNext;
  
  /*
   * The time offset of this dynamic.
   */
  int32_t t;
  
  /*
   * For ramp dynamics, "a" is the starting intensity, in range 1-9.
   * 
   * For constant dynamics, "a" is set to zero.
   */
  uint8_t a;
  
  /*
   * For constant dynamics, "b" is the intensity, in range 1-9.
   * 
   * For ramp dynamics, if "b" is in range 1-9, then "b" is the ending
   * intensity of the ramp.  If "b" is zero for ramp dynamics, it means
   * that the ending intensity of the ramp is the starting intensity of
   * the next dynamic.
   */
  uint8_t b;
};

/*
 * A layer register, used for building layer information.
 */
typedef struct {
  
  /*
   * The total number of dynamics in this layer.
   */
  int32_t dcount;
  
  /*
   * The time offset of the buffered grace note, or -1 if no grace note
   * is currently buffered.
   */
  int32_t gtime;
  
  /*
   * The grace note dynamic in range 1-9, or zero if no grace note is
   * currently buffered.
   */
  uint8_t gval;
  
  /*
   * Pointer to the first dynamic record in this layer.
   * 
   * Only valid if dcount is greater than zero.
   */
  DYNREC *pFirst;
  
  /*
   * Pointer to the last dynamic record in this layer.
   * 
   * Only valid if dcount is greater than zero.
   */
  DYNREC *pLast;
  
} LAYERREG;

/*
 * Static data
 * ===========
 */

/*
 * Flag indicating whether the layer table has been initialized.
 * 
 * Use init_table() to initialize the table if necessary.
 */
static int m_tinit = 0;

/*
 * The table of layer registers.
 * 
 * Only valid if m_tinit.
 */
static LAYERREG m_t[LAYER_MAX + 1];

/*
 * Flag indicating whether the level table has been initialized.
 * 
 * Use init_level() to initialize the table.
 */
static int m_level_init = 0;

/*
 * The level table.
 * 
 * Only valid if m_level_init.
 * 
 * Use an index of [DYNL_MIN, DYNL_MAX] into this table to get the
 * output level to write into the file.  Indices below DYNL_MIN are
 * unused.
 */
static int m_level[DYNL_MAX + 1];

/*
 * Local functions
 * ===============
 */

/* Prototypes */
static int32_t pitchToLevel(int32_t p);

static void init_level(double g);
static void init_table(void);

static int layerHasGrace(int32_t layer_i);
static int32_t layerGraceTime(int32_t layer_i);
static int layerIsEmpty(int32_t layer_i);
static int32_t layerLastTime(int32_t layer_i);
static int layerDynC(int32_t layer_i, int32_t t, int32_t val);
static void layerGrace(int32_t layer_i, int32_t t, int32_t val);
static int layerDynR(int32_t layer_i, int32_t t, int32_t val);
static int layerDangling(int32_t layer_i);

static void writeLayer(FILE *pf, int32_t layer_i);

static int parseInt(const char *pstr, int32_t *pv);

/*
 * Decode a pitch to a dynamic level.
 * 
 * The return value is in range [DYNL_MIN, DYNL_MAX] unless the pitch is
 * not a valid dynamic level, in which case -1 is returned.
 * 
 * Parameters:
 * 
 *   p - the pitch to look up
 * 
 * Return:
 * 
 *   the dynamic level, or -1 if pitch not valid
 */
static int32_t pitchToLevel(int32_t p) {
  
  int32_t result = 0;
  
  switch (p) {
    
    case -10:
      result = 9;
      break;
    
    case -12:
      result = 8;
      break;
    
    case -1:
      result = 7;
      break;
    
    case -3:
      result = 6;
      break;
    
    case 4:
      result = 5;
      break;
    
    case 9:
      result = 4;
      break;
      
    case 11:
      result = 3;
      break;
    
    case 0:
      result = 2;
      break;
    
    case 2:
      result = 1;
      break;
    
    default:
      result = -1;
  }
  
  return result;
}

/*
 * Initialize the level table using the given gamma value.
 * 
 * g must be finite and greater than zero.
 * 
 * The level table must not already be initialized, or a fault occurs.
 * 
 * Parameters:
 * 
 *   g - the gamma value
 */
static void init_level(double g) {
  
  int x = 0;
  double f = 0;
  
  /* Check parameter */
  if (!isfinite(g)) {
    abort();
  }
  if (!(g > 0.0)) {
    abort();
  }
  
  /* Clear table */
  memset(&m_level, 0, (DYNL_MAX + 1) * sizeof(int));
  
  /* Compute table values */
  for(x = DYNL_MIN; x <= DYNL_MAX; x++) {
    
    /* Compute floating-point value */
    f = (((double) x) / ((double) DYNL_MAX));
    if (g != 1.0) {
      f = pow(f, g);
    }
    f = f * 1024.0;
    if (!isfinite(f)) {
      abort();  /* numeric error */
    }
    if (f < 0.0) {
      f = 0.0;
    } else if (f > 1024.0) {
      f = 1024.0;
    }
    
    /* Set integer value */
    m_level[x] = (int) f;
  }
  
  /* Set initialization flag */
  m_level_init = 1;
}

/*
 * Initialize the layer table if not already initialized.
 */
static void init_table(void) {
  
  int32_t i = 0;
  
  /* Only proceed if not initialized */
  if (!m_tinit) {
    
    /* Clear the table */
    memset(&m_t, 0, (LAYER_MAX + 1) * sizeof(LAYERREG));
    
    /* Set each dcount field to zero, and clear each gtime and gval
     * field */
    for(i = 0; i <= LAYER_MAX; i++) {
      (m_t[i]).dcount = 0;
      (m_t[i]).gtime = -1;
      (m_t[i]).gval = 0;
    }
    
    /* Set initialization flag */
    m_tinit = 1;
  }
}

/*
 * Check whether a given layer index has a buffered grace note.
 * 
 * layer_i must be in range [0, LAYER_MAX].
 * 
 * Parameters:
 * 
 *   layer_i - the layer to check
 * 
 * Return:
 * 
 *   non-zero if buffered grace note, zero if not
 */
static int layerHasGrace(int32_t layer_i) {
  
  int result = 0;
  
  /* Check parameter */
  if ((layer_i < 0) || (layer_i > LAYER_MAX)) {
    abort();
  }
  
  /* Initialize table if necessary */
  init_table();
  
  /* See if layer has grace note */
  if ((m_t[layer_i]).gval != 0) {
    result = 1;
  }
  
  /* Return result */
  return result;
}

/*
 * Return the time value of the buffered grace note in the given layer.
 * 
 * layer_i must be in range [0, LAYER_MAX].
 * 
 * A fault occurs if no grace note is buffered in that layer.
 * 
 * Parameters:
 * 
 *   layer_i - the layer to check
 * 
 * Return:
 * 
 *   the time value of the buffered grace note
 */
static int32_t layerGraceTime(int32_t layer_i) {
  
  int32_t result = 0;
  
  /* Check parameter */
  if ((layer_i < 0) || (layer_i > LAYER_MAX)) {
    abort();
  }
  
  /* Initialize table if necessary */
  init_table();
  
  /* Get time value */
  result = (m_t[layer_i]).gtime;
  if (result < 0) {
    abort();  /* no buffered grace note */
  }
  
  /* Return result */
  return result;
}

/*
 * Check whether the given layer is empty, meaning it does not have any
 * dynamics.
 * 
 * A layer can be empty even if it has a buffered grace note.
 * 
 * layer_i must be in range [0, LAYER_MAX].
 * 
 * Parameters:
 * 
 *   layer_i - the layer to check
 * 
 * Return:
 * 
 *   non-zero if layer empty, zero if not
 */
static int layerIsEmpty(int32_t layer_i) {
  
  int result = 0;
  
  /* Check parameter */
  if ((layer_i < 0) || (layer_i > LAYER_MAX)) {
    abort();
  }
  
  /* Initialize table if necessary */
  init_table();
  
  /* See if layer is empty */
  if ((m_t[layer_i]).dcount < 1) {
    result = 1;
  }
  
  /* Return result */
  return result;
}

/*
 * Get the time offset of the last dynamic that was added to the given
 * layer.
 * 
 * layer_i must be in range [0, LAYER_MAX].
 * 
 * A fault occurs if the indicated layer is empty.
 * 
 * Parameters:
 * 
 *   layer_i - the layer to check
 * 
 * Return:
 * 
 *   the time offset of the last dynamic that was added
 */
static int32_t layerLastTime(int32_t layer_i) {
  
  /* Check parameter */
  if ((layer_i < 0) || (layer_i > LAYER_MAX)) {
    abort();
  }
  
  /* Initialize table if necessary */
  init_table();
  
  /* Make sure requested layer is not empty */
  if ((m_t[layer_i]).dcount < 1) {
    abort();  /* layer is empty */
  }
  
  /* Return result */
  return ((m_t[layer_i]).pLast)->t;
}

/*
 * Add a constant dynamic to the given layer.
 * 
 * layer_i must be in range [0, LAYER_MAX].
 * 
 * t must be zero or greater.  If the layer is currently empty, t must
 * be zero.  If the layer is not currently empty, t must be greater than
 * the t of the last dynamic that was added to the layer.  A fault
 * occurs if these conditions do not hold.
 * 
 * val must be in range [DYNL_MIN, DYNL_MAX].
 * 
 * There must not currently be a grace note buffered in the layer, or a
 * fault occurs.
 * 
 * If adding the dynamic would result in too many dynamics in the layer,
 * the function fails.
 * 
 * Parameters:
 * 
 *   layer_i - the layer to add the constant dynamic to
 * 
 *   t - the time offset of the new dynamic
 * 
 *   val - the intensity of the new dynamic
 * 
 * Return:
 * 
 *   non-zero if successful, zero if too many dynamics in layer
 */
static int layerDynC(int32_t layer_i, int32_t t, int32_t val) {
  
  int status = 1;
  DYNREC *dr = NULL;
  
  /* Initialize table if necessary */
  init_table();
  
  /* Check parameters */
  if ((layer_i < 0) || (layer_i > LAYER_MAX)) {
    abort();
  }
  if (t < 0) {
    abort();
  }
  if (layerIsEmpty(layer_i)) {
    if (t != 0) {
      abort();
    }
  } else {
    if (t <= layerLastTime(layer_i)) {
      abort();
    }
  }
  if ((val < DYNL_MIN) || (val > DYNL_MAX)) {
    abort();
  }
  
  /* Check that no grace note currently buffered */
  if (layerHasGrace(layer_i)) {
    abort();
  }
  
  /* Only proceed if room for another dynamic */
  if ((m_t[layer_i]).dcount < LAYER_MAXDYN) {
    
    /* Allocate new dynamic record */
    dr = (DYNREC *) malloc(sizeof(DYNREC));
    if (dr == NULL) {
      abort();
    }
    memset(dr, 0, sizeof(DYNREC));
    
    /* Set variables */
    dr->pNext = NULL;
    dr->t = t;
    dr->a = 0;
    dr->b = (uint8_t) val;
    
    /* Check whether this is the first record */
    if ((m_t[layer_i]).dcount < 1) {
      /* First record -- set both pFirst and pLast */
      (m_t[layer_i]).pFirst = dr;
      (m_t[layer_i]).pLast = dr;
      
    } else {
      /* Not the first record -- link new record into layer */
      ((m_t[layer_i]).pLast)->pNext = dr;
      (m_t[layer_i]).pLast = dr;
    }
    
    /* Increase count */
    ((m_t[layer_i]).dcount)++;
    
  } else {
    /* Layer is full */
    status = 0;
  }
  
  /* Return status */
  return status;
}

/*
 * Buffer a grace note in the given layer.
 * 
 * layer_i must be in range [0, LAYER_MAX].
 * 
 * t must be zero or greater.  If the layer is currently empty, t must
 * be zero.  If the layer is not currently empty, t must be greater than
 * the t of the last dynamic that was added to the layer.  A fault
 * occurs if these conditions do not hold.
 * 
 * val must be in range [DYNL_MIN, DYNL_MAX].
 * 
 * There must not currently be a grace note buffered in the layer, or a
 * fault occurs.
 * 
 * Parameters:
 * 
 *   layer_i - the layer to add the constant dynamic to
 * 
 *   t - the time offset of the new dynamic
 * 
 *   val - the intensity of the new dynamic
 */
static void layerGrace(int32_t layer_i, int32_t t, int32_t val) {
  
  /* Initialize table if necessary */
  init_table();
  
  /* Check parameters */
  if ((layer_i < 0) || (layer_i > LAYER_MAX)) {
    abort();
  }
  if (t < 0) {
    abort();
  }
  if (layerIsEmpty(layer_i)) {
    if (t != 0) {
      abort();
    }
  } else {
    if (t <= layerLastTime(layer_i)) {
      abort();
    }
  }
  if ((val < DYNL_MIN) || (val > DYNL_MAX)) {
    abort();
  }
  
  /* Check that no grace note currently buffered */
  if (layerHasGrace(layer_i)) {
    abort();
  }
  
  /* Buffer the grace note */
  (m_t[layer_i]).gtime = t;
  (m_t[layer_i]).gval = (uint8_t) val;
}

/*
 * Add a ramp dynamic to the given layer.
 * 
 * layer_i must be in range [0, LAYER_MAX].
 * 
 * t must be zero or greater.  If the layer is currently empty, t must
 * be zero.  If the layer is not currently empty, t must be greater than
 * the t of the last dynamic that was added to the layer.  If there is a
 * grace note buffered in the layer, t must equal the time value of the
 * buffered grace note.  A fault occurs if these conditions do not hold.
 * 
 * val must be in range [DYNL_MIN, DYNL_MAX].
 * 
 * If adding the dynamic would result in too many dynamics in the layer,
 * the function fails.
 * 
 * If there is no grace note buffered, a ramp dynamic will be added,
 * starting at the given val intensity and proceeding to the starting
 * intensity of the next dynamic that will be added to the layer.  If a
 * grace note is buffered, the grace note gives the starting intensity,
 * val gives the ending intensity, and the grace note buffer is then
 * cleared.
 * 
 * Parameters:
 * 
 *   layer_i - the layer to add the ramp dynamic to
 * 
 *   t - the time offset of the new dynamic
 * 
 *   val - the intensity of the new dynamic (see above)
 * 
 * Return:
 * 
 *   non-zero if successful, zero if too many dynamics in layer
 */
static int layerDynR(int32_t layer_i, int32_t t, int32_t val) {
  
  int status = 1;
  DYNREC *dr = NULL;
  
  /* Initialize table if necessary */
  init_table();
  
  /* Check parameters */
  if ((layer_i < 0) || (layer_i > LAYER_MAX)) {
    abort();
  }
  if (t < 0) {
    abort();
  }
  if (layerIsEmpty(layer_i)) {
    if (t != 0) {
      abort();
    }
  } else {
    if (t <= layerLastTime(layer_i)) {
      abort();
    }
  }
  if (layerHasGrace(layer_i)) {
    if (t != layerGraceTime(layer_i)) {
      abort();
    }
  }
  if ((val < DYNL_MIN) || (val > DYNL_MAX)) {
    abort();
  }
  
  /* Only proceed if room for another dynamic */
  if ((m_t[layer_i]).dcount < LAYER_MAXDYN) {
    
    /* Allocate new dynamic record */
    dr = (DYNREC *) malloc(sizeof(DYNREC));
    if (dr == NULL) {
      abort();
    }
    memset(dr, 0, sizeof(DYNREC));
    
    /* Set variables, depending on whether buffered grace note */
    if (layerHasGrace(layer_i)) {
      /* Buffered grace note */
      dr->pNext = NULL;
      dr->t = t;
      dr->a = (m_t[layer_i]).gval;
      dr->b = (uint8_t) val;
      
      /* Clear buffered grace note */
      (m_t[layer_i]).gtime = -1;
      (m_t[layer_i]).gval = 0;
      
    } else {
      /* No buffered grace note */
      dr->pNext = NULL;
      dr->t = t;
      dr->a = (uint8_t) val;
      dr->b = 0;
    }
    
    /* Check whether this is the first record */
    if ((m_t[layer_i]).dcount < 1) {
      /* First record -- set both pFirst and pLast */
      (m_t[layer_i]).pFirst = dr;
      (m_t[layer_i]).pLast = dr;
      
    } else {
      /* Not the first record -- link new record into layer */
      ((m_t[layer_i]).pLast)->pNext = dr;
      (m_t[layer_i]).pLast = dr;
    }
    
    /* Increase count */
    ((m_t[layer_i]).dcount)++;
    
  } else {
    /* Layer is full */
    status = 0;
  }
  
  /* Return status */
  return status;
}

/*
 * Check whether a given layer is "dangling."
 * 
 * layer_i must be in range [0, LAYER_MAX].
 * 
 * A layer is dangling if it has a buffered grace note, or if there is
 * at least one dynamic in the layer and the last dynamic is a ramp
 * dynamic.
 * 
 * Parameters:
 * 
 *   layer_i - the layer to check
 * 
 * Return:
 * 
 *   non-zero if layer is dangling, zero if not
 */
static int layerDangling(int32_t layer_i) {
  
  int result = 0;
  
  /* Check parameter */
  if ((layer_i < 0) || (layer_i > LAYER_MAX)) {
    abort();
  }
  
  /* Initialize table if necessary */
  init_table();
  
  /* See if layer is dangling */
  if (layerHasGrace(layer_i)) {
    result = 1;
  } else if (!layerIsEmpty(layer_i)) {
    if (((m_t[layer_i]).pLast)->a != 0) {
      result = 1;
    }
  }
  
  /* Return result */
  return result;
}

/*
 * Write a layer in textual Retro format to the given file.
 * 
 * pf must be open for writing or undefined behavior occurs.  Writing is
 * fully sequential.
 * 
 * layer_i must be in range [0, LAYER_MAX].
 * 
 * A fault occurs if the given layer is dangling or if it is empty.  Use
 * layerDangling() and layerIsEmpty() to check for these conditions.
 * 
 * m_level_init must be non-zero, indicating that the level table has
 * been initialized.  A fault occurs if the level table is not yet
 * initialized.
 * 
 * Layers are always written with a layer multiplier of 1024.  If a
 * different multiplier is desired, you can derive a new layer from the
 * layer that is output.
 * 
 * Parameters:
 * 
 *   pf - the output file
 * 
 *   layer_i - the layer to write
 */
static void writeLayer(FILE *pf, int32_t layer_i) {
  
  LAYERREG *plr = NULL;
  DYNREC *pdr = NULL;
  DYNREC *pdn = NULL;
  int start = 0;
  int end = 0;
  
  /* Check parameters */
  if ((pf == NULL) || (layer_i < 0) || (layer_i > LAYER_MAX)) {
    abort();
  }
  if (layerIsEmpty(layer_i) || layerDangling(layer_i)) {
    abort();
  }
  if (!m_level_init) {
    abort();
  }
  
  /* Get pointer to layer register */
  plr = &(m_t[layer_i]);
  
  /* Write the opening line */
  fprintf(pf, "[\n");
  
  /* Go through all dynamics */
  for(pdr = plr->pFirst; pdr != NULL; pdr = pdr->pNext) {
  
    /* If not first, write comma and line break */
    if (pdr != plr->pFirst) {
      fprintf(pf, ",\n");
    }
  
    /* Determine kind of dynamic */
    if (pdr->a == 0) {
      /* Constant dynamic -- get level */
      start = m_level[pdr->b];
      
      /* Write command */
      fprintf(pf, "  %ld %d lc", (long) (pdr->t), start);
      
    } else if (pdr->b == 0) {
      /* Ramp with end value same as next start value -- get levels */
      start = m_level[pdr->a];
      
      pdn = pdr->pNext;
      if (pdn->a == 0) {
        end = m_level[pdn->b];
      } else {
        end = m_level[pdn->a];
      }
      
      /* Write command */
      fprintf(pf, "  %ld %d %d lr", (long) (pdr->t), start, end);
    
    } else {
      /* Ramp fully specified -- get levels */
      start = m_level[pdr->a];
      end = m_level[pdr->b];
      
      /* Write command */
      fprintf(pf, "  %ld %d %d lr", (long) (pdr->t), start, end);
    }
  }
  
  /* Write the closing line */
  fprintf(pf, "\n] 1024 %ld layer\n", (long) (layer_i + 1));
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
  int32_t lvl = 0;
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
  
  /* Initialize level table */
  if (status) {
    init_level(g);
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
      
      /* Determine level from pitch */
      lvl = pitchToLevel(n.pitch);
      if (lvl < 0) {
        status = 0;
        fprintf(stderr, "%s: Invalid pitch encountered!\n", pModule);
      }
      
      /* Check layer index */
      if (status) {
        if (n.layer_i > LAYER_MAX) {
          status = 0;
          fprintf(stderr, "%s: Maximum layer index exceeded!\n",
                  pModule);
        }
      }
      
      /* If note is a grace note, make sure articulation indicates a
       * ramp, and also that grace note offset is -1 */
      if (status && (n.dur < 0)) {
        if (n.art != ARTKEY_RAMP) {
          status = 0;
          fprintf(stderr, "%s: Grace note must be part of ramp!\n",
                  pModule);
        }
        if (status && (n.dur != -1)) {
          status = 0;
          fprintf(stderr, "%s: Only grace note offset -1 is allowed!\n",
                  pModule);
        }
      }
      
      /* If this is first note, make sure it has t=0; otherwise, make
       * sure t value of this note is greater than last time value */
      if (status) {
        if (layerIsEmpty(n.layer_i)) {
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
      
      /* Determine what to do based on articulation and duration */
      if (status && (n.art == ARTKEY_CONSTANT)) {
        /* Constant dynamic -- make sure no grace note is buffered */
        if (status) {
          if (layerHasGrace(n.layer_i)) {
            status = 0;
            fprintf(stderr, "%s: Grace note before constant dynamic!\n",
                    pModule);
          }
        }
        
        /* Report constant dynamic */
        if (status) {
          if (!layerDynC(n.layer_i, n.t, lvl)) {
            status = 0;
            fprintf(stderr, "%s: Layer is too long!\n");
          }
        }
      
      } else if (status && (n.dur < 0)) {
        /* Grace note -- verify no grace note buffered */
        if (layerHasGrace(n.layer_i)) {
          status = 0;
          fprintf(stderr, "%s: Multiple grace notes!\n", pModule);
        }
          
        /* Buffer grace note */
        if (status) {
          layerGrace(n.layer_i, n.t, lvl);
        }
        
      } else if (status && (n.art == ARTKEY_RAMP)) {
        /* Ramp dynamic -- if a grace note is buffered, make sure its t
         * value matches that of this note */
        if (layerHasGrace(n.layer_i)) {
          if (layerGraceTime(n.layer_i) != n.t) {
            status = 0;
            fprintf(stderr, "%s: Grace note missing beat!\n",
                    pModule);
          }
        }
          
        /* Report ramp dynamic */
        if (status) {
          if (!layerDynR(n.layer_i, n.t, lvl)) {
            status = 0;
            fprintf(stderr, "%s: Layer is too long!\n", pModule);
          }
        }
        
      } else if (status) {
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
  
  /* Make sure no dangling layers */
  if (status) {
    for(i = 0; i <= LAYER_MAX; i++) {
      if (layerDangling(i)) {
        status = 0;
        fprintf(stderr, "%s: Dangling layer!\n", pModule);
      }
    }
  }
  
  /* Write non-empty layers to output */
  if (status) {
    for(i = 0; i <= LAYER_MAX; i++) {
      if (!layerIsEmpty(i)) {
        writeLayer(stdout, i);
      }
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
