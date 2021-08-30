#ifndef EVENT_H_INCLUDED
#define EVENT_H_INCLUDED

/*
 * event.h
 * 
 * Event Buffer module of the Noir compiler.
 */

#include "noirdef.h"
#include <stdio.h>

/*
 * The maximum number of sections that may be defined.
 * 
 * This includes section index zero as the first section.
 */
#define EVENT_MAXSECT (INT32_C(65535))

/*
 * The maximum number of notes that may be defined.
 */
#define EVENT_MAXNOTE (INT32_C(1048576))

/*
 * Define a new section beginning at the given offset in quanta.
 * 
 * offset is the offset in quanta from the beginning of the piece.  An
 * offset of zero is the beginning of the piece.  offset must be zero or
 * greater.
 * 
 * The first section (section index zero) is already defined with an
 * offset zero.  This call is only made for sections after section zero.
 * 
 * Subsequent sections must have an offset that is greater than or equal
 * to the offset of the previous section or a fault occurs.
 * 
 * If the number of sections exceeds EVENT_MAXSECT, then the function
 * will fail.
 * 
 * A fault occurs if this is called after event_finish().
 * 
 * Parameters:
 * 
 *   offset - the offset of the new section
 * 
 * Return:
 * 
 *   non-zero if successful, zero if too many sections
 */
int event_section(int32_t offset);

/*
 * Define a new note event.
 * 
 * Note events may be defined in any order.  They will be sorted
 * appropriately when they are output.  However, notes that have a
 * section index that is greater than zero may only be defined after
 * that section has been defined with event_section().
 * 
 * t is the offset of the note in quanta from the beginning of the
 * piece.  It must be greater than or equal to the starting offset of
 * the section the note belongs to or a fault occurs.
 * 
 * dur is the duration of the note in quanta.  If dur is less than zero,
 * then the note is an unmeasured grace note and the absolute value of
 * dur is the grace note offset, where -1 is the note before the beat,
 * -2 is the grace note before -1, and so forth.  A fault occurs if a
 * value of zero is given.  dur must not be below -(INT32_MAX).
 * 
 * pitch is the pitch of the note, in semitones from middle C.  The
 * range is [NOIR_MINPITCH, NOIR_MAXPITCH] (as given in noirdef.h).
 * 
 * art is the numeric articulation index.  The range is
 * [0, NOIR_MAXART], where NOIR_MAXART is given in noirdef.h
 * 
 * sect is the section index of the section that the note belongs to.
 * It must be zero or greater.  If it is greater than zero, it must
 * reference a section that has been defined by event_section(), and the
 * time offset of the note must be greater than or equal to the starting
 * offset of the section.
 * 
 * layer is the layer index within the section.  The actual numeric
 * value that is stored in the event is one less than the value that is
 * given here.  The layer parameter is in [1, NOIR_MAXLAYER], where the
 * constant NOIR_MAXLAYER is given in noirdef.h
 * 
 * A fault occurs if any of the parameters are invalid.  The function
 * fails if too many notes have been added.
 * 
 * A fault occurs if this is called after event_finish().
 * 
 * Parameters:
 * 
 *   t - the time offset of the note
 * 
 *   dur - the duration of the note
 * 
 *   pitch - the pitch of the note
 * 
 *   art - the articulation of the note
 * 
 *   sect - the section the note belongs to
 * 
 *   layer - the layer within that section the note belongs to
 * 
 * Return:
 * 
 *   non-zero if successful, zero if too many notes
 */
int event_note(
    int32_t t,
    int32_t dur,
    int32_t pitch,
    int32_t art,
    int32_t sect,
    int32_t layer);

/*
 * Flip grace note offsets at the end of the event buffer.
 * 
 * count is the number of grace note events that need their grace note
 * offsets flipped.  count must be at least zero and less than or equal
 * to the total number of notes that have been added with event_note().
 * 
 * max_offs is the maximum grace note offset that is used in the
 * sequence.  It must be at least one.
 * 
 * The last (count) events in the event buffer must all be grace notes,
 * and their grace note offsets must each be less than or equal to the
 * max_offs value that is passed.
 * 
 * If count is zero or max_offs is one, then the function does nothing.
 * 
 * Otherwise, the grace note offsets of each of the selected grace note
 * events are flipped so that the grace note sequence is in the proper
 * order.
 * 
 * A fault occurs if this is called after event_finish().
 * 
 * Parameters:
 * 
 *   count - the number of grace note events to flip
 * 
 *   max_offs - the maximum grace note offset in the sequence
 */
void event_flip(int32_t count, int32_t max_offs);

/*
 * Output the section table and all the notes in Noir Music File (NMF)
 * format to the given file.
 * 
 * pf is the file to write the output to.  It must be open for writing
 * or undefined behavior occurs.  Writing is fully sequential.
 * 
 * At least one note must have been defined with event_note() or the
 * function will fail.
 * 
 * This function may only be used once.  Once the function has been
 * called, no further calls can be made to the event buffer module.
 * 
 * Parameters:
 * 
 *   pf - the file to write the NMF output to
 * 
 * Return:
 * 
 *   non-zero if successful, zero if no notes have been defined
 */
int event_finish(FILE *pf);

#endif
