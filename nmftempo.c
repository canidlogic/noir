/*
 * nmftempo.c
 * ==========
 * 
 * Convert a Noir Music File (NMF) with a quantum basis of 96 quanta per
 * quarter note into a NMF file with fixed-length quanta, according to a
 * given tempo map.
 * 
 * If only a single tempo is needed, nmfrate is an easier method than
 * this program.  nmftempo, however, allows for multiple tempi and also
 * for gradual tempo changes.
 * 
 * Syntax
 * ------
 * 
 *   nmftempo [map] [srate]
 * 
 * [map] is the path to a Shastina file in "%noir-tempo;" format that
 * will be read to build the tempo map.  See an example of the tempo map
 * format in the "doc" directory.
 * 
 * [srate] is the sampling rate to use for the output NMF file.  It must
 * be either 44100 or 48000.
 * 
 * The input NMF is read from standard input.  It must have a basis of
 * 96 quanta per quarter note.  The output NMF is written to standard
 * output it will have a basis of either 44,100 or 48,000 quanta per
 * second, depending on the [srate] parameter.
 * 
 * Compilation
 * -----------
 * 
 * Requires nmf and libshastina beta 0.9.2 or compatible.
 * 
 * May also require the math library with -lm
 */

#include <limits.h>
#include <math.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "nmf.h"
#include "shastina.h"

/*
 * Error codes
 * ===========
 * 
 * Remember to update error_string!
 */
 
#define ERR_OK      (0)   /* No error */
#define ERR_NMFIN   (1)   /* Error parsing input NMF */
#define ERR_BASISIN (2)   /* Input NMF has improper basis */
#define ERR_XFORM   (3)   /* Error transforming t */
#define ERR_NOZEROT (4)   /* No t=0 tempo */
#define ERR_NOCHRON (5)   /* Tempi not in chronological order */
#define ERR_NUMERIC (6)   /* Numeric computation error */ 
#define ERR_TOOMANY (7)   /* Too many tempi */
#define ERR_DANGLE  (8)   /* Dangling ramp tempo */
#define ERR_EMPTY   (9)   /* Empty tempo map */
#define ERR_TYPESIG (10)  /* Shastina type signature missing */
#define ERR_BADENT  (11)  /* Unsupported Shastina entity */
#define ERR_BADOP   (12)  /* Unsupported operation */
#define ERR_STACKRM (13)  /* Items remaining on stack at end */
#define ERR_STCKFUL (14)  /* Stack is full */
#define ERR_STCKEMP (15)  /* Stack is empty */
#define ERR_DURSTR  (16)  /* Invalid duration string */
#define ERR_NUMSTR  (17)  /* Invalid numeric literal */
#define ERR_OVERFL  (18)  /* Integer overflow */
#define ERR_BADSEC  (19)  /* Invalid section number */
#define ERR_BADCUR  (20)  /* Cursor position out of range */
#define ERR_BADRATE (21)  /* Invalid rate */
#define ERR_BADQ    (22)  /* Invalid quanta count */
#define ERR_BADMIL  (23)  /* Invalid millisecond count */
 
#define ERR_SN_MIN  (500) /* Mininum error code used for Shastina */
#define ERR_SN_MAX  (600) /* Maximum error code used for Shastina */

/*
 * Constants
 * =========
 */

/*
 * The maximum number of tempo nodes allowed in the map.
 */
#define MAX_TEMPI (16384)

/*
 * The initial allocation of nodes in the map.
 */
#define INIT_ALLOC (16)

/*
 * The maximum number of elements on the interpreter stack.
 */
#define MAX_STACK (32)

/*
 * Type declarations
 * =================
 */

/*
 * Structure representing a node in the tempo map.
 */
typedef struct {
  
  /*
   * The A and B parameters of this node.
   * 
   * To transform a t value according to this node, first subtract t by
   * the offset_input of this node to get x.
   * 
   * Then, compute y = a * (x^2) + b * x
   * 
   * Finally, compute y + offset_output to get the transformed t value.
   * 
   * a will be 0.0 for constant tempo nodes, non-zero for ramp nodes.
   */
  double a;
  double b;
  
  /*
   * The t offset at which this node takes effect in the input t basis.
   */
  int32_t offset_input;
  
  /*
   * The transformed t offset at the start of this node.
   */
  int32_t offset_output;
  
} TEMPONODE;

/*
 * Static data
 * ===========
 */

/*
 * Flag indicating whether the tempo map has been initialized.
 * 
 * If set to one, it means the tempo map was successfully initialized.
 * If set to -1, it means there was an error.
 * 
 * Zero means tempo map has not been initialized yet.
 */
static int m_map_init = 0;

/*
 * The sampling rate of the tempo map.
 * 
 * Must be either 48000 or 44100.
 * 
 * Only valid if m_map_init.
 */
static int32_t m_map_rate = 0;

/*
 * The number of nodes in the tempo map.
 * 
 * Only valid if m_map_init.
 */
static int32_t m_map_count = 0;

/*
 * The capacity of the tempo map in nodes.
 * 
 * Only valid if m_map_init.
 */
static int32_t m_map_cap = 0;

/*
 * Pointer to the tempo map.
 * 
 * Only valid if m_map_init.
 */
static TEMPONODE *m_map_t = NULL;

/*
 * Flag indicating whether the tempo node buffer is filled.
 */
static int m_tbuf_filled = 0;

/*
 * The tempo node buffer values.
 * 
 * Only valid if m_tbuf_filled.
 * 
 * These give the buffered time of the ramp node, and the quanta/rate
 * values of the endpoints of the ramp node.
 * 
 * The ramp node must be buffered because the next node must be read
 * before the length can be determined.
 */
static int32_t m_tbuf_t = 0;
static int32_t m_tbuf_q1 = 0;
static int32_t m_tbuf_r1 = 0;
static int32_t m_tbuf_q2 = 0;
static int32_t m_tbuf_r2 = 0;

/*
 * Flag indicating whether the interpreter stack has been initialized.
 */
static int m_st_init = 0;

/*
 * The total number of items on the interpreter stack.
 * 
 * Only valid if m_st_init.
 */
static int32_t m_st_count = 0;

/*
 * The interpreter stack.
 * 
 * Only valid if m_st_init.
 */
static int32_t m_st_t[MAX_STACK];

/*
 * Pointer to the input NMF data.
 * 
 * Set during entrypoint.
 */
static NMF_DATA *m_pdi = NULL;

/*
 * The tempo map cursor.
 */
static int32_t m_cursor = 0;

/*
 * Local functions
 * ===============
 */

/* Prototypes */
static void init_stack(void);
static int stack_push(int32_t val, int *per);
static int stack_pop(int32_t *pv, int *per);

static int checkTime(int32_t t, int *per);
static int addTempo(int32_t t, double a, double b, int *per);

static int addConstantTempo(int32_t t, int32_t q, int32_t r, int *per);
static int addSpanTempo(int32_t t, int32_t q, int32_t m, int *per);
static int addRampTempo(
    int32_t   t,
    int32_t   t_next,
    int32_t   q1,
    int32_t   r1,
    int32_t   q2,
    int32_t   r2,
    int     * per);

static int flushRampBuffer(int32_t t_next, int *per);
static int bufferRamp(
    int32_t   t,
    int32_t   q1,
    int32_t   r1,
    int32_t   q2,
    int32_t   r2,
    int     * per);

static int32_t mapTransform(int32_t t);
static int applyMap(NMF_DATA *pdi, FILE *pOut, int *per);

