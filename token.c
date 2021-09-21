/*
 * token.c
 * 
 * Implementation of token.h
 * 
 * See the header for further information.
 */

#include "token.h"
#include <stdlib.h>
#include <string.h>

/*
 * Static data
 * ===========
 */

/*
 * Flag indicating whether the module has been initialized.
 */
static int m_token_init = 0;

/*
 * Flag indicating whether about to read the first byte.
 * 
 * This is used to allow for UTF-8 BOM detection.
 * 
 * Only valid if m_token_init is non-zero.
 */
static int m_token_first = 1;

/*
 * The previous byte read, or zero if EOF, or -1 if no bytes read yet.
 * 
 * Only valid if m_token_init is non-zero.
 */
static int m_token_prev = -1;

/*
 * The line number counter.
 * 
 * Only valid if m_token_init is non-zero.
 */
static int32_t m_token_line = 1;

/*
 * Pushback register.
 * 
 * Only valid if m_token_init is non-zero.
 * 
 * -1 if pushback register is empty.
 */
static int m_token_pushback = -1;

/*
 * The input file.
 * 
 * Only valid if m_token_init is non-zero.
 */
static FILE *m_token_pIn = NULL;

/*
 * Local functions
 * ===============
 */

/* Prototypes */
static int token_readByteFilter(int *per);
static int token_readByteFinal(int *per);
static void token_pushback(int c);

static int token_isWhitespace(int c);
static int token_isPrinting(int c);
static int token_isSuffix(int c);
static int token_isAccidental(int c);

static int token_isAtomic(int c);
static int token_isPitchString(int c);
static int token_isRhythmString(int c);
static int token_isParamOp(int c);
static int token_isKeyOp(int c);

/*
 * Read a byte from input, with some basic filters.
 * 
 * The module must be initialized or a fault occurs.
 * 
 * This function uses the m_token_first flag to determine whether the
 * very first byte is being read.  If the very first byte is 0xEF, then
 * there must be two more bytes and they must be 0xBB and 0xBF, forming
 * a UTF-8 Byte Order Mark (BOM), which is then discarded and ignored.
 * If the first byte is 0xEF but it isn't part of a UTF-8 BOM, then
 * there is an invalid character error.
 * 
 * This function also makes sure that no literal nul (0x0) characters
 * are present in input, causing an invalid character error in those
 * cases.
 * 
 * This function keeps track of m_token_prev as a buffer for the
 * previous character read.  This is used to convert all line breaks to
 * LF-only style.
 * 
 * The filter therefore removes an optional UTF-8 BOM from the beginning
 * of input, makes sure nul is not present in the input data, and
 * converts line breaks to LF-only.
 * 
 * per points to a variable to receive the error status on error.  End
 * Of File (EOF) is represented as a successful read of a terminating
 * nul character.
 * 
 * Parameters:
 * 
 *   per - pointer to error variable
 * 
 * Return:
 * 
 *   the next filtered byte read (1-255), or zero if EOF, or -1 if error
 */
static int token_readByteFilter(int *per) {
  
  int status = 1;
  int c = 0;
  
  /* Check state */
  if (!m_token_init) {
    abort();
  }
  
  /* Check parameter */
  if (per == NULL) {
    abort();
  }
  
  /* Read a byte */
  c = getc(m_token_pIn);
  
  /* If we read terminating nul, error */
  if (c == 0) {
    *per = ERR_NULCHAR;
    status = 0;
  }
  
  /* Check for I/O error and EOF */
  if (status) {
    if (c == EOF) {
      if (feof(m_token_pIn)) {
        /* End Of File (EOF) */
        c = 0;
      
      } else {
        /* I/O error */
        *per = ERR_IOREAD;
        status = 0;
      }
    }
  }
  
  /* Special handling if very first byte */
  if (status && m_token_first) {
    
    /* Check if UTF-8 BOM */
    if (c == 0xef) {
      /* UTF-8 BOM, so make sure we read the rest of it */
      if (getc(m_token_pIn) != 0xbb) {
        *per = ERR_BADCHAR;
        status = 0;
      }
      if (status) {
        if (getc(m_token_pIn) != 0xbf) {
          *per = ERR_BADCHAR;
          status = 0;
        }
      }
      
      /* Clear first character flag */
      m_token_first = 0;
      
      /* If we read the BOM successfully, recursively call this function
       * to read whatever comes after the BOM */
      if (status) {
        return token_readByteFilter(per);
      }
      
    } else {
      /* No UTF-8 BOM, so just clear flag and continue on */
      m_token_first = 0;
    }
  }
  
  /* If the current character is an LF and the previous character was a
   * CR, or the current character is a CR and the previous character was
   * an LF, then clear the previous character register and recursively
   * call the function again to read whatever is after the line break
   * pair */
  if (status && (
        ((c == ASCII_LF) && (m_token_prev == ASCII_CR)) ||
        ((c == ASCII_CR) && (m_token_prev == ASCII_LF))
      )) {
    m_token_prev = -1;
    return token_readByteFilter(per);
  }
  
  /* Update previous character register */
  if (status) {
    m_token_prev = c;
  }
  
  /* Convert CR to LF */
  if (status && (c == ASCII_CR)) {
    c = ASCII_LF;
  }
  
  /* Set character to -1 if error */
  if (!status) {
    c = -1;
  }
  
  /* Return character, or zero for EOF, or -1 for error */
  return c;
}

