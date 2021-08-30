#ifndef ENTITY_H_INCLUDED
#define ENTITY_H_INCLUDED

/*
 * entity.h
 * 
 * Entity handling module of the Noir compiler.
 * 
 * This module bridges the tokens read from the token module to a series
 * of calls into the nvm module.
 * 
 * Requires the token and nvm modules, as well as the event module
 * because the nvm module requires it.
 */

#include "noirdef.h"

/*
 * Fully interpret the input file.
 * 
 * The token module must already be initialized.  This function will
 * read all tokens from the token module, interpret them, and make all
 * appropriate calls to the nvm module so that the Noir notation is run
 * through the virtual machine.  The nvm module will notify the event
 * module of all relevant events.
 * 
 * The event_finish() function is NOT called during this process.  The
 * honors are left to the client to call that function.
 * 
 * If an error occurs, pln and per will be used to return the line
 * number and the error number.
 * 
 * This function may only be called once.
 * 
 * Parameters:
 * 
 *   pln - pointer to variable to receive the line number in case of
 *   error
 * 
 *   per - pointer to variable to receive the error number in case of
 *   error
 * 
 * Return:
 * 
 *   non-zero if successful, zero if error
 */
int entity_run(int32_t *pln, int *per);

#endif
