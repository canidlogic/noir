#ifndef NVM_H_INCLUDED
#define NVM_H_INCLUDED

/*
 * nmv.h
 * 
 * The Noir Virtual Machine component of the Noir compiler.
 * 
 * This module interprets the instructions in Noir notation and outputs
 * events to the event module as is appropriate.
 * 
 * Requires the event module.
 */

#include "noirdef.h"

/*
 * Definition of pitch set structure.
 * 
 * Use the nvm_pitchset functions to manipulate the structure.  Do not
 * access the internal fields directly.
 */
typedef struct {
  
  /*
   * Bitmap storing pitches with negative value, where -1 is the least
   * significant bit, -2 is the second least significant bit, and so
   * forth.
   */
  uint64_t a;
  
  /*
   * Bitmap storing pitches with zero or positive value, where zero is
   * the most significant bit, 1 is the second most significant bit,
   * and so forth.
   */
  uint64_t b;
  
} NVM_PITCHSET;

/*
 * Clear a pitch set so it contains no pitches.
 * 
 * This can also be used to initialize a pitch set structure.
 * 
 * Parameters:
 * 
 *   ps - the pitch set to clear
 */
void nvm_pitchset_clear(NVM_PITCHSET *ps);

/*
 * Check whether the given pitch set is empty.
 * 
 * Parameters:
 * 
 *   ps - the pitch set to check
 * 
 * Return:
 * 
 *   non-zero if empty, zero if at least one pitch
 */
int nvm_pitchset_isEmpty(const NVM_PITCHSET *ps);

/*
 * Add a pitch to a pitch set, if not already present.
 * 
 * pitch is the pitch to add, in semitones from middle C.  Its range is
 * [NMF_MINPITCH, NMF_MAXPITCH].
 * 
 * Parameters:
 * 
 *   ps - the pitch set to modify
 * 
 *   pitch - the pitch to add
 */
void nvm_pitchset_add(NVM_PITCHSET *ps, int32_t pitch);

/*
 * Drop a specific pitch from a pitch set, if it is present.
 * 
 * pitch is the pitch to drop, in semitones from middle C.  Its range is
 * [NMF_MINPITCH, NMF_MAXPITCH].
 * 
 * Parameters:
 * 
 *   ps - the pitch set to modify
 * 
 *   pitch - the pitch to add
 */
void nvm_pitchset_drop(NVM_PITCHSET *ps, int32_t pitch);

/*
 * Return the lowest pitch in the given pitch set.
 * 
 * The returned pitch will be in range [NMF_MINPITCH, NMF_MAXPITCH].  A
 * fault occurs if the pitch set is empty.
 * 
 * Parameters:
 * 
 *   ps - the pitch set to search
 * 
 * Return:
 * 
 *   the lowest pitch in the pitch set
 */
int32_t nvm_pitchset_least(const NVM_PITCHSET *ps);

/*
 * Return the highest pitch in the given pitch set.
 * 
 * The returned pitch will be in range [NMF_MINPITCH, NMF_MAXPITCH].  A
 * fault occurs if the pitch set is empty.
 * 
 * Parameters:
 * 
 *   ps - the pitch set to search
 * 
 * Return:
 * 
 *   the highest pitch in the pitch set
 */
int32_t nvm_pitchset_most(const NVM_PITCHSET *ps);

/*
 * Transpose all pitches in a given pitch set by the given number of
 * semitones.
 * 
 * offset is the number of semitones to transpose.  A value of zero does
 * nothing.  A positive value transposes all pitches upwards by that
 * number of semitones.  A negative value transposes all pitches down by
 * that number of semitones.
 * 
 * If the pitch set is empty, this function has no effect.
 * 
 * If transposition by the given offset would cause any pitch to go
 * outside of the range [NMF_MINPITCH, NMF_MAXPITCH], the function fails
 * and the pitch set is not modified.
 * 
 * Parameters:
 * 
 *   ps - the pitch set to transpose
 * 
 *   offset - the number of semitones to transpose
 * 
 * Return:
 * 
 *   non-zero if successful, zero if transposition causes any pitch to
 *   go out of range
 */
int nvm_pitchset_transpose(NVM_PITCHSET *ps, int32_t offset);

