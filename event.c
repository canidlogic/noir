/*
 * event.c
 * 
 * Implementation of event.h
 * 
 * See the header for further information.
 */

#include "event.h"
#include <stdlib.h>
#include <string.h>

/*
 * Constants
 * =========
 */

/*
 * Module state constants.
 */
#define EVENT_STATE_NONE  (0) /* Hasn't been initialized yet */
#define EVENT_STATE_INIT  (1) /* Initialized */
#define EVENT_STATE_FINAL (2) /* Finish function has been called */

/*
 * Initial allocation capacities, in elements, of the section and note
 * tables.
 */
#define EVENT_SECTALLOC_INIT (16)
#define EVENT_NOTEALLOC_INIT (256)

/*
 * The bias used for 32-bit and 16-bit integers stored in NMF.
 */
#define EVENT_BIAS32 (INT64_C(2147483648))
#define EVENT_BIAS16 (INT32_C(32768))

/*
 * The NMF signature values.
 */
#define EVENT_SIGPRI (UINT32_C(1928196216))
#define EVENT_SIGSEC (UINT32_C(1313818926))

/*
 * Type declarations
 * =================
 */

/*
 * Representation of a note.
 */
typedef struct {
  
  /*
   * The time offset in quanta of this note.
   * 
   * This is zero or greater.
   */
  int32_t t;
  
  /*
   * The duration of this note.
   * 
   * If this is greater than zero, it is a count of quanta.
   * 
   * If this is less than zero, the absolute value is grace note offset.
   * -1 is the grace note immediately before the beat, -2 is the grace
   * note before -1, and so forth.
   * 
   * A value of zero is not allowed.
   */
  int32_t dur;
  
  /*
   * The pitch of this note.
   * 
   * This is the number of semitones (half steps) away from middle C.  A
   * value of zero is middle C, -1 is B below middle C, 2 is D above
   * middle C, and so forth.
   * 
   * The range is [NOIR_MINPITCH, NOIR_MAXPITCH].
   */
  int16_t pitch;
  
  /*
   * The articulation index of this note.
   * 
   * The range is [0, NOIR_MAXART].
   */
  uint16_t art;
  
  /*
   * The section index of this note.
   */
  uint16_t sect;
  
  /*
   * One less than the layer index of this note with the section.
   */
  uint16_t layer_i;
  
} EVENT_NOTE;

/*
 * Static data
 * ===========
 */

/*
 * Status of the module.
 * 
 * This is one of the EVENT_STATE constants.
 */
static int m_event_state = EVENT_STATE_NONE;

/*
 * The number of sections that have been defined so far.
 * 
 * Only valid if m_event_state is EVENT_STATE_INIT.
 */
static int32_t m_event_sect_count;

/*
 * The capacity of the section list.
 * 
 * Only valid if m_event_state is EVENT_STATE_INIT.
 */
static int32_t m_event_sect_cap;

/*
 * Pointer to the dynamically allocated section table.
 * 
 * Only valid if m_event_state is EVENT_STATE_INIT.
 * 
 * m_event_sect_count gives the number of sections, and the variable
 * m_event_sect_cap gives the capacity of the table.
 */
static int32_t *m_event_sect_t;

/*
 * The number of notes that have been defined so far.
 * 
 * Only valid if m_event_state is EVENT_STATE_INIT.
 */
static int32_t m_event_note_count;

/*
 * The capacity of the note table, in records.
 * 
 * Only valid if m_event_state is EVENT_STATE_INIT.
 */
static int32_t m_event_note_cap;

/*
 * Pointer to the dynamically allocated note table.
 * 
 * Only valid if m_event_state is EVENT_STATE_INIT.
 * 
 * m_event_note_count gives the number of notes, and the variable
 * m_event_note_cap gives the capacity of the table.
 */
static EVENT_NOTE *m_event_note_t;

/*
 * Local functions
 * ===============
 */

/* Prototypes */
static void event_xchg(int32_t a, int32_t b);
static int event_cmp(int32_t a, int32_t b);
static void event_writeByte(FILE *pOut, int c);
static void event_writeUint32(FILE *pOut, uint32_t v);
static void event_writeUint16(FILE *pOut, uint16_t v);
static void event_writeBias32(FILE *pOut, int32_t v);
static void event_writeBias16(FILE *pOut, int16_t v);
static void event_init(void);

/*
 * Swap two note events.
 * 
 * a and b are the indices of the note events to swap.  If a and b are
 * equal, the function does nothing.
 * 
 * Parameters:
 * 
 *   a - the index of the first note event
 * 
 *   b - the index of the second note event
 */
