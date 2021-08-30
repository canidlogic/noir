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
 * See the "Noir Music File (NMF) specification" for the format of the
 * output binary file.
 * 
 * Compilation
 * -----------
 * 
 * @@TODO: event
 */

#include "noirdef.h"
#include "event.h"

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
  
  /* @@TODO: */
  if (status) {
    if (!event_note(0, 96, 9, 0, 0, 1)) {
      abort();
    }
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
