/*
 * noir.c
 * ======
 * 
 * The Noir notation program compiles a textual description of music
 * into a binary NMF file that represents the composition.
 * 
 * Syntax
 * ------
 * 
 *   noir
 * 
 * The input file is read from standard input, and the NMF file is
 * written to standard output.
 * 
 * File formats
 * ------------
 * 
 * See the "Noir notation specification" for the format of the input
 * text file.
 * 
 * See libnmf for the format of the output binary file.
 * 
 * Compilation
 * -----------
 * 
 * Compile with the following modules:
 * 
 *   entity.c
 *   event.c 
 *   nvm.c
 *   token.c
 * 
 * Compile with libnmf.
 */

#include "noirdef.h"
#include "entity.h"
#include "event.h"
#include "token.h"

#include <stdio.h>
#include <stdlib.h>

/*
 * Local functions
 * ===============
 */

/* Prototypes */
static int noir(FILE *pIn, FILE *pOut, int32_t *pln, int *per);
static const char *err_string(int code);

/*
 * Compile a Noir notation file to Noir Music Format (NMF).
 * 
 * pIn is the file to read the Noir notation from.  It must be open for
 * reading and it must not be the same file as pOut or undefined
 * behavior occurs.  Reading is fully sequential.
 * 
 * pOut is the file to write the NMF file to.  It must be open for
 * writing and it must not be the same file as pIn or undefined behavior
 * occurs.  Writing is fully sequential.
 * 
 * pln is either NULL or it points to a variable to receive the line
 * number in the input in case of an error.  -1 is written to it if the
 * line number overflows, is unknown, or irrelevant, or if there is no
 * error.
 * 
 * per is either NULL or it points to a variable to receive an error
 * code.  ERR_OK is written if there is no error.  The error codes are
 * defined in noirdef, and err_string() can be used to get error 
 * messages.
 * 
 * Parameters:
 * 
 *   pIn - the input Noir notation
 * 
 *   pOut - the output NMF
 * 
 *   pln - pointer to line number, or NULL
 * 
 *   per - pointer to error, or NULL
 * 
 * Return:
 * 
 *   non-zero if successful, zero if error
 */
static int noir(FILE *pIn, FILE *pOut, int32_t *pln, int *per) {
  
  int status = 1;
  int dummy = 0;
  int32_t dummy32 = 0;
  
  /* Check parameters */
  if ((pIn == NULL) || (pOut == NULL) || (pIn == pOut)) {
    abort();
  }
  
  /* Redirect optional parameters if NULL */
  if (pln == NULL) {
    pln = &dummy32;
  }
  if (per == NULL) {
    per = &dummy;
  }
  
  /* Reset line and error */
  *pln = -1;
  *per = ERR_OK;
  
  /* Initialize the token module */
  token_init(pIn);
  
  /* Run the input file and interpret it */
  if (!entity_run(pln, per)) {
    status = 0;
  }
  
  /* Write event buffer and section table to output */
  if (status) {
    if (!event_finish(pOut)) {
      *pln = -1;
      *per = ERR_EMPTY;
      status = 0;
    }
  }
  
  /* Return status */
  return status;
}

/*
 * Given an error code, return an error message string for it.
 * 
 * The error message will begin with a capital letter but not have any
 * punctuation nor a line break after it.
 * 
 * If ERR_OK is passed, "No error" is returned.  If an unrecognized
 * error is passed, "Unknown error" is returned.
 * 
 * Parameters:
 * 
 *   code - the error code to look up
 * 
 * Return:
 * 
 *   an error message string
 */