static int pushDur(const char *pstr, int *per);
static int pushNum(const char *pstr, int *per);
static int opMul(int *per);
static int opSect(int *per);
static int opStep(int *per);
static int opTempo(int *per);
static int opRamp(int *per);
static int opSpan(int *per);
static int parseMap(FILE *pIn, int32_t srate, int *per, long *pln);

static int parseInt(const char *pstr, int32_t *pv);
static const char *error_string(int code);

/*
 * Initialize the interpreter stack, if not already initialized.
 */
static void init_stack(void) {
  if (!m_st_init) {
    memset(m_st_t, 0, MAX_STACK * sizeof(int32_t));
    m_st_count = 0;
    m_st_init = 1;
  }
}

/*
 * Push a value onto the interpreter stack.
 * 
 * per points to a variable to receive an error code if failure.
 * 
 * Parameters:
 * 
 *   val - the value to push
 * 
 *   per - pointer to variable to receive an error code
 * 
 * Return:
 * 
 *   non-zero if successful, zero if error (stack full)
 */
static int stack_push(int32_t val, int *per) {
  
  int status = 1;
  
  /* Check parameters */
  if (per == NULL) {
    abort();
  }
  
  /* Initialize stack if necessary */
  init_stack();
  
  /* Push, if possible */
  if (m_st_count < MAX_STACK) {
    /* Push */
    m_st_t[m_st_count] = val;
    m_st_count++;
    
  } else {
    /* Stack full */
    status = 0;
    *per = ERR_STCKFUL;
  }
  
  /* Return status */
  return status;
}

/*
 * Pop a value off the interpreter stack.
 * 
 * pv points to the variable to receive the value.
 * 
 * per points to a variable to receive an error code if failure.
 * 
 * Parameters:
 * 
 *   pv - points to variable to receive value
 * 
 *   per - pointer to variable to receive an error code
 * 
 * Return:
 * 
 *   non-zero if successful, zero if error (stack is empty)
 */
static int stack_pop(int32_t *pv, int *per) {
  
  int status = 1;
  
  /* Check parameters */
  if ((pv == NULL) || (per == NULL)) {
    abort();
  }
  
  /* Initialize stack if necessary */
  init_stack();
  
  /* Pop, if possible */
  if (m_st_count > 0) {
    /* Pop */
    *pv = m_st_t[m_st_count - 1];
    m_st_count--;
    
  } else {
    /* Stack empty */
    status = 0;
    *per = ERR_STCKEMP;
  }
  
  /* Return status */
  return status;
}

/*
 * Check a given time value to make sure it is proper.
 * 
 * t must be zero or greater or a fault occurs.
 * 
 * per points to a variable to receive an error code in case of error.
 * 
 * The tempo map must be initialized or a fault occurs.
 * 
 * If the tempo map is empty and buffer is empty, t must be zero or
 * there is an error.  If the tempo map is not empty, t must be greater
 * than the last node that was added or an error occurs.
 * 
 * If there is currently a node buffered in the tempo node buffer, t
 * must be greater than the t value in the buffer or an error occurs.
 * 
 * Parameters:
 * 
 *   t - the time to check
 * 
 *   per - pointer to a variable to receive an error code
 * 
 * Return:
 * 
 *   non-zero if successful, zero if t is not valid
 */
static int checkTime(int32_t t, int *per) {
  
  int status = 1;
  
  /* Check parameters */
  if ((t < 0) || (per == NULL)) {
    abort();
  }
  
  /* Check state */
  if (m_map_init <= 0) {
    abort();
  }
  
  /* If tempo map not empty, make sure t greater than last node added */
  if (m_map_count > 0) {
    if (t <= (m_map_t[m_map_count - 1]).offset_input) {
      status = 0;
      *per = ERR_NOCHRON;
    }
  }
  
  /* If tempo node buffer filled, make sure t greater than buffered t
   * value */
  if (status && m_tbuf_filled) {
    if (t <= m_tbuf_t) {
      status = 0;
      *per = ERR_NOCHRON;
    }
  }
  
  /* If tempo map empty and tempo buffer node empty, make sure t is
   * zero */
  if (status && (m_map_count < 1) && (!m_tbuf_filled)) {
    if (t != 0) {
      status = 0;
      *per = ERR_NOZEROT;
    }
  }
  
  /* Return status */
  return status;
}

/*
 * Add a tempo to the tempo map at the given time and with the given A
 * and B parameters.
 * 
 * t is the time offset at the start of the tempo.  It must be zero or
 * greater or a fault occurs.  checkTime() is used to check whether it
 * is valid in the map, with an error occuring if it is not.
 * 
 * A and B are the values to write into the tempo node.  See the tempo
 * node structure for further information.  Both values must be finite,
 * or an error occurs.
 * 
 * per is a pointer to a variable to receive an error code if error.
 * 
 * The tempo map must be initialized before using this function.  This
 * function does NOT attempt to flush the ramp buffer.
 * 
 * Parameters:
 * 
 *   t - the time offset of the tempo node
 * 
 *   a - the A value of the tempo node
 * 
 *   b - the B value of the tempo node
 * 
 *   per - pointer to a variable to receive an error code
 * 
 * Return:
 * 
 *   non-zero if successful, zero if error
 */
static int addTempo(int32_t t, double a, double b, int *per) {
  
  int status = 1;
  int32_t newcap = 0;
  int32_t ofo = 0;
  int32_t x = 0;
  double f = 0.0;
  TEMPONODE *pt = NULL;
  
  /* Check state */
  if (m_map_init <= 0) {
    abort();
  }
  
  /* Check parameters */
  if ((t < 0) || (per == NULL)) {
    abort();
  }
  
  /* Check time value */
  if (!checkTime(t, per)) {
    status = 0;
  }
  
  /* Check floating values */
  if (status) {
    if ((!isfinite(a)) || (!isfinite(b))) {
      status = 0;
      *per = ERR_NUMERIC;
    }
  }
  
  /* Check if map is full */
  if (status) {
    if (m_map_count >= MAX_TEMPI) {
      status = 0;
      *per = ERR_TOOMANY;
    }
  }
  
  /* If capacity is full, expand it */
  if (status && (m_map_count >= m_map_cap)) {
    
    /* First try doubling */
    newcap = m_map_cap * 2;
    
    /* If doubling goes beyond limit, set to limit */
    if (newcap > MAX_TEMPI) {
      newcap = MAX_TEMPI;
    }
    
    /* Reallocate */
    m_map_t = (TEMPONODE *) realloc(
                              m_map_t, newcap * sizeof(TEMPONODE));
    if (m_map_t == NULL) {
      abort();
    }
    memset(
      &(m_map_t[m_map_cap]),
      0,
      (newcap - m_map_cap) * sizeof(TEMPONODE));
    
    /* Update capacity */
    m_map_cap = newcap;
  }
  
  /* If this is very first tempo, offset_output is zero; otherwise, we
   * need to compute offset_output from the previous node */
  if (m_map_count < 1) {
    /* First tempo, so output offset is zero */
    ofo = 0;
  
  } else {
    /* Not first tempo, so get pointer to current last tempo */
    pt = &(m_map_t[m_map_count - 1]);
    
    /* Compute offset_output of new tempo */
    x = (t - pt->offset_input);
    if (pt->a == 0.0) {
      f = pt->b * ((double) x);
    } else {
      f = pt->a * (((double) x) * ((double) x)) + pt->b * ((double) x);
    }
    f += ((double) pt->offset_output);
    f = floor(f);
    
    if (!isfinite(f)) {
      status = 0;
      *per = ERR_NUMERIC;
    }
    
    if (status) {
      if (!((f >= (double) INT32_MIN) && (f <= (double) INT32_MAX))) {
        status = 0;
        *per = ERR_NUMERIC;
      }
    }
    
    if (status) {
      ofo = (int32_t) f;
      if (ofo <= pt->offset_output) {
        ofo = pt->offset_output + 1;
      }
    }
  }
  
  /* Add the new tempo */
  if (status) {
    pt = &(m_map_t[m_map_count]);
    pt->a = a;
    pt->b = b;
    pt->offset_input = t;
    pt->offset_output = ofo;
    m_map_count++;
  }
  
  /* Return status */
  return status;
}

