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
static int entity_validAtomicOp(const char *pstr);
static int entity_intOp(const char *pstr, int32_t *pv);
static int32_t entity_keyOp(const char *pstr);

static int entity_onepitch(
          NVM_PITCHSET * ps,
    const char         * pstr,
          int          * per);
static int32_t entity_onedur(const char *pstr, int *per);

static int entity_pitch(TOKEN *ptk, int *per);
static int entity_dur(TOKEN *ptk, int *per);
static int entity_op(const char *pstr, int *per);

/*
 * Verify that the given token is a valid atomic operation.
 * 
 * This simply checks that the token has exactly one character.
 * 
 * Parameters:
 * 
 *   pstr - the token to validate
 * 
 * Return:
 * 
 *   non-zero if valid, zero if not
 */
static int entity_validAtomicOp(const char *pstr) {
  
  int status = 1;
  
  /* Check parameter */
  if (pstr == NULL) {
    abort();
  }
  
  /* Validate length */
  if (strlen(pstr) != 1) {
    status = 0;
  }
  
  /* Return status */
  return status;
}

/*
 * Get the signed integer parameter from the given integer parameter
 * operation token.
 * 
 * pstr is the token, and pv points to the variable to receive the
 * integer parameter if successful.
 * 
 * Parameters:
 * 
 *   pstr - the token
 * 
 *   pv - pointer to variable to receive integer parameter
 * 
 * Return:
 * 
 *   non-zero if successful, zero if invalid token
 */
