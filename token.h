#ifndef TOKEN_H_INCLUDED
#define TOKEN_H_INCLUDED

/*
 * token.h
 * 
 * Tokenizer module of the Noir compiler.
 */

#include "noirdef.h"
#include <stdio.h>

/*
 * The maximum number of characters allowed in a token, including the
 * terminating nul character.
 */
#define TOKEN_MAXCHAR (32)

/*
 * Structure representing a token read from the file.
 */
typedef struct {
  
  /*
   * ERR_OK if token was read successfully, otherwise an error code from
   * noirdef.h indicating the error that prevented a token from being
   * read.
   */
  int status;
  
  /*
   * The line number in the input file that this token was read from.
   * 
   * If status indicates an error, this is the line number of the error.
   */
  int32_t line;
  
  /*
   * If status is ERR_OK, this contains the nul-terminated token that
   * was read from the file.
   * 
   * The End Of File (EOF) is recorded as an empty token (first
   * character is nul).
   * 
   * The contents of this buffer are undefined if an error occurs.
   */
  char str[TOKEN_MAXCHAR];
  
} TOKEN;

/*
 * Initialize the token reading module.
 * 
 * pIn is the file to read from.  It must be open for reading or
 * undefined behavior occurs.  Reading is fully sequential.  The file
 * must not be used except by this module until all token_read()
 * requests have been made, or undefined behavior occurs.
 * 
 * The token module may only be initialized once.  A fault occurs if it
 * is initialized more than once.
 * 
 * Parameters:
 * 
 *   pIn - the file to read from
 */
void token_init(FILE *pIn);

/*
 * Read the next token.
 * 
 * The module must have been initialized with token_init() before using
 * this function.
 * 
 * ptk is the pointer to the structure to fill in with the new token.
 * See the structure documentation for further information.
 * 
 * If there was an error, the status field in the token structure will
 * indicate the error number.
 * 
 * Parameters:
 * 
 *   ptk - the token structure to fill in
 * 
 * Return:
 * 
 *   non-zero if successful, zero if error
 */
int token_read(TOKEN *ptk);

#endif
