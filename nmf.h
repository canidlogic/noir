#ifndef NMF_H_INCLUDED
#define NMF_H_INCLUDED

/*
 * nmf.h
 * 
 * Noir Music File (NMF) parser library.
 */

#include <stddef.h>
#include <stdio.h>
#include <stdint.h>

/*
 * The maximum section count possible for an NMF file.
 */
#define NMF_MAXSECT (INT32_C(65535))

/*
 * The maximum note count possible for an NMF file.
 */
#define NMF_MAXNOTE (INT32_C(1048576))

/*
 * The minimum and maximum pitch values.
 */
#define NMF_MINPITCH (-39)
#define NMF_MAXPITCH (48)

/*
 * The maximum articulation value.
 */
#define NMF_MAXART (61)

/*
 * The quantum basis values.
 */
#define NMF_BASIS_Q96   (0)   /* 96 quanta in a quarter note */
#define NMF_BASIS_44100 (1)   /* 44,100 quanta per second */
#define NMF_BASIS_48000 (2)   /* 48,000 quanta per second */

/*
 * NMF_DATA structure prototype.
 * 
 * Definition given in the implementation.
 */
struct NMF_DATA_TAG;
typedef struct NMF_DATA_TAG NMF_DATA;

/*
 * Representation of a parsed note.
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
   * A value of zero is reserved for cues and other special data.
   */
  int32_t dur;
  
  /*
   * The pitch of this note.
   * 
   * This is the number of semitones (half steps) away from middle C.  A
   * value of zero is middle C, -1 is B below middle C, 2 is D above
   * middle C, and so forth.
   * 
   * The range is [NMF_MINPITCH, NMF_MAXPITCH].
   */
  int16_t pitch;
  
  /*
   * The articulation index of this note.
   * 
   * The range is [0, NMF_MAXART].
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
  
} NMF_NOTE;

/*
 * Parse the given file as an NMF file and return an object representing
 * the parsed data.
 * 
 * pf is the file to read.  The file is read from the current position
 * in sequential order.  Any additional data after the NMF file is
 * ignored.  Undefined behavior occurs if pf is not open for reading.
 * 
 * The returned object must eventually be freed with nmf_free().
 * 
 * Parameters:
 * 
 *   pf - the NMF file to parse
 * 
 * Return:
 * 
 *   a parsed representation of the NMF file data, or NULL if there was
 *   an error
 */
NMF_DATA *nmf_parse(FILE *pf);

/*
 * Wrapper around nmf_parse that accepts a file path instead of an open
 * file.
 * 
 * Parameters:
 * 
 *   pPath - the path to the NMF file to parse
 * 
 * Return:
 * 
 *   a parsed representation of the NMF file, or NULL if there was an
 *   error
 */
NMF_DATA *nmf_parse_path(const char *pPath);

/*
 * Release the given parsed data object.
 * 
 * If NULL is passed, the call is ignored.
 * 
 * Parameters:
 * 
 *   pd - the parsed data object to free, or NULL
 */
void nmf_free(NMF_DATA *pd);

/*
 * Return the quantum basis of the parsed data object.
 * 
 * The return value is one of the NMF_BASIS constants.
 * 
 * Parameters:
 * 
 *   pd - the parsed data object
 * 
 * Return:
 * 
 *   the quantum basis
 */
int nmf_basis(NMF_DATA *pd);

/*
 * Return the number of sections in the parsed data object.
 * 
 * The range is [1, NMF_MAXSECT].
 * 
 * Parameters:
 * 
 *   pd - the parsed data object
 * 
 * Return:
 * 
 *   the number of sections
 */
int32_t nmf_sections(NMF_DATA *pd);

/*
 * Return the number of notes in the parsed data object.
 * 
 * The range is [1, NMF_MAXNOTE].
 * 
 * Parameters:
 * 
 *   pd - the parsed data object
 * 
 * Return:
 * 
 *   the number of notes
 */
int32_t nmf_notes(NMF_DATA *pd);

/*
 * Return the starting quanta offset of the given section index.
 * 
 * sect_i is the section index within the parsed data object.  It must
 * be at least zero and less than nmf_sections().
 * 
 * Each section after the first section must have a starting offset that
 * is greater than or equal to the previous section.  The first section
 * must have a starting offset of zero.
 * 
 * Parameters:
 * 
 *   pd - the parsed data object
 * 
 *   sect_i - the section index
 * 
 * Return:
 * 
 *   the starting quanta offset of this section
 */
int32_t nmf_offset(NMF_DATA *pd, int32_t sect_i);

/*
 * Return a specific note within the parsed data object.
 * 
 * note_i is the note index within the parsed data object.  It must be
 * at least zero and less than nmf_notes().
 * 
 * pn points to the structure to fill with information about the note.
 * 
 * The notes are in any order; there are no sorting guarantees.
 * 
 * Parameters:
 * 
 *   pd - the parsed data object
 * 
 *   note_i - the note index
 * 
 *   pn - the note structure to fill
 */
void nmf_get(NMF_DATA *pd, int32_t note_i, NMF_NOTE *pn);

#endif