/*
 * Add a constant tempo to the tempo map.
 * 
 * The tempo map must be initialized before using this function.  The
 * ramp buffer will be flushed automatically before adding the constant
 * tempo.
 * 
 * t is the time offset at the start of the tempo.  It must be zero or
 * greater or a fault occurs.  checkTime() is used to check whether it
 * is valid in the map, with an error occuring if it is not.
 * 
 * q is the number of quanta in a beat.  It must be greater than zero.
 * 
 * r is the number beats in ten minutes.  It must be greater than zero.
 * This is ten times the Beat Per Minute BPM rate.
 * 
 * per is a pointer to a variable to receive an error code if the
 * function fails.
 * 
 * Parameters:
 * 
 *   t - the time offset of the tempo node
 * 
 *   q - the number of quanta per beat
 * 
 *   r - the number of beats per ten minutes
 * 
 *   per - pointer to a variable to receive the error code
 * 
 * Return:
 * 
 *   non-zero if successful, zero if error
 */
static int addConstantTempo(int32_t t, int32_t q, int32_t r, int *per) {
  
  int status = 1;
  double f = 0.0;
  
  /* Check state */
  if (m_map_init <= 0) {
    abort();
  }
  
  /* Check parameters */
  if ((t < 0) || (q < 1) || (r < 1) || (per == NULL)) {
    abort();
  }
  
  /* Flush ramp buffer if necessary */
  if (!flushRampBuffer(t, per)) {
    status = 0;
  }
  
  /* Check that time is valid within the map */
  if (status) {
    if (!checkTime(t, per)) {
      status = 0;
    }
  }
  
  /* Since this is a constant tempo, the A parameter will be zero;
   * compute the B parameter */
  if (status) {
    f = (600.0 * ((double) m_map_rate)) /
          (((double) r) * ((double) q));
  }
  
  /* Add the tempo */
  if (status) {
    if (!addTempo(t, 0.0, f, per)) {
      status = 0;
    }
  }
  
  /* Return status */
  return status;
}

/*
 * Add a span-based tempo to the tempo map.
 * 
 * The tempo map must be initialized before using this function.  The
 * ramp buffer will be flushed automatically before adding the span
 * tempo.
 * 
 * t is the time offset at the start of the span.  It must be zero or
 * greater or a fault occurs.  checkTime() is used to check whether it
 * is valid in the map, with an error occuring if it is not.
 * 
 * q is the number of quanta in a span.  It must be greater than zero.
 * 
 * m is the number of milliseconds that q quanta should take.  It must
 * be greater than zero.
 * 
 * per is a pointer to a variable to receive an error code if the
 * function fails.
 * 
 * A constant tempo will be added such that q quanta take m
 * milliseconds.
 * 
 * Parameters:
 * 
 *   t - the time offset of the tempo node
 * 
 *   q - the number of quanta per span
 * 
 *   m - the number of milliseconds per span
 * 
 *   per - pointer to a variable to receive the error code
 * 
 * Return:
 * 
 *   non-zero if successful, zero if error
 */
static int addSpanTempo(int32_t t, int32_t q, int32_t m, int *per) {
  
  int status = 1;
  double f = 0.0;
  
  /* Check state */
  if (m_map_init <= 0) {
    abort();
  }
  
  /* Check parameters */
  if ((t < 0) || (q < 1) || (m < 1) || (per == NULL)) {
    abort();
  }
  
  /* Flush ramp buffer if necessary */
  if (!flushRampBuffer(t, per)) {
    status = 0;
  }
  
  /* Check that time is valid within the map */
  if (status) {
    if (!checkTime(t, per)) {
      status = 0;
    }
  }
  
  /* Since this is a constant tempo, the A parameter will be zero;
   * compute the B parameter */
  if (status) {
    f = (((double) m) * (((double) m_map_rate) / 1000.0)) /
        ((double) q);
  }
  
  /* Add the tempo */
  if (status) {
    if (!addTempo(t, 0.0, f, per)) {
      status = 0;
    }
  }
  
  /* Return status */
  return status;
}

/*
 * Add a ramp tempo to the tempo map.
 * 
 * The tempo map must be initialized before using this function.  The
 * ramp buffer will be flushed automatically before adding the new ramp
 * tempo.  (Careful about when exactly the buffer flag is cleared, to
 * avoid an infinite loop while flushing.)
 * 
 * t is the time offset at the start of the ramp.  It must be zero or
 * greater or a fault occurs.  checkTime() is used to check whether it
 * is valid in the map, with an error occuring if it is not.
 * 
 * t_next is the time offset of the next tempo.  It must be greater than
 * t or a fault occurs.  t_next is only used for determining the length
 * of the ramp.
 * 
 * q1 and q2 are the number of quanta in a beat at the starting tempo of
 * the ramp and the ending tempo of the ramp.  Both must be greater than
 * zero.
 * 
 * r1 and r2 and the number of beats per ten minutes at the starting
 * tempo of the ramp and the ending tempo of the ramp.  Both must be
 * greater than zero.
 * 
 * per is a pointer to a variable to receive an error code if the
 * function fails.
 * 
 * Parameters:
 * 
 *   t - the time offset of the tempo node
 * 
 *   t_next - the time offset of the next tempo node
 * 
 *   q1 - the number of quanta per beat of the starting tempo
 * 
 *   r1 - the number of beats per ten minutes of the starting tempo
 * 
 *   q2 - the number of quanta per beat of the ending tempo
 * 
 *   r2 - the number of beats per ten minutes of the ending tempo
 * 
 *   per - pointer to a variable to receive the error code
 * 
 * Return:
 * 
 *   non-zero if successful, zero if error
 */
static int addRampTempo(
    int32_t   t,
    int32_t   t_next,
    int32_t   q1,
    int32_t   r1,
    int32_t   q2,
    int32_t   r2,
    int     * per) {
  
  int status = 1;
  double accel = 0.0;
  double v_start = 0.0;
  double v_end = 0.0;
  
  /* Check state */
  if (m_map_init <= 0) {
    abort();
  }
  
  /* Check parameters */
  if ((t < 0) || (t_next <= t) ||
      (q1 < 1) || (r1 < 1) ||
      (q2 < 1) || (r2 < 1) || (per == NULL)) {
    abort();
  }
  
  /* Flush ramp buffer if necessary */
  if (!flushRampBuffer(t, per)) {
    status = 0;
  }
  
  /* Check that time is valid within the map */
  if (status) {
    if (!checkTime(t, per)) {
      status = 0;
    }
  }
  
  /* Compute the velocity at the start and the velocity at the end of
   * the ramp */
  if (status) {
    v_start = (600.0 * ((double) m_map_rate)) /
          (((double) r1) * ((double) q1));
    v_end = (600.0 * ((double) m_map_rate)) /
          (((double) r2) * ((double) q2));
  }
  
  /* The acceleration is the difference between the end and start tempo,
   * divided by the length of the span in input quanta */
  if (status) {
    accel = (v_end - v_start) / ((double) (t_next - t));
  }
  
  /* Add the tempo, with the A parameter being half the acceleration and
   * the B parameter being the starting velocity (so that the first
   * derivative is the starting velocity at the beginning of the span,
   * and the second derivative is the acceleration) */
  if (status) {
    if (!addTempo(t, (accel / 2.0), v_start, per)) {
      status = 0;
    }
  }
  
  /* Return status */
  return status;
}