/*
 * Report an encountered pitch set in the input file.
 * 
 * ps is the pitch set that was encountered.  Pass a pitch set with one
 * element to report a single pitch.  Pass an empty pitch set to report
 * a rest.
 * 
 * per points to a variable to receive an error code if the function
 * fails.  The error codes are defined in noirdef.h
 * 
 * Parameters:
 * 
 *   ps - the pitch set to report
 * 
 *   per - pointer to an error variable
 * 
 * Return:
 * 
 *   non-zero if successful, zero if error
 */
int nvm_pset(const NVM_PITCHSET *ps, int *per);

/*
 * Report an encountered duration in the input file.
 * 
 * q is the number of quanta in the duration.  It must be zero or
 * greater.  If it is zero, it means an unmeasured grace note.
 * 
 * Rhythm groups should be reported as a single duration covering the
 * total duration within the group.
 * 
 * per points to a variable to receive an error code if the function
 * fails.  The error codes are defined in noirdef.h
 * 
 * Parameters:
 * 
 *   q - the number of quanta, or zero for unmeasured grace note
 * 
 *   per - pointer to an error variable
 * 
 * Return:
 * 
 *   non-zero if successful, zero if error
 */
int nvm_dur(int32_t q, int *per);

/*
 * Report that the end of the input file has occurred.
 * 
 * This might cause an error if the virtual machine is not in an
 * appropriate final state.
 * 
 * per points to a variable to receive an error code if the function
 * fails.  The error codes are defined in noirdef.h
 * 
 * Parameters:
 * 
 *   per - pointer to an error variable
 * 
 * Return:
 * 
 *   non-zero if successful, zero if error
 */
int nvm_eof(int *per);

/*
 * The "/" repeater operation.
 * 
 * per points to a variable to receive an error code if the function
 * fails.  The error codes are defined in noirdef.h
 * 
 * Parameters:
 * 
 *   per - pointer to an error variable
 * 
 * Return:
 * 
 *   non-zero if successful, zero if error
 */
int nvm_op_repeat(int *per);

/*
 * The "\" multiple repeater operation.
 * 
 * t is the repeat count parameter.  This function will range-check and
 * report an error if necessary.
 * 
 * per points to a variable to receive an error code if the function
 * fails.  The error codes are defined in noirdef.h
 * 
 * Parameters:
 * 
 *   t - the repeat count
 * 
 *   per - pointer to an error variable
 * 
 * Return:
 * 
 *   non-zero if successful, zero if error
 */
int nvm_op_multiple(int32_t t, int *per);

/*
 * The "$" section operation.
 * 
 * per points to a variable to receive an error code if the function
 * fails.  The error codes are defined in noirdef.h
 * 
 * Parameters:
 * 
 *   per - pointer to an error variable
 * 
 * Return:
 * 
 *   non-zero if successful, zero if error
 */
int nvm_op_section(int *per);

/*
 * The "@" section return operation.
 * 
 * per points to a variable to receive an error code if the function
 * fails.  The error codes are defined in noirdef.h
 * 
 * Parameters:
 * 
 *   per - pointer to an error variable
 * 
 * Return:
 * 
 *   non-zero if successful, zero if error
 */
int nvm_op_return(int *per);

/*
 * The "{" push current location operation.
 * 
 * per points to a variable to receive an error code if the function
 * fails.  The error codes are defined in noirdef.h
 * 
 * Parameters:
 * 
 *   per - pointer to an error variable
 * 
 * Return:
 * 
 *   non-zero if successful, zero if error
 */
int nvm_op_pushloc(int *per);

/*
 * The ":" return to location operation.
 * 
 * per points to a variable to receive an error code if the function
 * fails.  The error codes are defined in noirdef.h
 * 
 * Parameters:
 * 
 *   per - pointer to an error variable
 * 
 * Return:
 * 
 *   non-zero if successful, zero if error
 */
int nvm_op_retloc(int *per);

/*
 * The "}" pop location stack operation.
 * 
 * per points to a variable to receive an error code if the function
 * fails.  The error codes are defined in noirdef.h
 * 
 * Parameters:
 * 
 *   per - pointer to an error variable
 * 
 * Return:
 * 
 *   non-zero if successful, zero if error
 */