/*
 * Read a byte from input, with all filters applied.
 * 
 * The module must be initialized or a fault occurs.
 * 
 * This is a wrapper around token_readByteFilter(), so it has the BOM,
 * nul, and line break filtering functionality of that function.
 * 
 * This function will also update the line counter appropriately,
 * incrementing it on each filtered LF.
 * 
 * Furthermore, if this function reads a "#" character, it will discard
 * that character and all characters up to but excluding the next
 * filtered LF or the End Of File (EOF), whichever comes first.  This
 * has the effect of removing comments from input.
 * 
 * Finally, if the pushback register is not empty, this function simply
 * returns that register content and clears the register.
 * 
 * per points to a variable to receive the error status on error.  End
 * Of File (EOF) is represented as a successful read of a terminating
 * nul character.
 * 
 * Parameters:
 * 
 *   per - pointer to error variable
 * 
 * Return:
 * 
 *   the next filtered byte read (1-255), or zero if EOF, or -1 if error
 */
static int token_readByteFinal(int *per) {
  
  int c = 0;
  
  /* Check state */
  if (!m_token_init) {
    abort();
  }
  
  /* Check parameter */
  if (per == NULL) {
    abort();
  }
  
  /* If pushback value, just return that */
  if (m_token_pushback >= 0) {
    c = m_token_pushback;
    m_token_pushback = -1;
    return c;
  }
  
  /* Call through to filter function */
  c = token_readByteFilter(per);
  
  /* If we read a number sign, then discard everything until we get an
   * error, EOF, or an LF */
  if (c == ASCII_NUMSIGN) {
    for(c = token_readByteFilter(per);
        (c > 0) && (c != ASCII_LF);
        c = token_readByteFilter(per));
  }
  
  /* If we read an LF, update line count, watching for overflow */
  if (c == ASCII_LF) {
    if (m_token_line < INT32_MAX) {
      m_token_line++;
    } else {
      *per = ERR_OVERLINE;
      c = -1;
    }
  }
  
  /* Return c */
  return c;
}

/*
 * Set a value in the pushback register that will be read on the next
 * call to token_readByteFinal().
 * 
 * c must be in range 0-255 and the module must be initialized.
 * 
 * Parameters:
 * 
 *   c - the byte value to push back
 */
static void token_pushback(int c) {
  
  /* Check state */
  if (!m_token_init) {
    abort();
  }
  
  /* Check parameter */
  if ((c < 0) || (c > 255)) {
    abort();
  }
  
  /* Set register */
  m_token_pushback = c;
}

/*
 * Determine whether the given character qualifies as whitespace.
 * 
 * Parameters:
 * 
 *   c - the character to check
 * 
 * Return:
 * 
 *   non-zero if whitespace, zero if not
 */
static int token_isWhitespace(int c) {
  
  int result = 0;
  
  if ((c == ASCII_SP) || (c == ASCII_HT) ||
      (c == ASCII_LF) || (c == ASCII_CR)) {
    result = 1;
  }
  
  return result;
}

/*
 * Determine whether a given character is a printing character.
 * 
 * Whitespace characters are never printing characters.
 * 
 * Parameters:
 * 
 *   c - the character to check
 * 
 * Return:
 * 
 *   non-zero if printing, zero if not
 */
static int token_isPrinting(int c) {
  
  int result = 0;
  
  if ((c >= ASCII_MINPRNT) && (c <= ASCII_MAXPRNT)) {
    result = 1;
  }
  
  return result;
}

/*
 * Determine whether the given character is a pitch register suffix or
 * rhythmic duration suffix.
 * 
 * Parameters:
 * 
 *   c - the character to check
 * 
 * Return:
 * 
 *   non-zero if yes, zero if not
 */
static int token_isSuffix(int c) {
  
  int result = 0;
  
  if ((c == ASCII_APOS) || (c == ASCII_COMMA) || (c == ASCII_PERIOD)) {
    result = 1;
  }
  
  return result;
}

/*
 * Determine whether the given character is a pitch accidental.
 * 
 * Parameters:
 * 
 *   c - the character to check
 * 
 * Return:
 * 
 *   non-zero if yes, zero if not
 */