static void event_xchg(int32_t a, int32_t b) {
  
  EVENT_NOTE n;
  
  /* Initialize structure */
  memset(&n, 0, sizeof(EVENT_NOTE));
  
  /* Initialize data if necessary */
  event_init();
  
  /* Check parameters */
  if ((a < 0) || (b < 0) ||
      (a >= m_event_note_count) || (b >= m_event_note_count)) {
    abort();
  }
  
  /* Only proceed if a not equal to b */
  if (a != b) {
  
    /* Copy a to temporary structure */
    memcpy(&n, &(m_event_note_t[a]), sizeof(EVENT_NOTE));
  
    /* Copy b to a */
    memcpy(
      &(m_event_note_t[a]), &(m_event_note_t[b]), sizeof(EVENT_NOTE));
    
    /* Copy temporary structure to b */
    memcpy(&(m_event_note_t[b]), &n, sizeof(EVENT_NOTE));
  }
}

/*
 * Compare two note events to each other.
 * 
 * The comparison is less than, equals, or greater than according to the
 * sorting order required for NMF output.  This sorting order is
 * primarily by time offset, with a secondary sort for duration.
 * 
 * Note that events that have the same time offset and duration are
 * "equal" according to this comparison, even though they may have
 * different pitches, articulations, sections, and layers!
 * 
 * Parameters:
 * 
 *   a - the index of the first note event
 * 
 *   b - the index of the second note event
 * 
 * Return:
 * 
 *   less than, equal to, or greater than zero as note event a is less
 *   than, equal to, or greater than note event b
 */