int nvm_op_poploc(int *per);

/*
 * The "^" push transposition operation.
 * 
 * t is the number of semitones to transpose.  This function will
 * range-check the parameter and report an error if necessary.
 * 
 * per points to a variable to receive an error code if the function
 * fails.  The error codes are defined in noirdef.h
 * 
 * Parameters:
 * 
 *   t - the transposition count
 * 
 *   per - pointer to an error variable
 * 
 * Return:
 * 
 *   non-zero if successful, zero if error
 */
int nvm_op_pushtrans(int32_t t, int *per);

/*
 * The "=" pop transposition operation.
 * 
 * per points to a variable to receive an error code if the function
 * fails.  The error codes are defined in noirdef.h
 * 
 * Parameters:
 * 
 *   per - pointer to an error variable
 * 
 * Return:
 * 
 *   non-zero if successful, zero if error
 */
int nvm_op_poptrans(int *per);

/*
 * The "*" immediate articulation operation.
 * 
 * art is the numeric index of the articulation.  It must be in the
 * range [0, NMF_MAXART] or a fault occurs.
 * 
 * per points to a variable to receive an error code if the function
 * fails.  The error codes are defined in noirdef.h
 * 
 * Parameters:
 * 
 *   art - the immediate articulation to set
 * 
 *   per - pointer to an error variable
 * 
 * Return:
 * 
 *   non-zero if successful, zero if error
 */
int nvm_op_immart(int art, int *per);

/*
 * The "!" push articulation operation.
 * 
 * art is the numeric index of the articulation.  It must be in the
 * range [0, NMF_MAXART] or a fault occurs.
 * 
 * per points to a variable to receive an error code if the function
 * fails.  The error codes are defined in noirdef.h
 * 
 * Parameters:
 * 
 *   art - the immediate articulation to push
 * 
 *   per - pointer to an error variable
 * 
 * Return:
 * 
 *   non-zero if successful, zero if error
 */
int nvm_op_pushart(int art, int *per);

/*
 * The "~" pop articulation operation.
 * 
 * per points to a variable to receive an error code if the function
 * fails.  The error codes are defined in noirdef.h
 * 
 * Parameters:
 * 
 *   per - pointer to an error variable
 * 
 * Return:
 * 
 *   non-zero if successful, zero if error
 */
int nvm_op_popart(int *per);

/*
 * The "&" set base layer operation.
 * 
 * layer is the layer to set.  This function will range-check the
 * parameter and report an error if necessary.
 * 
 * per points to a variable to receive an error code if the function
 * fails.  The error codes are defined in noirdef.h
 * 
 * Parameters:
 * 
 *   layer - the layer to set
 * 
 *   per - pointer to an error variable
 * 
 * Return:
 * 
 *   non-zero if successful, zero if error
 */
int nvm_op_setbase(int32_t layer, int *per);

/*
 * The "+" push layer operation.
 * 
 * layer is the layer to push.  This function will range-check the
 * parameter and report an error if necessary.
 * 
 * per points to a variable to receive an error code if the function
 * fails.  The error codes are defined in noirdef.h
 * 
 * Parameters:
 * 
 *   layer - the layer to push
 * 
 *   per - pointer to an error variable
 * 
 * Return:
 * 
 *   non-zero if successful, zero if error
 */
int nvm_op_pushlayer(int32_t layer, int *per);

/*
 * The "-" pop layer operation.
 * 
 * per points to a variable to receive an error code if the function
 * fails.  The error codes are defined in noirdef.h
 * 
 * Parameters:
 * 
 *   per - pointer to an error variable
 * 
 * Return:
 * 
 *   non-zero if successful, zero if error
 */
int nvm_op_poplayer(int *per);

/*
 * The "`" operation for defining a cue.
 * 
 * cue_num is the number of the cue within the current section.  This
 * must be in range [0, NOIR_MAXCUE].
 * 
 * per points to a variable to receive an error code if the function
 * fails.  The error codes are defined in noirdef.h
 * 
 * Parameters:
 * 
 *   cue_num - the cue number
 * 
 *   per - pointer to an error variable
 * 
 * Return:
 * 
 *   non-zero if successful, zero if error
 */
int nvm_op_cue(int32_t cue_num, int *per);

#endif