static int token_isAccidental(int c) {
  
  int result = 0;
  
  if ((c == ASCII_X_LOWER) || (c == ASCII_X_UPPER) ||
      (c == ASCII_S_LOWER) || (c == ASCII_S_UPPER) ||
      (c == ASCII_N_LOWER) || (c == ASCII_N_UPPER) ||
      (c == ASCII_H_LOWER) || (c == ASCII_H_UPPER) ||
      (c == ASCII_T_LOWER) || (c == ASCII_T_UPPER)) {
    result = 1;
  }
  
  return result;
}

/*
 * Determine whether the given character is atomic, meaning it stands by
 * itself.
 * 
 * Parameters:
 * 
 *   c - the character to check
 * 
 * Return:
 * 
 *   non-zero if yes, zero if not
 */
static int token_isAtomic(int c) {
  
  int result = 0;
  
  if ((c == ASCII_LPAREN)  || (c == ASCII_RPAREN)  ||
      (c == ASCII_R_UPPER) || (c == ASCII_R_LOWER) ||
      (c == ASCII_LSQUARE) || (c == ASCII_RSQUARE) ||
      (c == ASCII_SLASH)   || (c == ASCII_DOLLAR)  ||
      (c == ASCII_ATSIGN)  || (c == ASCII_LCURLY)  ||
      (c == ASCII_COLON)   || (c == ASCII_RCURLY)  ||
      (c == ASCII_EQUALS)  || (c == ASCII_TILDE)   ||
      (c == ASCII_HYPHEN)) {
    result = 1;
  }
  
  return result;
}

/*
 * Determine whether the given character is the beginning of a pitch
 * definition string.
 * 
 * Parameters:
 * 
 *   c - the character to check
 * 
 * Return:
 * 
 *   non-zero if yes, zero if not
 */
static int token_isPitchString(int c) {
  
  int result = 0;
  
  if (((c >= ASCII_A_UPPER) && (c <= ASCII_G_UPPER)) ||
      ((c >= ASCII_A_LOWER) && (c <= ASCII_G_LOWER))) {
    result = 1;
  }
  
  return result;
}

/*
 * Determine whether the given character is the beginning of a rhythm
 * definition string.
 * 
 * Parameters:
 * 
 *   c - the character to check
 * 
 * Return:
 * 
 *   non-zero if yes, zero if not
 */
static int token_isRhythmString(int c) {
  
  int result = 0;
  
  if ((c >= ASCII_ZERO) && (c <= ASCII_NINE)) {
    result = 1;
  }
  
  return result;
}

/*
 * Determine whether the given character is an operator that takes a
 * parameter ended with a semicolon.
 * 
 * Parameters:
 * 
 *   c - the character to check
 * 
 * Return:
 * 
 *   non-zero if yes, zero if not
 */
static int token_isParamOp(int c) {
  
  int result = 0;
  
  if ((c == ASCII_BSLASH) || (c == ASCII_CARET) ||
      (c == ASCII_AMP)    || (c == ASCII_PLUS) ||
      (c == ASCII_GRACC)) {
    result = 1;
  }
  
  return result;
}

/*
 * Determine whether the given character is an operator that takes a
 * single key character parameter.
 * 
 * Parameters:
 * 
 *   c - the character to check
 * 
 * Return:
 * 
 *   non-zero if yes, zero if not
 */
static int token_isKeyOp(int c) {
  
  int result = 0;
  
  if ((c == ASCII_STAR) || (c == ASCII_EXCLAIM)) {
    result = 1;
  }
  
  return result;
}

/*
 * Public function implementations
 * ===============================
 * 
 * See the header for specifications.
 */

/*
 * token_init function.
 */
void token_init(FILE *pIn) {
  
  /* Check parameter */
  if (pIn == NULL) {
    abort();
  }
  
  /* Check state */
  if (m_token_init) {
    abort();  /* already initialized */
  }
  
  /* Initialize variables */
  m_token_init = 1;
  m_token_first = 1;
  m_token_prev = -1;
  m_token_line = 1;
  m_token_pushback = -1;
  m_token_pIn = pIn;
}

/*
 * token_read function.
 */
