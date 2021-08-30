/*
 * entity.c
 * 
 * Implementation of entity.h
 * 
 * See the header for further information.
 */

#include "entity.h"
#include "nvm.h"
#include "token.h"

#include <stdlib.h>
#include <string.h>

/*
 * Static data
 * ===========
 */

/*
 * Flag indicating whether the module has run yet.
 */
static int m_entity_ran = 0;

/*
 * Local functions
 * ===============
 */

/* Prototypes */
static int entity_onepitch(
          NVM_PITCHSET * ps,
    const char         * pstr,
          int          * per);
static int32_t entity_onedur(const char *pstr, int *per);

static int entity_pitch(TOKEN *ptk, int *per);
static int entity_dur(TOKEN *ptk, int *per);
static int entity_op(const char *pstr, int *per);

/*
 * Interpret a pitch token, possibly reading further tokens in the case
 * of a pitch set.
 * 
 * ptk points to the token structure containing the pitch token that was
 * read.  Additional tokens might be read with this token structure.
 * 
 * per points to a variable to receive an error code in case of error.
 * 
 * Parameters:
 * 
 *   ptk - pointer to pitch token
 * 
 *   per - pointer to variable to receive error code
 * 
 * Return:
 * 
 *   non-zero if successful, zero if error
 */
static int entity_pitch(TOKEN *ptk, int *per) {
  
  NVM_PITCHSET pset;
  int c = 0;
  int status = 1;
  int32_t depth = 0;
  
  /* Initialize structure */
  nvm_pitchset_clear(&pset);
  
  /* Check whether we have a single pitch, a rest, or a pitch set */
  c = (ptk->str)[0];
  
  if ((c == ASCII_R_UPPER) || (c == ASCII_R_LOWER)) {
    /* We have a rest, so report the empty pitch set */
    if (!nvm_pset(&pset, per)) {
      status = 0;
    }
    
  } else if (c == ASCII_LPAREN) {
    /* We have a pitch set -- set the nesting depth to one */
    depth = 1;
    
    /* Keep going until we have left the group */
    while (depth > 0) {
      
      /* Read another token */
      if (!token_read(ptk)) {
        status = 0;
        *per = ptk->status;
      }
      
      /* Interpret the token */
      if (status) {
        c = (ptk->str)[0];
        
        if (c == ASCII_LPAREN) {
          /* Left parenthesis -- increase nesting depth, watching for
           * overflow */
          if (depth < INT32_MAX) {
            depth++;
          } else {
            status = 0;
            *per = ERR_TOODEEP;
          }
          
        } else if (c == ASCII_RPAREN) {
          /* Right parenthesis -- decrease nesting depth; if depth goes
           * down to zero, leave loop */
          depth--;
          if (depth < 1) {
            break;
          }
          
        } else if ((c == ASCII_R_LOWER) || (c == ASCII_R_UPPER)) {
          /* Rest -- does nothing within a pitch set */
          
        } else if (((c >= ASCII_A_LOWER) && (c <= ASCII_G_LOWER)) ||
                    ((c >= ASCII_A_UPPER) && (c <= ASCII_G_UPPER))) {
          /* Individual pitch token -- add it to the set */
          if (!entity_onepitch(&pset, ptk->str, per)) {
            status = 0;
          }
        
        } else {
          /* Non-pitch token in pitch group */
          status = 0;
          *per = ERR_UNCLOSED;
        }
      }
      
      /* Leave loop if error */
      if (!status) {
        break;
      }
    }
    
    /* Report the full pitch set */
    if (status) {
      if (!nvm_pset(&pset, per)) {
        status = 0;
      }
    }
  
  } else if (c == ASCII_RPAREN) {
    /* This shouldn't happen */
    abort();
  
  } else {
    /* We have a single pitch -- add it to pitch set */
    if (!entity_onepitch(&pset, ptk->str, per)) {
      status = 0;
    }
    
    /* Report the single pitch */
    if (status) {
      if (!nvm_pset(&pset, per)) {
        status = 0;
      }
    }
  }
  
  /* Return status */
  return status;
}

/* @@TODO: */
#include <stdio.h>
static int entity_onepitch(
          NVM_PITCHSET * ps,
    const char         * pstr,
          int          * per) {
  fprintf(stderr, "entity_onepitch %s\n", pstr);
  return 1;
}
static int32_t entity_onedur(const char *pstr, int *per) {
  fprintf(stderr, "entity_onedur %s\n", pstr);
  return 1;
}
static int entity_dur(TOKEN *ptk, int *per) {
  fprintf(stderr, "entity_dur %s\n", ptk->str);
  return 1;
}
static int entity_op(const char *pstr, int *per) {
  fprintf(stderr, "entity_op %s\n", pstr);
  return 1;
}

/*
 * Public function implementations
 * ===============================
 * 
 * See the header for specifications.
 */

/*
 * entity_run function.
 */
int entity_run(int32_t *pln, int *per) {

  TOKEN tk;
  int retval = 0;
  int status = 1;
  int c = 0;
  
  /* Initialize structure */
  memset(&tk, 0, sizeof(TOKEN));
  
  /* Check state and update it */
  if (m_entity_ran) {
    abort();
  } else {
    m_entity_ran = 1;
  }
  
  /* Check parameters */
  if ((pln == NULL) || (per == NULL)) {
    abort();
  }
  
  /* Go through all tokens except EOF */
  for(retval = token_read(&tk);
      retval && ((tk.str)[0] != 0);
      retval = token_read(&tk)) {
    
    /* Get first character of token */
    c = (tk.str)[0];
    
    /* We can't have closing groups on top level */
    if (status && ((c == ASCII_RPAREN) || (c == ASCII_RSQUARE))) {
      status = 0;
      *pln = tk.line;
      *per = ERR_RIGHT;
    }
    
    /* First character determines what to do -- either pitch entity,
     * duration entity, or operator */
    if (status) {
      if ((c == ASCII_LPAREN) || (c == ASCII_R_UPPER) ||
          (c == ASCII_R_LOWER) ||
          ((c >= ASCII_A_LOWER) && (c <= ASCII_G_LOWER)) ||
          ((c >= ASCII_A_UPPER) && (c <= ASCII_G_UPPER))) {
        
        /* Interpret pitch entity */
        if (!entity_pitch(&tk, per)) {
          status = 0;
          *pln = tk.line;
        }
        
      } else if ((c == ASCII_LSQUARE) ||
                  ((c >= ASCII_ZERO) && (c <= ASCII_NINE))) {
        
        /* Interpret duration entity */
        if (!entity_dur(&tk, per)) {
          status = 0;
          *pln = tk.line;
        }
        
      } else {
        /* Interpret operator */
        if (!entity_op(tk.str, per)) {
          status = 0;
          *pln = tk.line;
        }
      }
    }
    
    /* Leave loop if error */
    if (!status) {
      break;
    }
  }
  
  /* If token reading failed, record error */
  if (status && (!retval)) {
    status = 0;
    *pln = tk.line;
    *per = tk.status;
  }
  
  /* If we got here successfully, report EOF */
  if (status) {
    if (!nvm_eof(per)) {
      status = 0;
      *pln = tk.line;
    }
  }
  
  /* Return status */
  return status;
}