/* 
 * Flush the ramp buffer, if it is filled.
 * 
 * t_next is the time value of the next node.  It must be zero or
 * greater.  per points to a variable to receive an error code in case
 * of error.
 * 
 * If the ramp buffer is empty, nothing happens.
 * 
 * If the ramp buffer is filled, this clears the buffer and calls
 * through to addRampTempo() now that all the information is known.
 * 
 * Parameters:
 * 
 *   t_next - time offset of the next node
 * 
 *   per - pointer to a variable to receive an error code
 * 
 * Return:
 * 
 *   non-zero if successful, zero if error
 */
static int flushRampBuffer(int32_t t_next, int *per) {
  
  int status = 1;
  
  /* Check parameters */
  if ((t_next < 0) || (per == NULL)) {
    abort();
  }
  
  /* Only proceed if buffer filled */
  if (m_tbuf_filled) {
    /* Clear buffer filled flag and call through */
    m_tbuf_filled = 0;
    if (!addRampTempo(
          m_tbuf_t,
          t_next,
          m_tbuf_q1,
          m_tbuf_r1,
          m_tbuf_q2,
          m_tbuf_r2,
          per)) {
      status = 0;
    }
  }
  
  /* Return status */
  return status;
}

/*
 * Buffer a ramp node.
 * 
 * This first flushes the ramp buffer using the given t value.
 * 
 * Then, the given parameters are stored in the ramp buffer.
 * 
 * t must be zero or greater.  If this is the first tempo that is being
 * added, t must be zero or an error occurs.  Otherwise, t must be
 * greater than the last tempo added or an error occurs.
 * 
 * The q and r parameters must all be greater than zero.  If the q1/r1
 * pair is identical to the q2/r2 pair, the call is equivalent to caling
 * addConstantTempo() and no buffering takes place.
 * 
 * per points to a variable to receive an error code in case of error.
 * 
 * The tempo map must be initialized or a fault occurs.
 * 
 * Parameters:
 * 
 *   t - the time offset of the ramp node to buffer
 * 
 *   q1 - the starting quanta of the beat
 * 
 *   r1 - the starting rate, in beats per 10 minutes
 * 
 *   q2 - the ending quanta of the beat
 * 
 *   r2 - the ending rate, in beats per 10 minutes
 * 
 *   per - pointer to variable to receive an error code
 * 
 * Return:
 * 
 *   non-zero if successful, zero if error
 */
static int bufferRamp(
    int32_t   t,
    int32_t   q1,
    int32_t   r1,
    int32_t   q2,
    int32_t   r2,
    int     * per) {
  
  int status = 1;
  
  /* Check state */
  if (m_map_init <= 0) {
    abort();
  }
  
  /* Check parameters */
  if ((t < 0) || (per == NULL) ||
      (q1 < 1) || (r1 < 1) ||
      (q2 < 1) || (r2 < 1)) {
    abort();
  }
  
  /* Further check of time parameter */
  if (!checkTime(t, per)) {
    status = 0;
  }
  
  /* First, filter out case of end tempo equal to start tempo */
  if (status) {
    if ((q1 == q2) && (r1 == r2)) {
      /* Rates are the same, so call through to constant tempo */
      if (!addConstantTempo(t, q1, r1, per)) {
        status = 0;
      }
    
    } else {
      /* Rates are not the same -- begin by flushing buffer */
      if (!flushRampBuffer(t, per)) {
        status = 0;
      }
    
      /* Store parameters in buffer */
      m_tbuf_t = t;
      m_tbuf_q1 = q1;
      m_tbuf_r1 = r1;
      m_tbuf_q2 = q2;
      m_tbuf_r2 = r2;
      m_tbuf_filled = 1;
    }
  }
  
  /* Return status */
  return status;
}

/*
 * Transform an input t value to an output t value using the tempo map.
 * 
 * t is the input quantum offset, which must be greater than or equal to
 * zero.  t is specified with a quantum basis of 96 quanta per quarter.
 * 
 * The return value is the offset using the fixed-length basis
 * established by parseMap().
 * 
 * If the output t can not be computed due to overflow or other numeric
 * problems, -1 is returned.
 * 
 * The tempo map must already be successfully initialized or a fault
 * occurs.
 * 
 * Parameters:
 * 
 *   t - the input t value
 * 
 * Return:
 * 
 *   the output t value, or -1 if t could not be computed
 */
static int32_t mapTransform(int32_t t) {
  
  int status = 1;
  TEMPONODE *pt = NULL;
  TEMPONODE *pnx = NULL;
  int32_t lo = 0;
  int32_t hi = 0;
  int32_t mid = 0;
  int32_t mid_val = 0;
  double f = 0.0;
  
  /* Check parameter */
  if (t < 0) {
    abort();
  }
  
  /* Check state */
  if (m_map_init <= 0) {
    abort();
  }
  
  /* Search for the tempo node with the greatest offset_input that is
   * less than or equal to the input t value; also, get pointer to node
   * after it, if there is one */
  if ((m_map_t[m_map_count - 1]).offset_input <= t) {
    /* t greater than or equal to last node, so use last node, and there
     * is no next node */
    pt = &(m_map_t[m_map_count - 1]);
    pnx = NULL;
    
  } else {
    /* t less than last node, so perform binary search to find desired
     * node */
    lo = 0;
    hi = m_map_count - 1;
    while (lo < hi) {
      
      /* Compute midpoint, which must be greater than low bound */
      mid = lo + ((hi - lo) / 2);
      if (mid <= lo) {
        mid = lo + 1;
      }
      
      /* Get midpoint value */
      mid_val = (m_map_t[mid]).offset_input;
      
      /* Compare t to midpoint value */
      if (t < mid_val) {
        /* t less than midpoint value, so set hi bound to one lower than
         * midpoint */
        hi = mid - 1;
        
      } else if (t > mid_val) {
        /* t greater than midpoint vlaue, so set lo bound to midpoint */
        lo = mid;
        
      } else if (t == mid_val) {
        /* t equals midpoint value, so zoom in on midpoint */
        lo = mid;
        hi = mid;
        
      } else {
        abort();  /* shouldn't happen */
      }
    }
    
    /* Get pointer to desired node and next node */
    pt = &(m_map_t[lo]);
    pnx = &(m_map_t[lo + 1]);
  }
  
  /* We found which node we are in, so change t to be an offset within
   * this tempo node */
  t = t - pt->offset_input;
  
  /* Compute the transformed offset in floating-point */
  if (pt->a == 0.0) {
    f = pt->b * ((double) t);
  } else {
    f = pt->a * (((double) t) * ((double) t)) + pt->b * ((double) t);
  }
  
  /* Floor the offset */
  f = floor(f);
  
  /* Check for numeric problems */
  if (!isfinite(f)) {
    status = 0;
  }
  if (status) {
    if (!((f >= ((double) INT32_MIN)) && (f <= ((double) INT32_MAX)))) {
      status = 0;
    }
  }
  
  /* Get transformed offset as integer */
  if (status) {
    t = (int32_t) f;
  }
  
  /* If transformed offset less than zero, set to zero */
  if (status && (t < 0)) {
    t = 0;
  }
  
  /* Add transformed offset to output offset, watching for overflow */
  if (status) {
    if (t <= INT32_MAX - pt->offset_output) {
      t = t + pt->offset_output;
    } else {
      status = 0;
    }
  }
  
  /* If there is a next node, make sure transformed t is less than its
   * output offset */
  if (status && (pnx != NULL)) {
    if (pnx->offset_output <= t) {
      t = pnx->offset_output - 1;
    }
  }
  
  /* If failure, set t to -1 */
  if (!status) {
    t = -1;
  }
  
  /* Return result */
  return t;
}