static const char *err_string(int code) {
  
  const char *ps = NULL;
  
  switch (code) {
    
    case ERR_OK:
      ps = "No error";
      break;
    
    case ERR_EMPTY:
      ps = "No notes were defined";
      break;
    
    case ERR_IOREAD:
      ps = "I/O error reading input";
      break;
    
    case ERR_NULCHAR:
      ps = "Input file includes nul byte";
      break;
    
    case ERR_BADCHAR:
      ps = "Invalid character in input";
      break;
    
    case ERR_OVERLINE:
      ps = "Too many lines in input text";
      break;
    
    case ERR_KEYTOKEN:
      ps = "Bad key operation token";
      break;
    
    case ERR_LONGTOKEN:
      ps = "Token is too long";
      break;
    
    case ERR_PARAMTK:
      ps = "Bad parameter operation token";
      break;
    
    case ERR_RIGHT:
      ps = "Right closing ) or ] without opening symbol";
      break;
    
    case ERR_UNCLOSED:
      ps = "Unclosed ( or [ group";
      break;
    
    case ERR_TOODEEP:
      ps = "Too much nesting";
      break;
    
    case ERR_INGRACE:
      ps = "Grace note in rhythm group";
      break;
    
    case ERR_LONGDUR:
      ps = "Rhythm duration too long";
      break;
    
    case ERR_BADDUR:
      ps = "Invalid duration";
      break;
    
    case ERR_BADPITCH:
      ps = "Invalid pitch";
      break;
    
    case ERR_PITCHR:
      ps = "Pitch out of range";
      break;
    
    case ERR_BADOP:
      ps = "Invalid operation";
      break;
    
    case ERR_UNDERFLOW:
      ps = "Attempted to pop an empty stack";
      break;
    
    case ERR_BADLAYER:
      ps = "Invalid layer index";
      break;
    
    case ERR_STACKFULL:
      ps = "Too many elements on stack";
      break;
    
    case ERR_HUGETRANS:
      ps = "Cumulative transposition value too large";
      break;
    
    case ERR_DANGLEART:
      ps = "Dangling immediate articulation";
      break;
    
    case ERR_NOLOC:
      ps = "Attempt to warp when location stack is empty";
      break;
    
    case ERR_LINGER:
      ps = "Lingering values in interpreter stacks";
      break;
    
    case ERR_MANYSECT:
      ps = "Too many sections";
      break;
    
    case ERR_MULTCOUNT:
      ps = "Invalid count for multiple operation";
      break;
    
    case ERR_TRANSRNG:
      ps = "Transposed pitches out of range";
      break;
    
    case ERR_NOPITCH:
      ps = "Current pitch register undefined";
      break;
    
    case ERR_NODUR:
      ps = "Current duration register undefined";
      break;
    
    case ERR_HUGEGRACE:
      ps = "Grace note sequence too long";
      break;
    
    case ERR_LONGPIECE:
      ps = "Composition is too long";
      break;
    
    case ERR_MANYNOTES:
      ps = "Too many notes and/or cues";
      break;
    
    case ERR_CUENUM:
      ps = "Cue number out of range";
      break;
    
    default:
      ps = "Unknown error";
  }
  
  return ps;
}

/*
 * Program entrypoint
 * ==================
 */

int main(int argc, char *argv[]) {
  
  int status = 1;
  const char *pModule = NULL;
  int32_t line = 0;
  int errcode = 0;
  
  /* Get module name */
  if (argc > 0) {
    if (argv != NULL) {
      if (argv[0] != NULL) {
        pModule = argv[0];
      }
    }
  }
  if (pModule == NULL) {
    pModule = "noir";
  }
  
  /* Check that there are no parameters */
  if (argc > 1) {
    fprintf(stderr, "%s: Not expecting parameters!\n", pModule);
    status = 0;
  }
  
  /* Call through to main function */
  if (status) {
    if (!noir(stdin, stdout, &line, &errcode)) {
      if (line >= 0) {
        fprintf(stderr, "%s: [Line %ld] %s!\n",
                  pModule,
                  (long) line,
                  err_string(errcode));
        status = 0;
      } else {
        fprintf(stderr, "%s: %s!\n", pModule, err_string(errcode));
        status = 0;
      }
    }
  }
  
  /* Invert status and return */
  if (status) {
    status = 0;
  } else {
    status = 1;
  }
  return status;
}