int token_read(TOKEN *ptk) {
  
  int status = 1;
  int errnum = ERR_OK;
  int c = 0;
  int count = 0;
  
  /* Check state */
  if (!m_token_init) {
    abort();
  }
  
  /* Check parameter */
  if (ptk == NULL) {
    abort();
  }
  
  /* Reset token structure */
  memset(ptk, 0, sizeof(TOKEN));
  ptk->status = ERR_OK;
  
  /* Read from input until we get non-whitespace, or End Of File, or an
   * error */
  for(c = token_readByteFinal(&errnum);
      (c > 0) && (token_isWhitespace(c));
      c = token_readByteFinal(&errnum));
  
  if (c < 0) {
    status = 0;
  }
  
  /* Set the line number */
  if (status) {
    ptk->line = m_token_line;
  }
  
  /* If not EOF, put character in start of token buffer, update count,
   * and read any extra token characters */
  if (status && (c > 0)) {
    
    /* Put character in buffer and update count */
    (ptk->str)[0] = (char) c;
    count++;
    
    /* We only need to read more characters if not atomic */
    if (!token_isAtomic(c)) {
      
      /* Determine non-atomic type of token */
      if (token_isPitchString(c)) {
        /* Non-atomic pitch token, so first see if accidentals to add */
        for(c = token_readByteFinal(&errnum);
            token_isAccidental(c);
            c = token_readByteFinal(&errnum)) {
          
          /* Add accidental, watching for buffer overflow */
          if (count < TOKEN_MAXCHAR - 1) {
            /* No overflow, so add character */
            (ptk->str)[count] = (char) c;
            count++;
            
          } else {
            /* Buffer overflow */
            status = 0;
            errnum = ERR_LONGTOKEN;
          }
          
          /* Leave loop if error */
          if (!status) {
            break;
          }
        }
        
        /* If we stopped on read error, clear status */
        if (status && (c < 0)) {
          status = 0;
        }
        
        /* Push back character we stopped on */
        if (status) {
          token_pushback(c);
        }
        
        /* Add any suffixes */
        if (status) {
          for(c = token_readByteFinal(&errnum);
              token_isSuffix(c);
              c = token_readByteFinal(&errnum)) {
          
            /* Add suffix, watching for buffer overflow */
            if (count < TOKEN_MAXCHAR - 1) {
              /* No overflow, so add character */
              (ptk->str)[count] = (char) c;
              count++;
            
            } else {
              /* Buffer overflow */
              status = 0;
              errnum = ERR_LONGTOKEN;
            }
          
            /* Leave loop if error */
            if (!status) {
              break;
            }
          }
        }
        
        /* If we stopped on read error, clear status */
        if (status && (c < 0)) {
          status = 0;
        }
        
        /* Push back character we stopped on */
        if (status) {
          token_pushback(c);
        }
        
      } else if (token_isRhythmString(c)) {
        /* Non-atomic rhythm token, so see if suffix character to add */
        c = token_readByteFinal(&errnum);
        if (c < 0) {
          /* Read error */
          status = 0;
        
        } else if (token_isSuffix(c)) {
          /* Suffix character, so add it */
          (ptk->str)[1] = (char) c;
          count++;
          
        } else {
          /* Not a suffix character, so push it back */
          token_pushback(c);
        }
        
      } else if (token_isParamOp(c)) {
        /* Param operation, so we need extra characters until
         * semicolon */
        for(c = token_readByteFinal(&errnum);
            (token_isPrinting(c)) && (c != ASCII_SEMICOL);
            c = token_readByteFinal(&errnum)) {
          
          /* Add character, watching for buffer overflow */
          if (count < TOKEN_MAXCHAR - 1) {
            /* No overflow, so add character */
            (ptk->str)[count] = (char) c;
            count++;
            
          } else {
            /* Buffer overflow */
            status = 0;
            errnum = ERR_LONGTOKEN;
          }
          
          /* Leave loop if error */
          if (!status) {
            break;
          }
        }
        
        /* If we stopped on read error, clear status */
        if (status && (c < 0)) {
          status = 0;
        }
        
        /* If we stopped on anything other than semicolon, error */
        if (status && (c != ASCII_SEMICOL)) {
          status = 0;
          errnum = ERR_PARAMTK;
        }
        
        /* Add semicolon, watching for overflow */
        if (status) {
          if (count < TOKEN_MAXCHAR - 1) {
            (ptk->str)[count] = (char) ASCII_SEMICOL;
            count++;
          } else {
            status = 0;
            errnum = ERR_LONGTOKEN;
          }
        }
        
      } else if (token_isKeyOp(c)) {
        /* Key operation, so we need one more printing char */
        c = token_readByteFinal(&errnum);
        if (c < 0) {
          status = 0;
        } else if (!token_isPrinting(c)) {
          status = 0;
          errnum = ERR_KEYTOKEN;
        }
        
        /* Add the extra printing character */
        if (status) {
          (ptk->str)[1] = (char) c;
          count++;
        }
        
      } else {
        /* Unrecognized character */
        errnum = ERR_BADCHAR;
        status = 0;
      }
    }
  }
  
  /* If we got an error, set structure appropriately */
  if (!status) {
    memset(ptk, 0, sizeof(TOKEN));
    ptk->status = errnum;
    ptk->line = m_token_line;
  }
  
  /* Return status */
  return status;
}