static int entity_intOp(const char *pstr, int32_t *pv) {
  
  int status = 1;
  size_t slen = 0;
  int negflag = 0;
  int c = 0;
  int32_t result = 0;
  
  /* Check parameters */
  if ((pstr == NULL) || (pv == NULL)) {
    abort();
  }
  
  /* Get token length */
  slen = strlen(pstr);
  
  /* There must be at least three characters */
  if (slen < 3) {
    status = 0;
  }
  
  /* Last character must be a semicolon */
  if (status) {
    if (pstr[slen - 1] != ASCII_SEMICOL) {
      status = 0;
    }
  }
  
  /* If the second character is a sign, there must be at least four
   * characters */
  if (status) {
    if ((pstr[1] == ASCII_PLUS) || (pstr[1] == ASCII_HYPHEN)) {
      if (slen < 4) {
        status = 0;
      }
    }
  }
  
  /* Advance pointer to first character of parameter */
  if (status) {
    pstr++;
  }
  
  /* If first character is sign, update negflag and advance */
  if (status) {
    if (*pstr == ASCII_PLUS) {
      negflag = 0;
      pstr++;
    } else if (*pstr == ASCII_HYPHEN) {
      negflag = 1;
      pstr++;
    }
  }
  
  /* Parse the numeric value */
  if (status) {
    for( ; *pstr != ASCII_SEMICOL; pstr++) {
      
      /* Get current character */
      c = *pstr;
      
      /* Character must be decimal digit */
      if ((c < ASCII_ZERO) || (c > ASCII_NINE)) {
        status = 0;
      }
      
      /* Convert character to numeric value */
      if (status) {
        c = c - ASCII_ZERO;
      }
      
      /* Multiply result by ten, watching for overflow */
      if (status) {
        if (result <= INT32_MAX / 10) {
          result *= 10;
        } else {
          status = 0; /* overflow */
        }
      }
      
      /* Add current digit in, watching for overflow */
      if (status) {
        if (result <= INT32_MAX - c) {
          result += (int32_t) c;
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
  
  /* Flip sign if negflag is on */
  if (status && negflag) {
    result = -(result);
  }
  
  /* Store result if successful */
  if (status) {
    *pv = result;
  }
  
  /* Return status */
  return status;
}

/*
 * Get the articulation key from the given key operation token.
 * 
 * pstr is the token, which will be validated to have a length exactly
 * two.
 * 
 * Parameters:
 * 
 *   pstr - the token
 * 
 * Return:
 * 
 *   the numeric articulation key, or -1 if invalid token
 */
static int32_t entity_keyOp(const char *pstr) {
  
  int status = 1;
  int32_t result = 0;
  int c = 0;
  
  /* Check parameter */
  if (pstr == NULL) {
    abort();
  }
  
  /* Token must have exactly two characters */
  if (strlen(pstr) != 2) {
    status = 0;
  }
  
  /* Decode articulation key */
  if (status) {
    c = pstr[1];
    if ((c >= ASCII_ZERO) && (c <= ASCII_NINE)) {
      /* 0 through 9 */
      result = (int32_t) (c - ASCII_ZERO);
      
    } else if ((c >= ASCII_A_UPPER) && (c <= ASCII_Z_UPPER)) {
      /* A through Z */
      result = (int32_t) ((c - ASCII_A_UPPER) + 10);
      
    } else if ((c >= ASCII_A_LOWER) && (c <= ASCII_Z_LOWER)) {
      /* a through z */
      result = (int32_t) ((c - ASCII_A_LOWER) + 36);
      
    } else {
      /* Invalid articulation key */
      status = 0;
    }
  }
  
  /* If failure, set result to -1 */
  if (!status) {
    result = -1;
  }
  
  /* Return result */
  return result;
}

/*
 * Decode a single pitch and add it to the given pitch set.
 * 
 * ps is the pitch set to add the pitch to.
 * 
 * pstr points to the token to decode.  It may NOT be a pitch set
 * parenthesis or a rest.
 * 
 * per points to a variable to receive an error code in case of error.
 * 
 * Parameters:
 * 
 *   ps - the pitch set to modify
 * 
 *   pstr - the pitch token to decode
 * 
 *   per - pointer to variable to receive error code
 * 
 * Return:
 * 
 *   non-zero if successful, zero if error
 */
static int entity_onepitch(
          NVM_PITCHSET * ps,
    const char         * pstr,
          int          * per) {
  
  int c = 0;
  int32_t pitch = 0;
  int status = 1;
  
  /* Check parameters */
  if ((ps == NULL) || (pstr == NULL) || (per == NULL)) {
    abort();
  }
  
  /* Get the first character */
  c = pstr[0];
  
  /* Get the starting pitch based on the first character */
  switch (c) {
    case ASCII_C_UPPER:
      pitch = -12;
      break;
    
    case ASCII_D_UPPER:
      pitch = -10;
      break;
    
    case ASCII_E_UPPER:
      pitch = -8;
      break;
    
    case ASCII_F_UPPER:
      pitch = -7;
      break;
    
    case ASCII_G_UPPER:
      pitch = -5;
      break;
    
    case ASCII_A_UPPER:
      pitch = -3;
      break;
    
    case ASCII_B_UPPER:
      pitch = -1;
      break;
    
    case ASCII_C_LOWER:
      pitch = 0;
      break;
    
    case ASCII_D_LOWER:
      pitch = 2;
      break;
    
    case ASCII_E_LOWER:
      pitch = 4;
      break;
    
    case ASCII_F_LOWER:
      pitch = 5;
      break;
    
    case ASCII_G_LOWER:
      pitch = 7;
      break;
    
    case ASCII_A_LOWER:
      pitch = 9;
      break;
    
    case ASCII_B_LOWER:
      pitch = 11;
      break;
    
    default:
      status = 0;
      *per = ERR_BADPITCH;
  }
  
  /* Any remaining characters adjust the pitch */
  if (status) {
    for(pstr++; *pstr != 0; pstr++) {
      
      /* Get current character */
      c = *pstr;
      
      /* If uppercase letter, change to lowercase */
      if ((c >= ASCII_A_UPPER) && (c <= ASCII_Z_UPPER)) {
        c = c + (ASCII_A_LOWER - ASCII_A_UPPER);
      }
      
      /* Interpret modifier */
      switch (c) {
        
        case ASCII_X_LOWER:
          /* Double-sharp */
          if (pitch <= INT32_MAX - 2) {
            pitch += 2;
          } else {
            status = 0;
            *per = ERR_PITCHR;
          }
          break;
        
        case ASCII_S_LOWER:
          /* Sharp */
          if (pitch <= INT32_MAX - 1) {
            pitch++;
          } else {
            status = 0;
            *per = ERR_PITCHR;
          }
          break;
        
        case ASCII_N_LOWER:
          /* Natural -- do nothing */
          break;
        
        case ASCII_H_LOWER:
          /* Flat */
          if (pitch >= INT32_MIN + 1) {
            pitch--;
          } else {
            status = 0;
            *per = ERR_PITCHR;
          }
          break;
        
        case ASCII_T_LOWER:
          /* Double-flat */
          if (pitch >= INT32_MIN + 2) {
            pitch -= 2;
          } else {
            status = 0;
            *per = ERR_PITCHR;
          }
          break;
        
        case ASCII_APOS:
          /* Up one octave */
          if (pitch <= INT32_MAX - 12) {
            pitch += 12;
          } else {
            status = 0;
            *per = ERR_PITCHR;
          }
          break;
        
        case ASCII_COMMA:
          /* Down one ocatve */
          if (pitch >= INT32_MIN + 12) {
            pitch -= 12;
          } else {
            status = 0;
            *per = ERR_PITCHR;
          }
          break;
        
        default:
          /* Unrecognized modifier */
          status = 0;
          *per = ERR_BADPITCH;
      }
      
      /* Leave loop if error */
      if (!status) {
        break;
      }
    }
  }
  
  /* Make sure resulting pitch is in range */
  if (status) {
    if ((pitch < NMF_MINPITCH) || (pitch > NMF_MAXPITCH)) {
      status = 0;
      *per = ERR_PITCHR;
    }
  }
  
  /* Add pitch to set */
  if (status) {
    nvm_pitchset_add(ps, pitch);
  }
  
  /* Return status */
  return status;
}

/*
 * Decode a single duration token into a quanta count.
 * 
 * pstr points to the token to decode.  It may NOT be a rhythm group
 * bracket.
 * 
 * The return value is the number of quanta represented by the token, or
 * zero if a grace note, or -1 if an error.
 * 
 * per points to a variable to receive an error code in case of error.
 * 
 * Parameters:
 * 
 *   pstr - the duration token to decode
 * 
 *   per - pointer to variable to receive error code
 * 
 * Return:
 * 
 *   the number of quanta, or zero for grace note, or -1 for error
 */
static int32_t entity_onedur(const char *pstr, int *per) {
  
  size_t slen = 0;
  int status = 1;
  int32_t result = 0;
  
  /* Check parameters */
  if ((pstr == NULL) || (per == NULL)) {
    abort();
  }
  
  /* Get string length, excluding terminating nul */
  slen = strlen(pstr);
  
  /* String length must be either one or two */
  if ((slen < 1) || (slen > 2)) {
    status = 0;
    *per = ERR_BADDUR;
  }
  
  /* First character must be decimal integer */
  if (status) {
    if ((pstr[0] < ASCII_ZERO) || (pstr[0] > ASCII_NINE)) {
      status = 0;
      *per = ERR_BADDUR;
    }
  }
  
  /* Get numeric value of first character */
  if (status) {
    result = (pstr[0] - ASCII_ZERO);
  }
  
  /* Look up appropriate quanta value */
  if (status) {
    switch (result) {
      
      case 0:
        /* Grace note, make sure no suffix */
        if (slen > 1) {
          status = 0;
          *per = ERR_BADDUR;
        }
        break;
      
      case 1:
        /* 64th note */
        result = 6;
        break;
      
      case 2:
        /* 32nd note */
        result = 12;
        break;
      
      case 3:
        /* 16th note */
        result = 24;
        break;
      
      case 4:
        /* 8th note */
        result = 48;
        break;
      
      case 5:
        /* Quarter note */
        result = 96;
        break;
      
      case 6:
        /* Half note */
        result = 192;
        break;
      
      case 7:
        /* Whole note */
        result = 384;
        break;
      
      case 8:
        /* Eighth triplet */
        result = 32;
        break;
      
      case 9:
        /* Quarter triplet */
        result = 64;
        break;
      
      default:
        /* Shouldn't happen */
        abort();
    }
  }
    
  /* If suffix, interpret it */
  if (status && (slen > 1)) {
    if (pstr[1] == ASCII_APOS) {
      /* Apostrophe -- double duration */
      result *= 2;
      
    } else if (pstr[1] == ASCII_PERIOD) {
      /* Period -- add half a value */
      result = result + (result / 2);
      
    } else if (pstr[1] == ASCII_COMMA) {
      /* Comma -- halve the duration */
      result /= 2;
      
    } else {
      /* Unknown suffix */
      status = 0;
      *per = ERR_BADDUR;
    }
  }
  
  /* If error, set result to -1 */
  if (!status) {
    result = -1;
  }
  
  /* Return result */
  return result;
}

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
  
  /* Check parameters */
  if ((ptk == NULL) || (per == NULL)) {
    abort();
  }
  
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
  
  } else if (((c >= ASCII_A_LOWER) && (c <= ASCII_G_LOWER)) ||
              ((c >= ASCII_A_UPPER) && (c <= ASCII_G_UPPER))) {
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
  
  } else {
    /* This shouldn't happen */
    abort();
  }
  
  /* Return status */
  return status;
}

/*
 * Interpret a duration token, possibly reading further tokens in the
 * case of a rhythm group.
 * 
 * ptk points to the token structure containing the duration token that
 * was read.  Additional tokens might be read with this token structure.
 * 
 * per points to a variable to receive an error code in case of error.
 * 
 * Parameters:
 * 
 *   ptk - pointer to duration token
 * 
 *   per - pointer to variable to receive error code
 * 
 * Return:
 * 
 *   non-zero if successful, zero if error
 */
static int entity_dur(TOKEN *ptk, int *per) {
  
  int32_t dur = 0;
  int32_t d = 0;
  int c = 0;
  int status = 1;
  int32_t depth = 0;
  
  /* Check parameters */
  if ((ptk == NULL) || (per == NULL)) {
    abort();
  }
  
  /* Check whether we have a single duration or a rhythm group */
  c = (ptk->str)[0];
  
  if (c == ASCII_LSQUARE) {
    /* We have a rhythm group -- set the nesting depth to one */
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
        
        if (c == ASCII_LSQUARE) {
          /* Left square bracket -- increase nesting depth, watching for
           * overflow */
          if (depth < INT32_MAX) {
            depth++;
          } else {
            status = 0;
            *per = ERR_TOODEEP;
          }
          
        } else if (c == ASCII_RSQUARE) {
          /* Right square bracket -- decrease nesting depth; if depth
           * goes down to zero, leave loop */
          depth--;
          if (depth < 1) {
            break;
          }
          
        } else if ((c >= ASCII_ZERO) && (c <= ASCII_NINE)) {
          /* Individual duration token -- decode it first */
          d = entity_onedur(ptk->str, per);
          if (d < 0) {
            status = 0;
          }
          
          /* Grace notes not allowed within groups */
          if (status && (d == 0)) {
            status = 0;
            *per = ERR_INGRACE;
          }
          
          /* Add duration to current duration, watching for overflow */
          if (status) {
            if (d <= INT32_MAX - dur) {
              dur += d;
            } else {
              status = 0;
              *per = ERR_LONGDUR;
            }
          }
        
        } else {
          /* Non-duration token in rhythm group */
          status = 0;
          *per = ERR_UNCLOSED;
        }
      }
      
      /* Leave loop if error */
      if (!status) {
        break;
      }
    }
    
    /* Report the full duration */
    if (status) {
      if (!nvm_dur(dur, per)) {
        status = 0;
      }
    }
  
  } else if (c == ASCII_RSQUARE) {
    /* This shouldn't happen */
    abort();
  
  } else if ((c >= ASCII_ZERO) && (c <= ASCII_NINE)) {
    /* We have a single duration -- decode it */
    dur = entity_onedur(ptk->str, per);
    if (dur < 0) {
      status = 0;
    }
    
    /* Report the single duration */
    if (status) {
      if (!nvm_dur(dur, per)) {
        status = 0;
      }
    }
  
  } else {
    /* This shouldn't happen */
    abort();
  }
  
  /* Return status */
  return status;
}

