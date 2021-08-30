#ifndef NOIRDEF_H_INCLUDED
#define NOIRDEF_H_INCLUDED

/*
 * noirdef.h
 * 
 * Standard definitions and includes of the Noir compiler.
 */

#include <stddef.h>
#include <stdint.h>

/*
 * Range of pitches.
 * 
 * These values are semitone offset from middle C.  The given range here
 * covers the full 88-key piano keyboard.
 */
#define NOIR_MINPITCH (-39)
#define NOIR_MAXPITCH (48)

/*
 * The maximum numeric articulation value.
 */
#define NOIR_MAXART (61)

/*
 * The maximum one-indexed layer number.
 */
#define NOIR_MAXLAYER (INT32_C(65536))

/*
 * Error codes.
 * 
 * Remember to update err_string() in main module!
 */
#define ERR_OK        (0)   /* No error */
#define ERR_EMPTY     (1)   /* No notes defined */

#endif