static int event_cmp(int32_t a, int32_t b) {
  
  int result = 0;
  const EVENT_NOTE *pa = NULL;
  const EVENT_NOTE *pb = NULL;
  
  /* Initialize data if necessary */
  event_init();
  
  /* Check parameters */
  if ((a < 0) || (b < 0) ||
      (a >= m_event_note_count) || (b >= m_event_note_count)) {
    abort();
  }
  
  /* Get pointers to a and b */
  pa = &(m_event_note_t[a]);
  pb = &(m_event_note_t[b]);
  
  /* Try first to compare time offsets */
  if (pa->t < pb->t) {
    /* First t less than second t */
    result = -1;
    
  } else if (pa->t > pb->t) {
    /* First t greater than second t */
    result = 1;
    
  } else if (pa->t == pb->t) {
    
    /* Both events have same t; compare durations next */
    if (pa->dur < pb->dur) {
      /* Same t, first event lower duration */
      result = -1;
      
    } else if (pa->dur > pb->dur) {
      /* Same t, first event higher duration */
      result = 1;
      
    } else if (pa->dur == pb->dur) {
      /* Same t, same dur */
      result = 0;
      
    } else {
      /* Shouldn't happen */
      abort();
    }
    
  } else {
    /* Shouldn't happen */
    abort();
  }
  
  /* Return result */
  return result;
}

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
static void event_writeByte(FILE *pOut, int c) {
  
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
 * This is a wrapper around event_writeByte() which writes the integer
 * value as four bytes with most significant byte first and least
 * significant byte last.
 * 
 * Parameters:
 * 
 *   pOut - output file to write to
 * 
 *   v - the value to write
 */
static void event_writeUint32(FILE *pOut, uint32_t v) {
  
  event_writeByte(pOut, (int) (v >> 24));
  event_writeByte(pOut, (int) ((v >> 16) & 0xff));
  event_writeByte(pOut, (int) ((v >> 8) & 0xff));
  event_writeByte(pOut, (int) (v & 0xff));
}

/*
 * Write an unsigned 16-bit integer value to the given file in big
 * endian order.
 * 
 * This is a wrapper around event_writeByte() which writes the integer
 * value as two bytes with most significant byte first and least
 * significant byte second.
 * 
 * Parameters:
 * 
 *   pOut - output file to write to
 * 
 *   v - the value to write
 */
static void event_writeUint16(FILE *pOut, uint16_t v) {
  
  event_writeByte(pOut, (int) (v >> 8));
  event_writeByte(pOut, (int) (v & 0xff));
}

/*
 * Write a biased 32-bit signed integer value to the given file in big
 * endian order.
 * 
 * This is a wrapper around event_writeUint32() which writes a raw value
 * that is the given signed value plus EVENT_BIAS32.  This biased value
 * must be in unsigned 32-bit range or a fault occurs.
 * 
 * Parameters:
 * 
 *   pOut - output file to write to
 * 
 *   v - the value to write
 */
static void event_writeBias32(FILE *pOut, int32_t v) {
  
  int64_t rv = 0;
  
  /* Compute raw value */
  rv = ((int64_t) v) + EVENT_BIAS32;
  
  /* If inside unsigned 32-bit range, pass through; else, fault */
  if ((rv >= 0) && (rv <= UINT32_MAX)) {
    event_writeUint32(pOut, (uint32_t) rv);
  } else {
    abort();
  }
}

/*
 * Write a biased 16-bit signed integer value to the given file in big
 * endian order.
 * 
 * This is a wrapper around event_writeUint16() which writes a raw value
 * that is the given signed value plus EVENT_BIAS16.  This biased value
 * must be in unsigned 16-bit range or a fault occurs.
 * 
 * Parameters:
 * 
 *   pOut - output file to write to
 * 
 *   v - the value to write
 */
static void event_writeBias16(FILE *pOut, int16_t v) {
  
  int32_t rv = 0;
  
  /* Compute raw value */
  rv = ((int32_t) v) + EVENT_BIAS16;
  
  /* If inside unsigned 16-bit range, pass through; else, fault */
  if ((rv >= 0) && (rv <= UINT16_MAX)) {
    event_writeUint16(pOut, (uint16_t) rv);
  } else {
    abort();
  }
}

/*
 * Initialize the module, if necessary.
 * 
 * A fault occurs if this is called when the state is FINAL.
 * 
 * Upon return, the state of the module is guaranteed to be INIT.
 */
static void event_init(void) {
  
  /* Only proceed if not already initialized */
  if (m_event_state != EVENT_STATE_INIT) {
    
    /* If not initialized, we have to be in NONE state, or else fault */
    if (m_event_state != EVENT_STATE_NONE) {
      abort();
    }
    
    /* Allocate the tables with initial capacities */
    m_event_sect_t = (int32_t *) calloc(
                                  EVENT_SECTALLOC_INIT,
                                  sizeof(int32_t));
    if (m_event_sect_t == NULL) {
      abort();
    }
    
    m_event_note_t = (EVENT_NOTE *) calloc(
                                      EVENT_NOTEALLOC_INIT,
                                      sizeof(EVENT_NOTE));
    if (m_event_note_t == NULL) {
      abort();
    }
    
    /* First section begins at offset zero */
    m_event_sect_t[0] = 0;
    
    /* Initialize the rest of the variables */
    m_event_sect_count = 1;
    m_event_sect_cap = EVENT_SECTALLOC_INIT;
    m_event_note_count = 0;
    m_event_note_cap = EVENT_NOTEALLOC_INIT;
    
    /* Update state */
    m_event_state = EVENT_STATE_INIT;
  }
}

/*
 * Public function implementations
 * ===============================
 * 
 * See the header for specifications.
 */

/*
 * event_section function.
 */
int event_section(int32_t offset) {
  
  int status = 1;
  int32_t newcap = 0;
  
  /* Make sure module initialized */
  event_init();
  
  /* Make sure offset is greater than or equal to offset of current last
   * section (the first section is defined automatically right away, so
   * there will always be at least one section) */
  if (offset < m_event_sect_t[m_event_sect_count - 1]) {
    abort();
  }
  
  /* Only proceed if room for another section; otherwise, fail */
  if (m_event_sect_count < EVENT_MAXSECT) {
    
    /* We have room for another section -- check if capacity of section
     * table needs to be expanded */
    if (m_event_sect_count >= m_event_sect_cap) {
      
      /* We need to expand capacity -- first, try doubling */
      newcap = m_event_sect_cap * 2;
      
      /* If doubling exceeds maximum section count, set to maximum
       * section count */
      if (newcap > EVENT_MAXSECT) {
        newcap = EVENT_MAXSECT;
      }
      
      /* Reallocate with the new capacity */
      m_event_sect_t = realloc(
                          m_event_sect_t,
                          newcap * sizeof(int32_t));
      if (m_event_sect_t == NULL) {
        abort();
      }
      memset(
          &(m_event_sect_t[m_event_sect_cap]),
          0,
          (newcap - m_event_sect_cap) * sizeof(int32_t));
      
      /* Update capacity variable */
      m_event_sect_cap = newcap;
    }
    
    /* Add the new section to the table */
    m_event_sect_t[m_event_sect_count] = offset;
    m_event_sect_count++;
    
  } else {
    /* Section table is full */
    status = 0;
  }
  
  /* Return status */
  return status;
}

/*
 * event_note function.
 */
int event_note(
    int32_t t,
    int32_t dur,
    int32_t pitch,
    int32_t art,
    int32_t sect,
    int32_t layer) {
  
  int status = 1;
  int32_t newcap = 0;
  EVENT_NOTE *pn = NULL;
  
  /* Make sure module initialized */
  event_init();
  
  /* Check parameters */
  if ((dur == 0) || (dur < -(INT32_MAX))) {
    abort();
  }
  if ((pitch < NOIR_MINPITCH) || (pitch > NOIR_MAXPITCH)) {
    abort();
  }
  if ((art < 0) || (art > NOIR_MAXART)) {
    abort();
  }
  if ((sect < 0) || (sect >= m_event_sect_count)) {
    abort();
  }
  if ((layer < 1) || (layer > NOIR_MAXLAYER)) {
    abort();
  }
  if (t < m_event_sect_t[sect]) {
    abort();
  }
  
  /* Only proceed if room for another note; otherwise, fail */
  if (m_event_note_count < EVENT_MAXNOTE) {
    
    /* We have room for another note -- check if capacity of note table
     * needs to be expanded */
    if (m_event_note_count >= m_event_note_cap) {
      
      /* We need to expand capacity -- first, try doubling */
      newcap = m_event_note_cap * 2;
      
      /* If doubling exceeds maximum note count, set to maximum note
       * count */
      if (newcap > EVENT_MAXNOTE) {
        newcap = EVENT_MAXNOTE;
      }
      
      /* Reallocate with the new capacity */
      m_event_note_t = realloc(
                          m_event_note_t,
                          newcap * sizeof(EVENT_NOTE));
      if (m_event_note_t == NULL) {
        abort();
      }
      memset(
          &(m_event_note_t[m_event_note_cap]),
          0,
          (newcap - m_event_note_cap) * sizeof(EVENT_NOTE));
      
      /* Update capacity variable */
      m_event_note_cap = newcap;
    }
    
    /* Add the new note to the table */
    pn = &(m_event_note_t[m_event_note_count]);
    pn->t = t;
    pn->dur = dur;
    pn->pitch = (int16_t) pitch;
    pn->art = (uint16_t) art;
    pn->sect = (uint16_t) sect;
    pn->layer_i = (uint16_t) (layer - 1);
    
    m_event_note_count++;
    
  } else {
    /* Note table is full */
    status = 0;
  }
  
  /* Return status */
  return status;
}

/*
 * event_flip function.
 */
void event_flip(int32_t count, int32_t max_offs) {
  
  int32_t i = 0;
  int32_t flipped = 0;
  EVENT_NOTE *pn = NULL;
  
  /* Make sure module initialized */
  event_init();
  
  /* Check parameters */
  if ((count < 0) || (max_offs < 1)) {
    abort();
  }
  if (count > m_event_note_count) {
    abort();
  }
  
  /* Only proceed if count is non-zero */
  if (count > 0) {
    
    /* Go through the relevant note events */
    for(i = 1; i <= count; i++) {
      
      /* Get a pointer to the current note */
      pn = &(m_event_note_t[m_event_note_count - i]);
      
      /* Fault if current event note not a grace note */
      if (pn->dur >= 0) {
        abort();
      }
      
      /* Compute the flipped grace note offset */
      flipped = (max_offs + 1) + pn->dur;
      
      /* If flipped value is less than one, grace note offset exceeded
       * max_offs, so fault */
      if (flipped < 1) {
        abort();
      }
      
      /* The flipped duration is the negated value of flipped */
      pn->dur = -(flipped);
    }
  }
}

/*
 * event_finish function.
 */
int event_finish(FILE *pf) {
  
  int status = 1;
  int32_t i = 0;
  EVENT_NOTE *pn = NULL;
  
  /* Make sure module initialized */
  event_init();
  
  /* Check parameter */
  if (pf == NULL) {
    abort();
  }
  
  /* Only proceed if at least one note */
  if (m_event_note_count > 0) {
    
    /* Sort the note events */
    /* @@TODO: */
    
    /* Write the NMF signatures */
    event_writeUint32(pf, EVENT_SIGPRI);
    event_writeUint32(pf, EVENT_SIGSEC);
    
    /* Write the section count and note counts */
    event_writeUint32(pf, (uint32_t) m_event_sect_count);
    event_writeUint32(pf, (uint32_t) m_event_note_count);
    
    /* Write the section table */
    for(i = 0; i < m_event_sect_count; i++) {
      event_writeUint32(pf, (uint32_t) m_event_sect_t[i]);
    }
    
    /* Write each note */
    for(i = 0; i < m_event_note_count; i++) {
      
      /* Get pointer to current note */
      pn = &(m_event_note_t[i]);
      
      /* Serialize the structure */
      event_writeUint32(pf, (uint32_t) pn->t);
      event_writeBias32(pf, pn->dur);
      event_writeBias16(pf, pn->pitch);
      event_writeUint16(pf, pn->art);
      event_writeUint16(pf, pn->sect);
      event_writeUint16(pf, pn->layer_i);
    }
    
    /* Set state to FINAL */
    m_event_state = EVENT_STATE_FINAL;
  
  } else {
    /* No notes have been defined */
    status = 0;
  }
  
  /* Return status */
  return status;
}
