/*
 * nmfwalk.c
 * =========
 * 
 * Walk through a Noir Music File (NMF), verify it, and optionally print
 * a textual description of its data.
 * 
 * Syntax
 * ------
 * 
 *   nmfwalk
 *   nmfwalk -check
 * 
 * Both invocations read an NMF file from standard input and verify it.
 * 
 * The "-check" invocation does nothing beyond verifying the NMF file.
 * 
 * The parameter-less invocation also prints out a textual description
 * of the data within the NMF file to standard output.
 * 
 * File format
 * -----------
 * 
 * See the "Noir Music File (NMF) specification" for the format of the
 * input binary file.
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
 * Print a textual representation of the given parsed data object to the
 * given file.
 * 
 * Parameters:
 * 
 *   pd - the parsed data object
 * 
 *   po - the output file
 */
static void report(NMF_DATA *pd, FILE *po) {
  
  int32_t x = 0;
  int basis = 0;
  int32_t scount = 0;
  int32_t ncount = 0;
  NMF_NOTE n;
  
  /* Initialize structure */
  memset(&n, 0, sizeof(NMF_NOTE));
  
  /* Check parameter */
  if ((pd == NULL) || (po == NULL)) {
    abort();
  }
  
  /* Get the basis */
  basis = nmf_basis(pd);
  
  /* Get section count and note count */
  scount = nmf_sections(pd);
  ncount = nmf_notes(pd);
  
  /* Print the basis */
  fprintf(po, "BASIS   : ");
  if (basis == NMF_BASIS_Q96) {
    fprintf(po, "96 quanta per quarter\n");
    
  } else if (basis == NMF_BASIS_44100) {
    fprintf(po, "44,100 quanta per second\n");
    
  } else if (basis == NMF_BASIS_48000) {
    fprintf(po, "48,000 quanta per second\n");
    
  } else {
    /* Unrecognized basis */
    abort();
  }
  
  /* Print the section and note counts */
  fprintf(po, "SECTIONS: %ld\n", (long) scount);
  fprintf(po, "NOTES   : %ld\n", (long) ncount);
  fprintf(po, "\n");
  
  /* Print each section location */
  for(x = 0; x < scount; x++) {
    fprintf(po, "SECTION %ld AT %ld\n",
              (long) x,
              (long) nmf_offset(pd, x));
  }
  fprintf(po, "\n");
  
  /* Print each note */
  for(x = 0; x < ncount; x++) {
    
    /* Get the note */
    nmf_get(pd, x, &n);
    
    /* Print the information */
    fprintf(po, "NOTE T=%ld DUR=%ld P=%d A=%ld S=%ld L=%ld\n",
            (long) n.t,
            (long) n.dur,
            (int) n.pitch,
            (long) n.art,
            (long) n.sect,
            ((long) n.layer_i) + 1);
  }
}

/*
 * Program entrypoint
 * ==================
 */

int main(int argc, char *argv[]) {
  
  int status = 1;
  const char *pModule = NULL;
  int silent = 0;
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
    pModule = "nmfwalk";
  }
  
  /* There may be at most one argument beyond the module name */
  if (argc > 2) {
    fprintf(stderr, "%s: Too many arguments!\n", pModule);
    status = 0;
  }
  
  /* If there is one argument, it must be "-check", and set the silent
   * flag in that case */
  if (status && (argc == 2)) {
    if (argv == NULL) {
      abort();
    }
    if (argv[0] == NULL) {
      abort();
    }
    if (strcmp(argv[1], "-check") != 0) {
      fprintf(stderr, "%s: Unrecognized argument!\n", pModule);
      status = 0;
    }
    if (status) {
      silent = 1;
    }
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
  
  /* If not silent, then report the contents */
  if (status && (!silent)) {
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
