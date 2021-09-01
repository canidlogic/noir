# Upgrade path to beta 0.5.3

This document discusses the architectural changes to Noir moving from version 0.5.2 beta to 0.5.3 beta.  The changes to the Noir ASCII format and NMF parsing are backwards compatible.  However, various architectural changes to NMF are __not__ backwards compatible, and projects that currently use shell scripts for rendering Noir into Retro will either have to rewrite for 0.5.3 or remain on 0.5.2.

## Changes to NMF

The updated NMF format will have a different secondary signature, since it is not backwards compatible.

The quantum basis field is dropped.  All events will be measured in microseconds from the start of the composition.

Section table offsets become signed 64-bit so that they can use microsecond counts without worrying about upper time limits.

Within the note table, time offsets and durations both become signed 64-bit so that they can use microseconds.  Negative durations and zero durations are no longer allowed.  Grace notes will be compiled into their proper time offset and duration, and a new flag will be set in the articulation field to indicate that the note is a grace note.  The "cue" notes of duration zero (which were not used in practice) can be replaced with notes of positive duration in a special layer that is interpreted as cue notes, if so desired.

The articulation field of notes gets a bit field added that indicates whether or not the note is a grace note.  This allows for different renderings of grace notes.

The leftover bits in the articulation and pitch fields are used to add an additional "ramp" field which is an integer value that encodes a normalized floating-point value in range \[0.0, 1.0\].  The ramp field is intended to be combined with the articulation field to allow for articulations that have a single normalized parameters.  This allows for things such as crescendi, diminuendi, and gradual changes in articulation.

## Changes to output architecture

NMF reading and writing is split out into a separate library, and `nmfwalk` follows the migration to that library.  The `nmfgraph` and `nmfsimple` tools are spun off from the Noir project into a separate `nmfretro` project.  Connecting Noir to specific synthesizers will be handled with separate projects that read NMF (using the NMF library) and then transform it appropriately to the synthesis format.

The NMF library will have read support for the previous version of NMF as well as conversion utilities.  The conversion utilities can be used to connect beta 0.5.2 of Noir to the new architecture synthesis renderers.  That way, the synthesis renderers can be developed and tested without waiting for updates to Noir.

Note that the `nmfgraph` technique is no longer required, since the special "ramp" field of notes in NMF now supports dynamic ramps.

We will also be able to add MIDI output support with a separate project now, just by implementing an NMF reader that transforms to MIDI.

## Changes to Noir architecture

The `nmftempo` utility program will be integrated as a module within Noir.  This is because the new definition of NMF now always requires absolute time.  For the `nmfrate` utility program, we will handle this as a shortcut mode that takes a tempo and then constructs a tempo file for Noir in memory and passes this through to the `nmftempo` module (which is now possible with the new input source architecture of Shastina).

The tempo file format is updated to take optional header metacommands that specify the grace note time in microseconds, as well as optional leading silence time in microseconds.  Both of these parameters have default values.

Tempo transformation is now also responsible for converting grace notes into absolute time and setting the grace note flag appropriately.

## Event buffer format change

Durations of zero are no longer supported, since cue durations are no longer a thing.

Notes get two additional signed, 32-bit integer fields.  The first stores the time offset at the start of the ramp and the second stores the time offset at the end of the ramp, if the note is part of an articulation ramp.  If the note is not part of an articulation ramp, both fields are set to zero.

The actual ramp value that will be output to the NMF file for the note is not computed until the note is mapped into absolute time in microseconds.  This ensures that ramps are properly gradual over time, regardless of the tempo setting or tempo changes.

## Articulation ramp support

Noir notation is updated to add a new articulation ramp feature (which is now also supported in NMF).  This is a backwards-compatible change.  Currently existing Noir notation files will simply have the ramp always set to zero for all notes.

Three atomic operators are added, `<` and `>` and the grave accent.  These represent the visual appearance of the desired ramp, with `<` meaning a ramp from zero to one and `>` meaning a ramp from one to zero.  The grave accent is used to mark the end of a ramp.

Ramps may not be nested, and no ramp may be open when the Noir file ends, or the `$` `@` or `:` operators are encountered.

When a ramp is opened, Noir will remember the cursor position when the ramp was opened and the direction of the ramp.  Noir will remember how many note events were output to the event buffer while the ramp was opened.  When a ramp is closed, the cursor position must be at least one beyond the cursor position when the ramp was opened.  All the notes that were output will the ramp was open are updated to receive the beginning and ending time offsets of the ramp, which have been added as additional fields in the event buffer.
