/*
 * nmfsimple.c
 * ===========
 * 
 * Open a fixed-rate NMF file, sort its notes, and output a series of
 * note events in the Retro synthesizer format, always using instrument
 * one and layer one for everything.
 * 
 * Grace notes and notes of duration zero are ignored.
 * 
 * Syntax
 * ------
 * 
 *   nmfsimple
 * 
 * The input fixed-rate NMF file is read from standard input.  The note
 * events are written to standard output.
 * 
 * Compation
 * ---------
 * 
 * Compile with the nmf library.
 */

#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "nmf.h"

/*
 * Local functions
 * ===============
 */

/* Prototypes */
static void report(NMF_DATA *pd, FILE *po);

/*
 * Print a textual representation of each note in the given parsed data
 * object to the given file.
 * 
 * The textual representation follows the Retro synthesizer note command
 * format, with instrument one and layer one always used.
 * 
 * Grace notes and notes of duration zero are ignored.
 * 
 * Parameters:
 * 
 *   pd - the parsed data object
 * 
 *   po - the output file
 */
static void report(NMF_DATA *pd, FILE *po) {
  
  int32_t x = 0;
  int32_t ncount = 0;
  NMF_NOTE n;
  
  /* Initialize structure */
  memset(&n, 0, sizeof(NMF_NOTE));
  
  /* Check parameter */
  if ((pd == NULL) || (po == NULL)) {
    abort();
  }
  
  /* Get note count */
  ncount = nmf_notes(pd);
  
  /* Print each note */
  for(x = 0; x < ncount; x++) {
    
    /* Get the note */
    nmf_get(pd, x, &n);
    
    /* Ignore notes of duration less than one */
    if (n.dur < 1) {
      continue;
    }
    
    /* Print the information */
    fprintf(po, "%ld %ld %d 1 1 n\n",
            (long) n.t,
            (long) n.dur,
            (int) n.pitch);
  }
}

/*
 * Program entrypoint
 * ==================
 */

int main(int argc, char *argv[]) {
  
  int status = 1;
  int basis = 0;
  const char *pModule = NULL;
  NMF_DATA *pd = NULL;
  
  /* Get module name */
  if (argc > 0) {
    if (argv != NULL) {
      if (argv[0] != NULL) {
        pModule = argv[0];
      }
    }
  }
  if (pModule == NULL) {
    pModule = "nmfsimple";
  }
  
  /* There may be no arguments beyond the module name */
  if (argc > 1) {
    fprintf(stderr, "%s: Not expecting arguments!\n", pModule);
    status = 0;
  }
  
  /* Parse standard input as an NMF file */
  if (status) {
    pd = nmf_parse(stdin);
    if (pd == NULL) {
      fprintf(stderr, "%s: A valid NMF file could not be read!\n",
                pModule);
      status = 0;
    }
  }
  
  /* NMF file must have fixed basis */
  if (status) {
    basis = nmf_basis(pd);
    if ((basis != NMF_BASIS_44100) && (basis != NMF_BASIS_48000)) {
      fprintf(stderr, "%s: Input must have fixed-rate basis!\n",
                pModule);
      status = 0;
    }
  }
  
  /* Sort the note events */
  if (status) {
    nmf_sort(pd);
  }
  
  /* Report the contents as note events */
  if (status) {
    report(pd, stdout);
  }
  
  /* Free parsed data object if allocated */
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
