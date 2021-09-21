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
 * Pointer to the NMF data object.
 * 
 * Only valid if m_event_state is EVENT_STATE_INIT.
 */
static NMF_DATA *m_event_pd = NULL;

/*
 * Local functions
 * ===============
 */

/* Prototypes */
static void event_init(void);

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
    
    /* Allocate a new data object */
    m_event_pd = nmf_alloc();
    
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
  
  /* Make sure module initialized */
  event_init();
  
  /* Call through */
  return nmf_sect(m_event_pd, offset);
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
  
  NMF_NOTE n;
  
  /* Initialize structure */
  memset(&n, 0, sizeof(NMF_NOTE));
  
  /* Make sure module initialized */
  event_init();
  
  /* Check parameters */
  if (t < 0) {
    abort();
  }
  if ((dur == 0) || (dur < -(INT32_MAX))) {
    abort();
  }
  if ((pitch < NMF_MINPITCH) || (pitch > NMF_MAXPITCH)) {
    abort();
  }
  if ((art < 0) || (art > NMF_MAXART)) {
    abort();
  }
  if ((sect < 0) || (sect >= NMF_MAXSECT)) {
    abort();
  }
  if ((layer < 1) || (layer > NOIR_MAXLAYER)) {
    abort();
  }

  /* Fill in the note structure */
  n.t = t;
  n.dur = dur;
  n.pitch = (int16_t) pitch;
  n.art = (uint16_t) art;
  n.sect = (uint16_t) sect;
  n.layer_i = (uint16_t) (layer - 1);
  
  /* Call through */
  return nmf_append(m_event_pd, &n);
}

/*
 * event_cue function.
 */
int event_cue(
    int32_t t,
    int32_t sect,
    int32_t cue_num) {
  
  NMF_NOTE n;
  
  /* Initialize structure */
  memset(&n, 0, sizeof(NMF_NOTE));
  
  /* Make sure module initialized */
  event_init();
  
  /* Check parameters */
  if (t < 0) {
    abort();
  }
  if ((sect < 0) || (sect >= NMF_MAXSECT)) {
    abort();
  }
  if ((cue_num < 0) || (cue_num > NOIR_MAXCUE)) {
    abort();
  }

  /* Fill in the note structure for a cue */
  n.t = t;
  n.dur = 0;
  n.pitch = 0;
  n.art = (uint16_t) (cue_num >> 16);
  n.sect = (uint16_t) sect;
  n.layer_i = (uint16_t) (cue_num & INT32_C(0xffff));
  
  /* Call through */
  return nmf_append(m_event_pd, &n);
}

/*
 * event_flip function.
 */
void event_flip(int32_t count, int32_t max_offs) {
  
  int32_t i = 0;
  int32_t note_count = 0;
  int32_t flipped = 0;
  NMF_NOTE n;
  
  /* Initialize structures */
  memset(&n, 0, sizeof(NMF_NOTE));
  
  /* Make sure module initialized */
  event_init();
  
  /* Get note count */
  note_count = nmf_notes(m_event_pd);
  
  /* Check parameters */
  if ((count < 0) || (max_offs < 1)) {
    abort();
  }
  if (count > note_count) {
    abort();
  }
  
  /* Only proceed if count is non-zero */
  if (count > 0) {
    
    /* Go through the relevant note events */
    for(i = 1; i <= count; i++) {
      
      /* Get the current note */
      nmf_get(m_event_pd, note_count - i, &n);
      
      /* Fault if current event note not a grace note */
      if (n.dur >= 0) {
        abort();
      }
      
      /* Compute the flipped grace note offset */
      flipped = (max_offs + 1) + n.dur;
      
      /* If flipped value is less than one, grace note offset exceeded
       * max_offs, so fault */
      if (flipped < 1) {
        abort();
      }
      
      /* The flipped duration is the negated value of flipped */
      n.dur = -(flipped);
      
      /* Update the note */
      nmf_set(m_event_pd, note_count - i, &n);
    }
  }
}

/*
 * event_finish function.
 */
int event_finish(FILE *pf) {
  
  int retval = 0;
  
  /* Make sure module initialized */
  event_init();
  
  /* Check parameter */
  if (pf == NULL) {
    abort();
  }
  
  /* Call through */
  retval = nmf_serialize(m_event_pd, pf);
  
  /* Close down data object */
  nmf_free(m_event_pd);
  m_event_pd = NULL;
  
  /* Set state to FINAL */
  m_event_state = EVENT_STATE_FINAL;
  
  /* Return retval */
  return retval;
}