/*
 * Interpret an operation token.
 * 
 * pstr points to the operation token to interpret.
 * 
 * per points to a variable to receive an error code in case of error.
 * 
 * Parameters:
 * 
 *   pstr - the operation token
 * 
 *   per - variable to receive an error code in case of error
 * 
 * Return:
 * 
 *   non-zero if successful, zero if error
 */
static int entity_op(const char *pstr, int *per) {
  
  int status = 1;
  int c = 0;
  int32_t v = 0;
  
  /* Check parameters */
  if ((pstr == NULL) || (per == NULL)) {
    abort();
  }
  
  /* Token must have at least one character */
  if (pstr[0] == 0) {
    status = 0;
    *per = ERR_BADOP;
  }
  
  /* Get first character */
  if (status) {
    c = pstr[0];
  }
  
  /* Handle specific operator */
  if (status) {
    switch (c) {
    
      case ASCII_SLASH:
        /* Repeater operation */
        if (entity_validAtomicOp(pstr)) {
          if (!nvm_op_repeat(per)) {
            status = 0;
          }
        
        } else {
          status = 0;
          *per = ERR_BADOP;
        }
        break;
    
      case ASCII_DOLLAR:
        /* Section begin operation */
        if (entity_validAtomicOp(pstr)) {
          if (!nvm_op_section(per)) {
            status = 0;
          }
          
        } else {
          status = 0;
          *per = ERR_BADOP;
        }
        break;
      
      case ASCII_ATSIGN:
        /* Return operation */
        if (entity_validAtomicOp(pstr)) {
          if (!nvm_op_return(per)) {
            status = 0;
          }
          
        } else {
          status = 0;
          *per = ERR_BADOP;
        }
        break;
    
      case ASCII_LCURLY:
        /* Push location operation */
        if (entity_validAtomicOp(pstr)) {
          if (!nvm_op_pushloc(per)) {
            status = 0;
          }
          
        } else {
          status = 0;
          *per = ERR_BADOP;
        }
        break;
      
      case ASCII_COLON:
        /* Return to location operation */
        if (entity_validAtomicOp(pstr)) {
          if (!nvm_op_retloc(per)) {
            status = 0;
          }
          
        } else {
          status = 0;
          *per = ERR_BADOP;
        }
        break;
      
      case ASCII_RCURLY:
        /* Pop location operation */
        if (entity_validAtomicOp(pstr)) {
          if (!nvm_op_poploc(per)) {
            status = 0;
          }
          
        } else {
          status = 0;
          *per = ERR_BADOP;
        }
        break;
      
      case ASCII_EQUALS:
        /* Pop transposition operation */
        if (entity_validAtomicOp(pstr)) {
          if (!nvm_op_poptrans(per)) {
            status = 0;
          }
          
        } else {
          status = 0;
          *per = ERR_BADOP;
        }
        break;
      
      case ASCII_TILDE:
        /* Pop articulation operation */
        if (entity_validAtomicOp(pstr)) {
          if (!nvm_op_popart(per)) {
            status = 0;
          }
          
        } else {
          status = 0;
          *per = ERR_BADOP;
        }
        break;
      
      case ASCII_HYPHEN:
        /* Pop layer operation */
        if (entity_validAtomicOp(pstr)) {
          if (!nvm_op_poplayer(per)) {
            status = 0;
          }
          
        } else {
          status = 0;
          *per = ERR_BADOP;
        }
        break;
      
      case ASCII_BSLASH:
        /* Multiple repeater operation */
        if (entity_intOp(pstr, &v)) {
          if (!nvm_op_multiple(v, per)) {
            status = 0;
          }
          
        } else {
          status = 0;
          *per = ERR_BADOP;
        }
        break;
    
      case ASCII_CARET:
        /* Push transposition operation */
        if (entity_intOp(pstr, &v)) {
          if (!nvm_op_pushtrans(v, per)) {
            status = 0;
          }
          
        } else {
          status = 0;
          *per = ERR_BADOP;
        }
        break;
      
      case ASCII_AMP:
        /* Set base layer operation */
        if (entity_intOp(pstr, &v)) {
          if (!nvm_op_setbase(v, per)) {
            status = 0;
          }
          
        } else {
          status = 0;
          *per = ERR_BADOP;
        }
        break;
      
      case ASCII_PLUS:
        /* Push layer operation */
        if (entity_intOp(pstr, &v)) {
          if (!nvm_op_pushlayer(v, per)) {
            status = 0;
          }
          
        } else {
          status = 0;
          *per = ERR_BADOP;
        }
        break;
      
      case ASCII_STAR:
        /* Immediate articulation operation */
        v = entity_keyOp(pstr);
        if (v >= 0) {
          if (!nvm_op_immart((int) v, per)) {
            status = 0;
          }
        
        } else {
          status = 0;
          *per = ERR_BADOP;
        }
        break;
      
      case ASCII_EXCLAIM:
        /* Push articulation operation */
        v = entity_keyOp(pstr);
        if (v >= 0) {
          if (!nvm_op_pushart((int) v, per)) {
            status = 0;
          }
        
        } else {
          status = 0;
          *per = ERR_BADOP;
        }
        break;
      
      default:
        /* Unknown operation */
        status = 0;
        *per = ERR_BADOP;
    }
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