/*
 * Apply a tempo map to the input NMF and write to the output NMF.
 * 
 * pdi is the input NMF data.
 * 
 * pOut is the output NMF file to write.  It must be open for writing or
 * undefined behavior occurs.  Writing is fully sequential.
 * 
 * per points to a variable to receive an error code in case of error.
 * The error code may be converted to an error message with the function
 * error_string().
 * 
 * The tempo map must be successfully initialized or a fault occurs.
 * 
 * Parameters:
 * 
 *   pdi - the input NMF data
 * 
 *   pOut - the output file to write
 * 
 *   per - pointer to variable to receive an error code
 * 
 * Return:
 * 
 *   non-zero if successful, zero if error
 */
static int applyMap(NMF_DATA *pdi, FILE *pOut, int *per) {
  
  int status = 1;
  NMF_DATA *pdo = NULL;
  int32_t sections = 0;
  int32_t notes = 0;
  int32_t i = 0;
  int32_t x = 0;
  int32_t y = 0;
  NMF_NOTE n;
  
  /* Initialize structures */
  memset(&n, 0, sizeof(NMF_NOTE));
  
  /* Check parameters */
  if ((pdi == NULL) || (pOut == NULL) || (per == NULL)) {
    abort();
  }
  
  /* Check state */
  if (m_map_init <= 0) {
    abort();  /* tempo map not initialized */
  }
  
  /* Reset error */
  *per = ERR_OK;
  
  /* Make sure input has proper quantum basis */
  if (status) {
    if (nmf_basis(pdi) != NMF_BASIS_Q96) {
      status = 0;
      *per = ERR_BASISIN;
    }
  }
  
  /* Allocate output object and set its basis */
  if (status) {
    pdo = nmf_alloc();
    if (m_map_rate == 48000) {
      nmf_rebase(pdo, NMF_BASIS_48000);
    } else if (m_map_rate == 44100) {
      nmf_rebase(pdo, NMF_BASIS_44100);
    } else {
      abort();  /* shouldn't happen */
    }
  }
  
  /* Get the number of sections and notes in the input */
  if (status) {
    sections = nmf_sections(pdi);
    notes = nmf_notes(pdi);
  }
  
  /* Transfer all input sections to output, transforming their offsets
   * according to the tempo map */
  if (status) {
    for(i = 1; i < sections; i++) {
      x = mapTransform(nmf_offset(pdi, i));
      if (x < 0) {
        status = 0;
        *per = ERR_XFORM;
        break;
      }
      if (!nmf_sect(pdo, x)) {
        abort();  /* shouldn't happen */
      }
    }
  }
  
  /* Transfer all notes to output, transforming their t offsets and
   * durations according to tempo map */
  if (status) {
    for(i = 0; i < notes; i++) {
      
      /* Get current note */
      nmf_get(pdi, i, &n);
      
      /* Transform the t value, unless it is zero; zero is left as zero
       * because that mapping should always hold */
      if (n.t != 0) {
        x = mapTransform(n.t);
        if (x < 0) {
          status = 0;
          *per = ERR_XFORM;
        }
      } else {
        x = 0;
      }
            
      /* If the duration is greater than zero, transform it; leave alone
       * durations of zero and negative durations (grace note
       * offsets) */
      if (status && (n.dur > 0)) {
        /* Compute the t value at the end of the duration, watching for
         * overflow */
        if (n.dur <= INT32_MAX - n.t) {
          y = n.dur + n.t;
        } else {
          status = 0;
          *per = ERR_XFORM;
        }
        
        /* Transform the endpoint t value */
        if (status) {
          y = mapTransform(y);
          if (y < 0) {
            status = 0;
            *per = ERR_XFORM;
          }
        }
        
        /* Compute the transformed duration */
        if (status) {
          n.dur = y - x;
        }
      }
      
      /* Now that duration is computed, store the transformed t */
      if (status) {
        n.t = x;
      }
      
      /* Write transformed note to output */
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
  
  /* Serialize to output */
  if (status) {
    if (!nmf_serialize(pdo, pOut)) {
      abort();  /* shouldn't happen */
    }
  }
  
  /* Free the data objects if allocated */
  nmf_free(pdi);
  nmf_free(pdo);
  pdi = NULL;
  pdo = NULL;
  
  /* Return status */
  return status;
}

/*
 * Push the number of quanta in a duration string onto the interpreter
 * stack.
 * 
 * pstr is the duration string.  per points to a variable to receive an
 * error code in case of error.
 * 
 * Parameters:
 * 
 *   pstr - the duration string
 * 
 *   per - pointer to variable to receive error code
 * 
 * Return:
 * 
 *   non-zero if successful, zero if error
 */
static int pushDur(const char *pstr, int *per) {
  
  int status = 1;
  int c = 0;
  int32_t d = 0;
  int32_t dur = 0;
  
  /* Check parameters */
  if ((pstr == NULL) || (per == NULL)) {
    abort();
  }
  
  /* Duration string may not be empty */
  if (*pstr == 0) {
    status = 0;
    *per = ERR_DURSTR;
  }
  
  /* Go through each character in the duration string */
  if (status) {
    for( ; *pstr != 0; pstr++) {
      
      /* Get current character */
      c = *pstr;
      
      /* Get the number of quanta represented */
      if (c == '1') {
        d = 6;
      } else if (c == '2') {
        d = 12;
      } else if (c == '3') {
        d = 24;
      } else if (c == '4') {
        d = 48;
      } else if (c == '5') {
        d = 96;
      } else if (c == '6') {
        d = 192;
      } else if (c == '7') {
        d = 384;
      } else if (c == '8') {
        d = 32;
      } else if (c == '9') {
        d = 64;
      } else {
        /* Invalid character */
        status = 0;
        *per = ERR_DURSTR;
      }
      
      /* Check if the next character is a modifier; if it is, apply it
       * and skip the character */
      if (status) {
        if (pstr[1] == '\'') {
          /* Doubling modifier */
          d *= 2;
          pstr++;
          
        } else if (pstr[1] == '.') {
          /* Multiply by 1.5 */
          d = d + (d / 2);
          pstr++;
          
        } else if (pstr[1] == ',') {
          /* Halving modifier */
          d /= 2;
          pstr++;
        }
      }
      
      /* Add duration to current duration, watching for overflow */
      if (status) {
        if (d <= INT32_MAX - dur) {
          dur += d;
        } else {
          /* overflow */
          status = 0;
          *per = ERR_DURSTR;
        }
      }
      
      /* Leave loop if error */
      if (!status) {
        break;
      }
    }
  }
  
  /* Push the duration */
  if (status) {
    if (!stack_push(dur, per)) {
      status = 0;
    }
  }
  
  /* Return status */
}

/*
 * Push an integer literal onto the interpreter stack.
 * 
 * pstr is the numeric string.  per points to a variable to receive an
 * error code in case of error.
 * 
 * Parameters:
 * 
 *   pstr - the numeric string
 * 
 *   per - pointer to variable to receive error code
 * 
 * Return:
 * 
 *   non-zero if successful, zero if error
 */
static int pushNum(const char *pstr, int *per) {
  
  int status = 1;
  int32_t val = 0;
  
  /* Check parameters */
  if ((pstr == NULL) || (per == NULL)) {
    abort();
  }
  
  /* Try to parse the number */
  if (!parseInt(pstr, &val)) {
    status = 0;
    *per = ERR_NUMSTR;
  }
  
  /* Push the number */
  if (status) {
    if (!stack_push(val, per)) {
      status = 0;
    }
  }
  
  /* Return status */
  return status;
}

/*
 * Run a multiply operation.
 * 
 * This pops two integers off the stack, multiplies them, and pushes the
 * result back onto the stack.
 * 
 * per points to a variable to receive to an error code if error.
 * 
 * Parameters:
 * 
 *   per - pointer to variable to receive error code
 * 
 * Return:
 * 
 *   non-zero if successful, zero if error
 */
static int opMul(int *per) {
  
  int status = 1;
  int32_t a = 0;
  int32_t b = 0;
  int64_t r = 0;
  
  /* Check parameter */
  if (per == NULL) {
    abort();
  }
  
  /* Pop the parameters */
  if (!stack_pop(&b, per)) {
    status = 0;
  }
  if (status) {
    if (!stack_pop(&a, per)) {
      status = 0;
    }
  }
  
  /* Compute result in 64-bit */
  if (status) {
    r = ((int64_t) a) * ((int64_t) b);
  }
  
  /* Range-check result */
  if (status) {
    if ((r < INT32_MIN) || (r > INT32_MAX)) {
      status = 0;
      *per = ERR_OVERFL;
    }
  }
  
  /* Push result */
  if (status) {
    if (!stack_push((int32_t) r, per)) {
      status = 0;
    }
  }
  
  /* Return status */
  return status;
}

/*
 * Run a section operation.
 * 
 * This pops an integer off the stack, and moves the cursor to the start
 * of that section, based on the input NMF sections in m_pdi.
 * 
 * A fault occurs if m_pdi is NULL.
 * 
 * per points to a variable to receive to an error code if error.
 * 
 * Parameters:
 * 
 *   per - pointer to variable to receive error code
 * 
 * Return:
 * 
 *   non-zero if successful, zero if error
 */
static int opSect(int *per) {
  
  int status = 1;
  int32_t sect = 0;
  
  /* Check parameter */
  if (per == NULL) {
    abort();
  }
  
  /* Check state */
  if (m_pdi == NULL) {
    abort();
  }
  
  /* Pop the section number */
  if (!stack_pop(&sect, per)) {
    status = 0;
  }
  
  /* Check that section number is in range */
  if (status) {
    if ((sect < 0) || (sect >= nmf_sections(m_pdi))) {
      status = 0;
      *per = ERR_BADSEC;
    }
  }
  
  /* Set the cursor to the section offset */
  if (status) {
    m_cursor = nmf_offset(m_pdi, sect);
  }
  
  /* Return status */
  return status;
}

/*
 * Run a step operation.
 * 
 * This pops an integer off the stack, and adds its value to the current
 * cursor position.  The integer may be negative, but it may not take
 * the cursor to a negative position.
 * 
 * per points to a variable to receive to an error code if error.
 * 
 * Parameters:
 * 
 *   per - pointer to variable to receive error code
 * 
 * Return:
 * 
 *   non-zero if successful, zero if error
 */
static int opStep(int *per) {
  
  int status = 1;
  int32_t sv = 0;
  int64_t r = 0;
  
  /* Check parameter */
  if (per == NULL) {
    abort();
  }
  
  /* Pop step value */
  if (!stack_pop(&sv, per)) {
    status = 0;
  }
  
  /* Add the step value to the cursor in 64-bit */
  if (status) {
    r = m_cursor + sv;
  }
  
  /* Range-check result */
  if (status) {
    if ((r < 0) || (r > INT32_MAX)) {
      status = 0;
      *per = ERR_BADCUR;
    }
  }
  
  /* Update cursor */
  if (status) {
    m_cursor = (int32_t) r;
  }
  
  /* Return status */
  return status;
}

/*
 * Run a constant tempo operation.
 * 
 * This pops a rate integer, and then a quanta integer off the stack,
 * and combines it with the current cursor position to add a constant
 * tempo to the tempo map.
 * 
 * per points to a variable to receive to an error code if error.
 * 
 * Parameters:
 * 
 *   per - pointer to variable to receive error code
 * 
 * Return:
 * 
 *   non-zero if successful, zero if error
 */
static int opTempo(int *per) {
  
  int status = 1;
  int32_t r = 0;
  int32_t q = 0;
  
  /* Check parameter */
  if (per == NULL) {
    abort();
  }
  
  /* Pop parameters */
  if (!stack_pop(&r, per)) {
    status = 0;
  }
  if (status) {
    if (!stack_pop(&q, per)) {
      status = 0;
    }
  }
  
  /* Range-check parameters */
  if (status && (r < 1)) {
    status = 0;
    *per = ERR_BADRATE;
  }
  if (status && (q < 1)) {
    status = 0;
    *per = ERR_BADQ;
  }
  
  /* Add tempo */
  if (status) {
    if (!addConstantTempo(m_cursor, q, r, per)) {
      status = 0;
    }
  }
  
  /* Return status */
  return status;
}

/*
 * Run a ramp tempo operation.
 * 
 * This pops a rate2 integer, then a quanta2 integer, then a rate1
 * integer, and finally a quanta1 integer off the stack, and combines it
 * with the current cursor position to buffer a ramp tempo.  The buffer
 * will be flushed on the next tempo, when the length of the ramp is
 * determined.
 * 
 * per points to a variable to receive to an error code if error.
 * 
 * Parameters:
 * 
 *   per - pointer to variable to receive error code
 * 
 * Return:
 * 
 *   non-zero if successful, zero if error
 */
static int opRamp(int *per) {
  
  int status = 1;
  int32_t r2 = 0;
  int32_t q2 = 0;
  int32_t r1 = 0;
  int32_t q1 = 0;
  
  /* Check parameter */
  if (per == NULL) {
    abort();
  }
  
  /* Pop parameters */
  if (!stack_pop(&r2, per)) {
    status = 0;
  }
  if (status) {
    if (!stack_pop(&q2, per)) {
      status = 0;
    }
  }
  if (status) {
    if (!stack_pop(&r1, per)) {
      status = 0;
    }
  }
  if (status) {
    if (!stack_pop(&q1, per)) {
      status = 0;
    }
  }
  
  /* Range-check parameters */
  if (status && ((r1 < 1) || (r2 < 1))) {
    status = 0;
    *per = ERR_BADRATE;
  }
  if (status && ((q1 < 1) || (q2 < 1))) {
    status = 0;
    *per = ERR_BADQ;
  }
  
  /* Buffer tempo */
  if (status) {
    if (!bufferRamp(m_cursor, q1, r1, q2, r2, per)) {
      status = 0;
    }
  }
  
  /* Return status */
  return status;
}

/*
 * Run a span tempo operation.
 * 
 * This pops a millisecond count, and then a quanta integer off the
 * stack, and combines it with the current cursor position to add a span
 * tempo to the tempo map.
 * 
 * per points to a variable to receive to an error code if error.
 * 
 * Parameters:
 * 
 *   per - pointer to variable to receive error code
 * 
 * Return:
 * 
 *   non-zero if successful, zero if error
 */
static int opSpan(int *per) {
  
  int status = 1;
  int32_t m = 0;
  int32_t q = 0;
  
  /* Check parameter */
  if (per == NULL) {
    abort();
  }
  
  /* Pop parameters */
  if (!stack_pop(&m, per)) {
    status = 0;
  }
  if (status) {
    if (!stack_pop(&q, per)) {
      status = 0;
    }
  }
  
  /* Range-check parameters */
  if (status && (m < 1)) {
    status = 0;
    *per = ERR_BADMIL;
  }
  if (status && (q < 1)) {
    status = 0;
    *per = ERR_BADQ;
  }
  
  /* Add tempo */
  if (status) {
    if (!addSpanTempo(m_cursor, q, m, per)) {
      status = 0;
    }
  }
  
  /* Return status */
  return status;
}

/*
 * Parse a tempo map.
 * 
 * This may only be called once.  Additional calls cause a fault.
 * 
 * pIn is the Shastina file to read.  It must be open for reading or
 * undefined behavior occurs.  Reading is fully sequential.
 * 
 * srate is the sampling rate to use.  It must be either 48000 or 44100.
 * 
 * per points to a variable to receive an error code in case of failure.
 * pln points to a variable to receive a line number in case of failure.
 * The error code can be converted into a string with error_string().
 * 
 * Parameters:
 * 
 *   pIn - the tempo map file to read
 * 
 *   srate - the sampling rate
 * 
 *   per - pointer to variable to receive the error code
 * 
 *   pln - pointer to variable to receive a line number
 * 
 * Return:
 * 
 *   non-zero if successful, zero if error
 */
static int parseMap(FILE *pIn, int32_t srate, int *per, long *pln) {
  
  int status = 1;
  int first_ent = 1;
  int autostep = 0;
  int retval = 0;
  SNPARSER *pr = NULL;
  SNSOURCE *ps = NULL;
  SNENTITY ent;
  
  /* Initialize structure */
  memset(&ent, 0, sizeof(SNENTITY));
  
  /* Check state */
  if (m_map_init != 0) {
    abort();
  }
  
  /* Check parameters */
  if ((pIn == NULL) || (per == NULL) || (pln == NULL)) {
    abort();
  }
  if ((srate != 48000) && (srate != 44100)) {
    abort();
  }
  
  /* Wrap the input file in a Shastina source object */
  ps = snsource_file(pIn, 0);
  
  /* Reset error */
  *per = ERR_OK;
  *pln = -1;
  
  /* Initialize interpreter stack */
  init_stack();
  
  /* Initialize an empty tempo map state */
  m_map_init = 1;
  m_map_count = 0;
  m_map_rate = srate;
  m_map_cap = INIT_ALLOC;
  m_map_t = (TEMPONODE *) calloc(INIT_ALLOC, sizeof(TEMPONODE));
  if (m_map_t == NULL) {
    abort();
  }
  
  /* Allocate a Shastina parser */
  pr = snparser_alloc();
  
  /* Go through all Shastina entities in input until either an error or
   * the EOF marker */
  for(snparser_read(pr, &ent, ps);
      ent.status > 0;
      snparser_read(pr, &ent, ps)) {
    
    /* If this is the first entity being read, make sure it is the
     * opening metacommand sequence, then continue the loop to read
     * on */
    if (first_ent) {
      /* First entity, so clear first entity flag */
      first_ent = 0;
      
      /* This entity must be BEGIN_META */
      if (ent.status != SNENTITY_BEGIN_META) {
        status = 0;
        *per = ERR_TYPESIG;
        *pln = snparser_count(pr);
      }
      
      /* Read another entity, which must be the correct metatoken */
      if (status) {
        snparser_read(pr, &ent, ps);
        if (ent.status != SNENTITY_META_TOKEN) {
          status = 0;
          *per = ERR_TYPESIG;
          *pln = snparser_count(pr);
        }
        if (status) {
          if (strcmp(ent.pKey, "noir-tempo") != 0) {
            status = 0;
            *per = ERR_TYPESIG;
            *pln = snparser_count(pr);
          }
        }
      }
      
      /* Read another entity, which must be end meta */
      if (status) {
        snparser_read(pr, &ent, ps);
        if (ent.status != SNENTITY_END_META) {
          status = 0;
          *per = ERR_TYPESIG;
          *pln = snparser_count(pr);
        }
      }
      
      /* Continue the loop */
      if (status) {
        continue;
      }
    }
    
    /* Handle the supported entity types (excluding the type signature,
     * handled above, and the special EOF type) */
    if (status) {
      if (ent.status == SNENTITY_STRING) {
        /* String type -- first, make sure quoted */
        if (ent.str_type != SNSTRING_QUOTED) {
          status = 0;
          *per = ERR_BADENT;
          *pln = snparser_count(pr);
        }
        
        /* If "t" prefix is present, set autostep flag; if no prefix,
         * clear autostep flag; if any other prefix, error */
        if (status) {
          if (strlen(ent.pKey) < 1) {
            /* No prefix */
            autostep = 0;
          
          } else if (strcmp(ent.pKey, "t") == 0) {
            /* Autostep prefix */
            autostep = 1;
            
          } else {
            /* Unknown prefix */
            status = 0;
            *per = ERR_BADENT;
            *pln = snparser_count(pr);
          }
        }
        
        /* Push the duration */
        if (status) {
          if (!pushDur(ent.pValue, per)) {
            status = 0;
            *pln = snparser_count(pr);
          }
        }
        
        /* If autostepping, invoke step op */
        if (status && autostep) {
          if (!opStep(per)) {
            status = 0;
            *pln = snparser_count(pr);
          }
        }
        
      } else if (ent.status == SNENTITY_NUMERIC) {
        /* Push numeric literal */
        if (!pushNum(ent.pKey, per)) {
          status = 0;
          *pln = snparser_count(pr);
        }
      
      } else if (ent.status == SNENTITY_OPERATION) {
        /* Determine the kind of operation */
        if (strcmp(ent.pKey, "mul") == 0) {
          if (!opMul(per)) {
            status = 0;
            *pln = snparser_count(pr);
          }
          
        } else if (strcmp(ent.pKey, "sect") == 0) {
          if (!opSect(per)) {
            status = 0;
            *pln = snparser_count(pr);
          }
          
        } else if (strcmp(ent.pKey, "step") == 0) {
          if (!opStep(per)) {
            status = 0;
            *pln = snparser_count(pr);
          }
          
        } else if (strcmp(ent.pKey, "tempo") == 0) {
          if (!opTempo(per)) {
            status = 0;
            *pln = snparser_count(pr);
          }
          
        } else if (strcmp(ent.pKey, "ramp") == 0) {
          if (!opRamp(per)) {
            status = 0;
            *pln = snparser_count(pr);
          }
          
        } else if (strcmp(ent.pKey, "span") == 0) {
          if (!opSpan(per)) {
            status = 0;
            *pln = snparser_count(pr);
          }
          
        } else {
          /* Unrecognized operation */
          status = 0;
          *per = ERR_BADOP;
          *pln = snparser_count(pr);
        }
      
      } else {
        /* Unsupported entity type */
        status = 0;
        *per = ERR_BADENT;
        *pln = snparser_count(pr);
      }
    }
    
    /* Leave loop if error */
    if (!status) {
      break;
    }
  }
  
  /* Watch for Shastina error */
  if (status && (ent.status < 0)) {
    status = 0;
    *per = ERR_SN_MAX + ent.status;
    *pln = snparser_count(pr);
  }
  
  /* Check that nothing after the |; in the file */
  if (status) {
    retval = snsource_consume(ps);
    if (retval < 0) {
      status = 0;
      *per = ERR_SN_MAX + retval;
      *pln = -1;
    }
  }
  
  /* Check that stack is empty */
  if (status && (m_st_count > 0)) {
    status = 0;
    *per = ERR_STACKRM;
    *pln = -1;
  }
  
  /* Make sure no tempo remains buffered */
  if (status && m_tbuf_filled) {
    status = 0;
    *per = ERR_DANGLE;
    *pln = -1;
  }
  
  /* Make sure tempo map is not empty */
  if (status && (m_map_count < 1)) {
    status = 0;
    *per = ERR_EMPTY;
    *pln = -1;
  }
  
  /* Free parser if allocated */
  snparser_free(pr);
  pr = NULL;
  
  /* Free input source if allocated */
  snsource_free(ps);
  ps = NULL;
  
  /* If failure, set initialization state to -1 */
  if (!status) {
    m_map_init = -1;
  }
  
  /* Return status */
  return status;
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
 * Convert an error code return into a string.
 * 
 * The string has the first letter capitalized, but no punctuation or
 * line break at the end.
 * 
 * If the code is ERR_OK, then "No error" is returned.  If the code is
 * unrecognized, then "Unknown error" is returned.
 * 
 * Parameters:
 * 
 *   code - the error code
 * 
 * Return:
 * 
 *   an error message
 */
static const char *error_string(int code) {
  
  const char *pResult = NULL;
  
  if ((code >= ERR_SN_MIN) && (code <= ERR_SN_MAX)) {
    /* Shastina error code, so handle with Shastina */
    pResult = snerror_str(code - ERR_SN_MAX);
    
  } else {
    /* Not a Shastina error code, so handle it ourselves */
    switch (code) {
      
      case ERR_OK:
        pResult = "No error";
        break;
      
      case ERR_NMFIN:
        pResult = "Error parsing input NMF";
        break;
      
      case ERR_BASISIN:
        pResult = "Input NMF has wrong quantum basis";
        break;
      
      case ERR_XFORM:
        pResult = "Error transforming t";
        break;
      
      case ERR_NOZEROT:
        pResult = "No tempo at t=0";
        break;
      
      case ERR_NOCHRON:
        pResult = "Tempi not in chronological order";
        break;
      
      case ERR_NUMERIC:
        pResult = "Numeric computation error";
        break;
      
      case ERR_TOOMANY:
        pResult = "Too many tempi";
        break;
      
      case ERR_DANGLE:
        pResult = "Ramp tempo at end of map";
        break;
      
      case ERR_EMPTY:
        pResult = "Empty tempo map";
        break;
      
      case ERR_TYPESIG:
        pResult = "Shastina type signature missing";
        break;
      
      case ERR_BADENT:
        pResult = "Unsupported Shastina entity";
        break;
      
      case ERR_BADOP:
        pResult = "Unsupported operation";
        break;
      
      case ERR_STACKRM:
        pResult = "Items remaining on stack";
        break;
      
      case ERR_STCKFUL:
        pResult = "Interpreter stack filled";
        break;
      
      case ERR_STCKEMP:
        pResult = "Interpreter stack ran empty";
        break;
      
      case ERR_DURSTR:
        pResult = "Invalid duration string";
        break;
      
      case ERR_NUMSTR:
        pResult = "Invalid numeric literal";
        break;
      
      case ERR_OVERFL:
        pResult = "Integer overflow";
        break;
      
      case ERR_BADSEC:
        pResult = "Section number not found in input";
        break;
      
      case ERR_BADCUR:
        pResult = "Cursor position out of range";
        break;
      
      case ERR_BADRATE:
        pResult = "Invalid rate";
        break;
      
      case ERR_BADQ:
        pResult = "Invalid quanta count";
        break;
      
      case ERR_BADMIL:
        pResult = "Invalid millisecond count";
        break;
      
      default:
        pResult = "Unknown error";
    }
  }
  
  return pResult;
}

/*
 * Program entrypoint
 * ==================
 */

int main(int argc, char *argv[]) {
  
  int status = 1;
  int x = 0;
  const char *pModule = NULL;
  int32_t srate = 0;
  
  int errcode = 0;
  long lnum = 0;
  
  FILE *pMap = NULL;
  
  /* Get module name */
  if (argc > 0) {
    if (argv != NULL) {
      if (argv[0] != NULL) {
        pModule = argv[0];
      }
    }
  }
  if (pModule == NULL) {
    pModule = "nmftempo";
  }
  
  /* We must have exactly two parameters beyond the module name */
  if (argc != 3) {
    status = 0;
    fprintf(stderr, "%s: Wrong number of parameters!\n", pModule);
  }
  
  /* Check that parameters are present */
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
  
  /* Parse srate parameter */
  if (status) {
    if (!parseInt(argv[2], &srate)) {
      status = 0;
      fprintf(stderr, "%s: Can't parse srate parameter!\n", pModule);
    }
  }
  if (status) {
    if ((srate != 44100) && (srate != 48000)) {
      status = 0;
      fprintf(stderr, "%s: Invalid sampling rate!\n", pModule);
    }
  }
  
  /* Parse the input */
  if (status) {
    m_pdi = nmf_parse(stdin);
    if (m_pdi == NULL) {
      status = 0;
      fprintf(stderr, "%s: %s!\n", pModule, error_string(ERR_NMFIN));
    }
  }
  
  /* Make sure input has proper quantum basis */
  if (status) {
    if (nmf_basis(m_pdi) != NMF_BASIS_Q96) {
      status = 0;
      fprintf(stderr, "%s: %s!\n", pModule, error_string(ERR_BASISIN));
    }
  }
  
  /* Open the tempo map file */
  if (status) {
    pMap = fopen(argv[1], "r");
    if (pMap == NULL) {
      status = 0;
      fprintf(stderr, "%s: Can't open tempo map file!\n", pModule);
    }
  }
  
  /* Build the tempo map from the tempo map parameter */
  if (status) {
    if (!parseMap(pMap, srate, &errcode, &lnum)) {
      status = 0;
      if ((lnum > 0) && (lnum < LONG_MAX)) {
        fprintf(stderr, "%s: [Tempo map line %ld] %s!\n",
                pModule, lnum, error_string(errcode));
      } else {
        fprintf(stderr, "%s: [Tempo map] %s!\n",
                pModule, error_string(errcode));
      }
    }
  }
  
  /* Close tempo map file */
  if (status) {
    fclose(pMap);
    pMap = NULL;
  }
  
  /* Apply the tempo map */
  if (status) {
    if (!applyMap(m_pdi, stdout, &errcode)) {
      status = 0;
      fprintf(stderr, "%s: %s!\n", pModule, error_string(errcode));
    }
  }
  
  /* Close the tempo map file if open */
  if (pMap != NULL) {
    fclose(pMap);
    pMap = NULL;
  }
  
  /* Invert status and return */
  if (status) {
    status = 0;
  } else {
    status = 1;
  }
  return status;
}
