#ifndef NOIRDEF_H_INCLUDED
#define NOIRDEF_H_INCLUDED

/*
 * noirdef.h
 * 
 * Standard definitions and includes of the Noir compiler.
 * 
 * Includes the nmf.h header from the Noir Music File (NMF) library.
 */

#include <stddef.h>
#include <stdint.h>
#include "nmf.h"

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
#define ERR_IOREAD    (2)   /* I/O error on read */
#define ERR_NULCHAR   (3)   /* Nul character encountered in input */
#define ERR_BADCHAR   (4)   /* Invalid character in input */
#define ERR_OVERLINE  (5)   /* Too many lines in input text */
#define ERR_KEYTOKEN  (6)   /* Bad key operation token */
#define ERR_LONGTOKEN (7)   /* Token is too long */
#define ERR_PARAMTK   (8)   /* Bad parameter operation token */
#define ERR_RIGHT     (9)   /* Right closing symbol without opener */
#define ERR_UNCLOSED  (10)  /* Unclosed group */
#define ERR_TOODEEP   (11)  /* Too much nesting */

/*
 * ASCII characters.
 */
#define ASCII_HT      (0x9)   /* Horizontal Tab */
#define ASCII_LF      (0xa)   /* Line Feed */
#define ASCII_CR      (0xd)   /* Carriage Return */
#define ASCII_SP      (0x20)  /* Space */
#define ASCII_EXCLAIM (0x21)  /* ! */
#define ASCII_NUMSIGN (0x23)  /* # */
#define ASCII_DOLLAR  (0x24)  /* $ */
#define ASCII_AMP     (0x26)  /* & */
#define ASCII_APOS    (0x27)  /* ' */
#define ASCII_LPAREN  (0x28)  /* ( */
#define ASCII_RPAREN  (0x29)  /* ) */
#define ASCII_STAR    (0x2a)  /* * */
#define ASCII_PLUS    (0x2b)  /* + */
#define ASCII_COMMA   (0x2c)  /* , */
#define ASCII_HYPHEN  (0x2d)  /* - */
#define ASCII_PERIOD  (0x2e)  /* . */
#define ASCII_SLASH   (0x2f)  /* / */
#define ASCII_ZERO    (0x30)  /* 0 */
#define ASCII_NINE    (0x39)  /* 9 */
#define ASCII_COLON   (0x3a)  /* : */
#define ASCII_SEMICOL (0x3b)  /* ; */
#define ASCII_EQUALS  (0x3d)  /* = */
#define ASCII_ATSIGN  (0x40)  /* @ */
#define ASCII_A_UPPER (0x41)  /* A */
#define ASCII_G_UPPER (0x47)  /* G */
#define ASCII_H_UPPER (0x48)  /* H */
#define ASCII_N_UPPER (0x4e)  /* N */
#define ASCII_R_UPPER (0x52)  /* R */
#define ASCII_S_UPPER (0x53)  /* S */
#define ASCII_T_UPPER (0x54)  /* T */
#define ASCII_X_UPPER (0x58)  /* X */
#define ASCII_LSQUARE (0x5b)  /* [ */
#define ASCII_BSLASH  (0x5c)  /* \ */
#define ASCII_RSQUARE (0x5d)  /* ] */
#define ASCII_CARET   (0x5e)  /* ^ */
#define ASCII_A_LOWER (0x61)  /* a */
#define ASCII_G_LOWER (0x67)  /* g */
#define ASCII_H_LOWER (0x68)  /* h */
#define ASCII_N_LOWER (0x6e)  /* n */
#define ASCII_R_LOWER (0x72)  /* r */
#define ASCII_S_LOWER (0x73)  /* s */
#define ASCII_T_LOWER (0x74)  /* t */
#define ASCII_X_LOWER (0x78)  /* x */
#define ASCII_LCURLY  (0x7b)  /* { */
#define ASCII_RCURLY  (0x7d)  /* } */
#define ASCII_TILDE   (0x7e)  /* ~ */

#define ASCII_MINPRNT (0x21)  /* Minimum printing character */
#define ASCII_MAXPRNT (0x7e)  /* Maximum printing character */

#endif
