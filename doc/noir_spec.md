# Noir notation specification

- &#x200b;1. [Introduction](#mds1)
- &#x200b;2. [Pitch notation](#mds2)
  - &#x200b;2.1 [Pitch sets](#mds2p1)
- &#x200b;3. [Rhythm notation](#mds3)
  - &#x200b;3.1 [Rhythm groups](#mds3p1)
- &#x200b;4. [Interpreter state](#mds4)
  - &#x200b;4.1 [Event buffer](#mds4p1)
  - &#x200b;4.2 [Cursor position](#mds4p2)
  - &#x200b;4.3 [Current pitch and duration](#mds4p3)
  - &#x200b;4.4 [Section state](#mds4p4)
  - &#x200b;4.5 [Location, transposition, and layer state](#mds4p5)
  - &#x200b;4.6 [Articulation state](#mds4p6)
  - &#x200b;4.7 [Grace note state](#mds4p7)
- &#x200b;5. [Execution](#mds5)
  - &#x200b;5.1 [List of operators](#mds5p1)
    - &#x200b;5.1.1 [Atomic operators](#mds5p1p1)
    - &#x200b;5.1.2 [Integer operators](#mds5p1p2)
    - &#x200b;5.1.3 [Key operators](#mds5p1p3)
- &#x200b;6. [Event buffer format](#mds6)
  - &#x200b;6.1 [Time offset](#mds6p1)
  - &#x200b;6.2 [Duration](#mds6p2)
  - &#x200b;6.3 [Pitch](#mds6p3)
  - &#x200b;6.4 [Articulation](#mds6p4)
  - &#x200b;6.5 [Section](#mds6p5)
  - &#x200b;6.6 [Layer](#mds6p6)

## <span id="mds1">1. Introduction</span>

This specification defines a plain-text US-ASCII format for notating music  compositions, called _Noir_ (pronounced "nwar").  Noir is designed to be entered manually by the user using a plain-text editor.

Interpreting a Noir notation file builds a description of all the notes in a music composition.  This detailed description of the music can then be transformed into other media such as MIDI, sequencer files, and cue sheets, by using the Noir music renderer.

## <span id="mds2">2. Pitch notation</span>

Musical pitch is notated in Noir using the following syntax:

1. Pitch name
2. Zero or more accidentals
3. Zero or more register marks

No whitespace is allowed within a pitch description.  Multiple pitch descriptions in a row may be separated from each other with any amount of whitespace, or the pitches may be written one after another without any separation.  There is no possibility for ambiguity even when pitches are written together without any whitespace separation.

The pitch name must be one capital letter `A-G` or one lowercase letter `a-g`.  The lowercase letters `cdefgab` (in that order) represent the octave beginning on middle C.  The uppercase letters `CDEFGAB` (in that order) represent the octave below middle C.  The lowercase letter `a` is the standard A-440 (fundamental frequency 440 hertz), also known as A4.  The following table gives the number of semitones above or below middle C for each pitch letter:

- `b` is 11 semitones above middle C
- `a` is 9 semitones above middle C
- `g` is 7 semitones above middle C
- `f` is 5 semitones above middle C
- `e` is 4 semitones above middle C
- `d` is 2 semitones above middle C
- `c` is middle C
- `B` is 1 semitone below middle C
- `A` is 3 semitones below middle C
- `G` is 5 semitones below middle C
- `F` is 7 semitones below middle C
- `E` is 8 semitones below middle C
- `D` is 10 semitones below middle C
- `C` is 12 semitones below middle C

Each uppercase pitch letter is exactly 12 semitones (one octave) below the corresponding lowercase pitch letter.

Accidentals are used to modify the pitch by semitones (half steps).  A "natural" accidental is also provided for sake of completeness, even though it has no effect on the pitch in Noir.  (Noir does not have any concept of key or scale.)  Unlike the pitch names, accidentals are case-insensitive; they may be used in either uppercase or lowercase with no difference in meaning.  The following accidentals are available:

- `X` is a double-sharp, which raises pitch two semitones
- `S` is a sharp, which raises pitch one semitone
- `N` is a natural, which has no effect on pitch
- `H` is a flat, which lowers pitch one semitone
- `T` is a double-flat, which lowers pitch two semitones

When multiple accidentals are used, the effect is cumulative.  Therefore, `bxth` would be equivalent `bh`, because the cumulative effect of `xth` is to lower pitch by one semitone, which is equivalent to `h`.  Furthermore, Noir assumes an equal-tempered scale, so for example `es` is equivalent to `f`.

Register marks are used to modify the pitch by octaves.  (An octave is twelve semitones.)  Two register marks are available:

- `'` (apostrophe) moves the pitch up one octave
- `,` (comma) moves the pitch down one octave

As with accidentals, when multiple register marks are used, their effect is cumulative.  After all accidentals and register marks have been applied to a pitch, the resulting pitch must be within the range of an 88-key piano, which is from `A,,,` up to and including `c''''`

Note that the apostrophe and comma are also used as rhythm modifiers (see &sect;3 [Rhythm notation](#mds3)).  Transposition functionality is available, too (see &sect;4.5 [Location, transposition, and layer state](#mds4p5)).

### <span id="mds2p1">2.1 Pitch sets</span>

A _pitch set_ is a set of zero or more pitches.  Pitch sets are represented internally by a bitmap, where each bit corresponds to one key on the 88-key piano.  Pitches that are included in the pitch set have the corresponding bit set, and pitches that are not included have the corresponding bit clear.

Specifying a specific pitch more than once in a pitch set has no effect.  Specifying an enharmonic equivalent (such as both `es` and `f`) also has no effect.

Pitch sets are enclosed within parentheses.  Parentheses may be nested, though nesting pitch groups is no different than just specifying all the pitches on the same level.  An empty set such as `()` is allowed, and means a rest.  Rests can also be specified with the special notation `R` or `r`, which both have the same meaning as `()`.  The `R` notation may be included within pitch sets, but it has no effect since it has no component pitches.

Note that Noir thinks of rests as the absence of pitch (an empty pitch set).  Multi-pitch sets work like chords.

Summary of pitch set notation:

- `()` enclose pitch sets
- `R` or `r` is shorthand for `()`

## <span id="mds3">3. Rhythm notation</span>

Rhythm in Noir is based on the concept of a _quantum,_ which is the minimal unit of rhythm in Noir.  All Noir rhythms can be represented as an integer number of quanta.  The duration of a quantum is not fixed, but varies depending on the current tempo.  Having variable-length quanta allows for rhythm to be specified independent of any particular tempo, as is the usual practice in music notation.

Rhythmic durations are notated in Noir using the following syntax:

1. Unit number
2. Rhythm suffix

The unit number is one of the ten decimal digits.  The following table shows what traditional rhythm symbol each unit number is meant to represent, and how many quanta each unit number has (except for the unmeasured grace note):

     Number |         Name          | Quanta
    --------|-----------------------|--------
        0   | unmeasured grace note |   --
        1   | 64th note             |      6
        2   | 32nd note             |     12
        3   | 16th note             |     24
        4   | 8th note              |     48
        5   | quarter note          |     96
        6   | half note             |    192
        7   | whole note            |    384
        8   | 8th note triplet      |     32
        9   | quarter note triplet  |     64

Note that the two dedicated unit numbers for triplets are useful for representing swing rhythms.  Also note that 128th notes, double whole notes, 16th note triplets, and half note triplets can be represented using rhythm suffixes, as described below.

All rhythmic units can be optionally modified by a _rhythm suffix,_ except for the grace note.  At most one rhythm suffix may be used; unlike for pitch suffixes, __it is not possible to stack multiple rhythm suffixes for cumulative effect.__  This means that double-dotted rhythms are not directly supported, though they can be represented with a [Rhythm group](#mds3p1) (&sect;3.1).  Each rhythm suffix multiplies the number of quanta by a certain value shown in the table below:

- `'` (apostrophe) doubles the duration
- `.` (period) multiplies the duration by 1.5
- `,` (comma) makes the duration half as long

The result of applying a suffix to a rhythmic unit is always another integer value of quanta.

No whitespace is allowed between the unit number and the rhythm suffix.  As with pitches, whitespace is optional between multiple rhythm units in a row.

Note that the apostrophe and comma are also used as pitch modifiers (see &sect;2 [Pitch notation](#mds2))

### <span id="mds3p1">3.1 Rhythm groups</span>

A longer rhythmic duration may be built out of multiple shorter rhythmic durations added together.  In Noir, this is accomplished with rhythm groups.  Rhythm groups have the same function as ties in traditional notation.

Rhythm groups are enclosed within `[]` brackets.  There must be at least one rhythmic unit within the brackets.  Also, grace notes are not allowed within rhythm groups, since they are unmeasured.  Rhythm groups may be nested, although nested rhythm groups are no different from having all the rhythmic units in the top-level group.

Only the total number of quanta within a rhythm group matters.  So, for example, the group `[667]` is equivalent to `[77]` and both are also equivalent to `7'` because all have a total of 768 quanta.

Summary of rhythm group notation:

- `[]` encloses rhythm groups

## <span id="mds4">4. Interpreter state</span>

Noir files are interpreted from start to finish.  This section describes the state variables that are used during interpretation.

### <span id="mds4p1">4.1 Event buffer</span>

All note and cue events that are output during interpretation are stored in the event buffer, in the order that they were output.  The event buffer is only referred to during grace note flushes (see &sect;4.7 [Grace note state](#mds4p7)), at which point the most recently output grace note events are modified to correct their grace note offsets.

The event buffer starts out empty.  See &sect;5 [Execution](#mds5) for details of how the event buffer is filled.

### <span id="mds4p2">4.2 Cursor position</span>

The cursor position is the current offset of quanta from the beginning of the composition, where a cursor position of zero is the start of the whole composition.

The `@` and `:` operators cause the cursor to jump position, to the current value of the base time offset (see &sect;4.4 [Section state](#mds4p4)) or to the value on top of the location stack, respectively.  An error occurs if `:` is used when the location stack is empty.

When a rest, pitch, pitch set, or `/` operator is encountered in the input, the cursor advances by the current value of the duration register _after_ any notes have been output.  An error occurs if the duration register is undefined in this case.  The duration register has a value of zero and hence no advance occurs when the current duration is a grace note.

The `\` operator is handled as the equivalent of a sequence of `/` operators.

### <span id="mds4p3">4.3 Current pitch and duration</span>

Two state registers keep track of the current pitch and duration at all times.  At the beginning of interpretation and after the pitch and duration registers have been reset, they contain special codes indicating that the registers are undefined.

Each time a pitch or pitch set is encountered, the pitch set is copied into the pitch register, overwriting any of the current contents.  Rests are considered to be empty pitch sets (see &sect;2.1 [Pitch sets](#mds2p1)), so rests cause the pitch register to be set to an empty pitch set.

Each time a rhythm or rhythm group is encountered, the total duration in quanta is copied into the duration register, overwriting any of the current contents.  Unmeasured grace notes are represented by a duration of zero quanta in the duration register.

The `$` `@` and `:` operations cause both the pitch and duration registers to be reset to undefined state.

Whenever the duration register contains a grace note value and then is reset or changed to a non-grace duration, this triggers a grace note flush (see &sect;4.7 [Grace note state](#mds4p7)).

### <span id="mds4p4">4.4 Section state</span>

The section count register and the base time offset register keep track of the sections in the composition.

The section count register contains the current section number.  It starts out at zero.  It is incremented at each `$` operation.

The base time offset contains the start of the current section as an offset in quanta from the beginning of the composition.  It starts out at zero.  At each `$` operation, the current value of the cursor position is copied into the base time offset register.

Note that events in the preceding sections might overlap with events in subsequent sections.  The start of a section only guarantees that no subsequent event will come earlier than the start of the new section.

### <span id="mds4p5">4.5 Location, transposition, and layer state</span>

Locations, transpositions, and layers are tracked with stacks.  The location stack stores locations as offsets in quanta from the beginning of the composition.  The transposition stack stores current transposition settings as positive or negative displacements in semitones.  The layer stack stores pairs of section numbers and layer IDs within those sections.

There is also a base layer register, which stores a section number and a layer ID within that section.

The location, transposition, and layer stacks start out empty.  The base layer register starts out with the first layer in section number zero.

When the `$` and `@` operators or the end of the input file occurs, the location, transposition, and layer stacks must be empty or an error occurs.

The `$` and `@` operators cause the base layer register to reset, _after_ those operators have made changes to the section state.  The base layer register is then reset to the first layer within the current section.

Whenever pitches and pitch sets are encountered in the input, each pitch is adjusted by the current transposition displacement on top of the transposition stack.  If the transposition stack is empty, a displacement of zero is assumed.

Whenever note events are output, their section and layer IDs are set to the values on top of the layer stack.  If the layer stack is empty, the value in the base layer register is used instead.

The `{` operator causes the current cursor location to be pushed on top of the location stack.  The `:` operator causes the current cursor location to jump to the location on top of the location stack (with an error if the location stack is empty).  The `}` operator causes the value on top of the location stack to be removed (with an error if the location stack is empty).

The `^` and `=` operators are used to push and pop the transposition stack, while the `+` and `-` operators are used to push and pop the layer stack.  The `&` operator is used to change the layer ID within the base layer register.

### <span id="mds4p6">4.6 Articulation state</span>

The articulation state consists of an articulation stack and an immediate articulation register.  The stack and the register hold articulations.  The register also has a special empty value.

When a rest, pitch, pitch set, or `/` operator is encountered in the input, then _after_ any note events have been output, the immediate articulation register is cleared to empty if not already empty.

The `\` operator is handled equivalent to a sequence of `/` operators, with the immediate articulation register cleared after the first iteration.  Therefore, the immediate articulation only applies to the first note in a sequence notated by the `\` operator.

When the `$` `@` and `:` operators or the end of the input file occur, the immediate articulation register must be empty or there is an error.

When the `$` and `@` operators or the end of the input file occurs, the articulation stack must be empty or there is an error.

Whenever note events are output, their articulation is set to the articulation in the immediate articulation register if it is not empty, otherwise the articulation on top of the articulation stack if the stack is not empty, otherwise to the first articulation index.

The `!` and `~` operators are used to push and pop the articulation stack.  The `*` operator is used to set an articulation in the immediate articulation register.

### <span id="mds4p7">4.7 Grace note state</span>

The grace note state consists of a grace event count register and a grace offset register.  Both registers contain integer values, and both registers start out at zero.

When a rest, pitch, pitch set, or `/` operator is encountered in the input and the current duration register indicates a grace note, then the grace offset register is incremented _before_ any note events are output.  The `\` operator is treated as equivalent to a sequence of `/` operators.

When the grace offset register contains a non-zero value, then note events that are output have their durations set to a special value that indicates the current grace note offset in the grace offset register.  Furthermore, each note event that is output when the grace offset register is non-zero causes the grace event count register to increment.

When a grace note flush is triggered by changes or a reset to the duration register (see &sect;4.3 [Current pitch and duration](#mds4p3)) or by a cue or by the end of the file, then the following things happen.  First, if the grace event count register is non-zero, then the last _n_ events in the event buffer, where _n_ is the grace event count register value, have their grace note offset flipped.  To flip a grace note offset, subtract it from one greater than the grace offset register value.  Second, reset both the grace event count register and the grace offset register to zero.

## <span id="mds5">5. Execution</span>

Noir notation is stored in a plain-text file.  Line breaks may be CR-only, LF-only, CR+LF, or LF+CR.  A UTF-8 Byte Order Mark (BOM) is allowed at the beginning of the file, but ignored by Noir.  Comments begin with the `#` character and run up to but excluding the next CR character, LF character, or the end of the file (whichever comes first).  Whitespace is defined as Horizontal Tab (HT), Space (SP), Carriage Return (CR), and Line Feed (LF).

Noir notation only uses US-ASCII.  Comments may use US-ASCII or any compatible superset thereof, such as UTF-8, CESU-8, ISO-8859-1, or Windows-1252.  Noir ignores all characters within the comments, except for the opening `#` and the closing line break.

The input file is read from start to finish in sequential order, and the notation is interpreted in the order it appears in the file.  The following kinds of entities are interpreted in the file:

1. Pitches (see &sect;2 [Pitch notation](#mds2))
2. Rests (see &sect;2.1 [Pitch sets](#mds2p1))
3. Pitch sets (see &sect;2.1 [Pitch sets](#mds2p1))
4. Rhythm units (see &sect;3 [Rhythm notation](#mds3))
5. Rhythm groups (see &sect;3.1 [Rhythm groups](#mds3p1))
6. Operators
7. End Of File (EOF)

Pitches are treated equivalent to pitch sets that contain one pitch.  Rests are treated equivalent to empty pitch sets.  Rhythm units and rhythm groups both are interpreted as durations specified as a count of quanta (with grace notes treated as having zero quanta).  This simplifies the entities to the following list:

1. Pitch sets (including pitches and rests)
2. Duration in quanta (covering rhythm units and rhythm groups)
3. Operators
4. End Of File (EOF)

Whitespace is optional betweeen entities and at the beginning and end.

Pitch sets cause the current pitch register to be set, with pitches modified by the current transposition displacement (see &sect;4.5 [Location, transposition, and layer state](#mds4p5)) before being set in the pitch register.  Then, if the pitch set is not empty (not a rest), note events are output to the event buffer for each unique pitch in the current pitch register.  Note events use the cursor position for the time offset, their articulation is determined by the current articulation state (see &sect;4.6 [Articulation state](#mds4p6)), and their layer is determined by the current layer state (see &sect;4.5 [Location, transposition, and layer state](#mds4p5)).  If the current duration register has a non-zero value, that is used for the duration.  Otherwise, see &sect;4.7 [Grace note state](#mds4p7) for how grace notes are handled.  Finally, the cursor position is advanced by the duration.

Durations cause the current duration register to be set.  If the duration is changed from a grace note (duration zero) to something else (non-zero), then a grace note flush is triggered &mdash; see &sect;4.7 [Grace note state](#mds4p7) for details.

Operators have various effects on the interpreter state.  Operators are documented below.

At the End Of File (EOF), the interpreter must be in a valid final state.  The location, transposition, layer, and articulation stacks must be empty, the immediate articulation register must also be empty, and there must be at least one note event in the event buffer for the state to be a valid final state.  A grace note flush is performed if necessary at EOF.

### <span id="mds5p1">5.1 List of operators</span>

Each operator begins with a non-alphanumeric character that is not a square bracket, not a parenthesis, and not the `#` character.  This allows operators to be distinguished from pitch sets, durations, and comments just by examining the first character.

There are three kinds of operator formats:

1. Atomic operators
2. Integer operators
3. Key operators

Atomic operators consist only of a single character.  Integer operators are an operator character followed by a signed integer followed by a `;` character.  The signed integer must be in 32-bit signed integer range.  Key operators are an operator character followed by a single case-sensitive alphanumeric character (`0-9` `A-Z` `a-z`).  No whitespace is allowed within operators.

#### <span id="mds5p1p1">5.1.1 Atomic operators</span>

    Operator /

The forward slash is a note repeater operation.  The current pitch register must be defined, or an error occurs.  The operator is equivalent to encountering a pitch, rest, or pitch set in the input that matches the current contents of the pitch register.  If the pitch register is not empty (a rest), note events are output to the event buffer for each unique pitch in the pitch set.  The current transposition settings are _not_ applied to the pitches, because the transposition was applied when the pitch register was first set.  Note events use the cursor position for the time offset, their articulation is determined by the current articulation state, and their layer is determined by the current layer state.  If the current duration is non-zero, that is used for the duration and the cursor is then advanced by that much.  Otherwise, see &sect;4.7 [Grace note state](#mds4p7) for how grace notes are handled.

    Operator $

The `$` operator marks the beginning of a section.  The location, transposition, layer, and articulation stacks must be empty, and the immediate articulation register must also be empty.  The current pitch and current duration registers are reset to empty.  Resetting the duration register might trigger a grace note flush.  Then, the section count register is incremented and the current cursor position is copied into the base time offset register.  Finally, the base layer register is set to the new section count value for the section number, and the first layer within the new section.

    Operator @

The `@` operator returns to the beginning of the current section.  The location, transposition, layer, and articulation stacks must be empty, and the immediate articulation register must also be empty.  The current pitch and current duration registers are reset to empty.  Resetting the duration register might trigger a grace note flush.  The base time offset register is copied into the current cursor position.  Finally, the base layer register is set to the first layer within the current section.

    Operator {

The `{` operator pushes the current cursor location on top of the location stack.

    Operator :

The `:` operator returns to the location on top of the location stack.  The location stack must not be empty, or an error occurs.  The immediate articulation register must be empty, or an error occurs.  The current pitch and current duration registers are reset to empty.  Resetting the duration register might trigger a grace note flush.  The location on top of the location stack is then copied into the current cursor position register (without changing the location stack).

    Operator }

The `}` operator removes the location on top of the location stack.  The location stack must not be empty when this operator is invoked.  This operator does not affect the current position.

    Operator =

The `=` operator removes the transposition setting on top of the transposition stack.  The transposition stack must not be empty when this operator is invoked.

    Operator ~

The tilde operator removes the articulation key on top of the articulation stack.  The articulation stack must not be empty when this operator is invoked.

    Operator -

The hyphen-minus operator removes a layer from the top of the layer stack.  The layer stack must not be empty when this operator is invoked.

#### <span id="mds5p1p2">5.1.2 Integer operators</span>

    Operator \

The backslash operator is equivalent to a sequence of forward slash operators `/`, with the number of forward slash operators equal to the given integer, which must be greater than zero.

    Operator ^

The `^` operator pushes a transposition setting on top of the transposition stack.  The integer value indicates how many semitones above or below the current transposition setting the new transposition setting should be.  If the transposition stack is empty, the value is pushed on the stack as-is.  If the transposition stack is not empty, the value is added to the value currently on top of the stack, and then this cumulative value is pushed on top of the stack.

    Operator &

The `&` operator sets the base layer register.  Only the layer ID within the base layer register is changed; the section number in the base layer register is unchanged.  The integer value indicates the new layer ID to set.  It must be greater than zero.

    Operator +

The `+` operator pushes a layer on top of the layer stack.  The given integer indicates the layer number, and the current value of the section count register is used for the section number.  The integer value must be greater than zero.

    Operator `

The grave accent operator emits a cue event to the NMF file at the current cursor location.  If there are buffered grace notes, this operator will cause a [grace note flush](#mds4p7) (&sect;4.7).  The integer parameter is a cue number within the current section.  The cue number must be zero or greater.  Noir will not check for the uniqueness of cue numbers, so you may define a cue number within a section however many times you wish, though this is not generally recommended.  Cues will be stored in the NMF output as notes that have zero duration and zero pitch.  The cue number will be encoded such that the layer field stores the 16 least significant bits of the cue number and the articulation field stores the most significant bits.

#### <span id="mds5p1p3">5.1.3 Key operators</span>

    Operator *

The asterisk operator sets an articulation in the immediate articulation register.

    Operator !

The `!` operator pushes an articulation on top of the articulation stack.

## <span id="mds6">6. Event buffer format</span>

At the end of a successful interpretation of a Noir notation file, the event buffer will include one event for each note in the composition.  There will also be a section table, which stores the starting time offset for each section of the piece.  This section of the specification describes the exact format of events in the event buffer.

### <span id="mds6p1">6.1 Time offset</span>

The time offset is a signed, 32-bit integer value that indicates the time at which the event begins.  It is always zero or greater, and measured in quanta.  For grace notes, the time offset indicates the beat that comes after the grace note sequence, and the duration (see &sect;6.2 [Duration](#mds6p2)) indicates where in the grace note sequence the grace note is located.

### <span id="mds6p2">6.2 Duration</span>

The duration is a signed, 32-bit integer value that indicates the duration of the note in quanta.  The special value of duration zero is not used by Noir, but is reserved for cue notes.  Values less than zero indicate that the note is an unmeasured grace note.  The absolute value of negative duration values indicates the position within the grace note sequence, with value one being the first grace note before the beat indicated by the time offset, two being the second grace note before the beat, and so forth.

### <span id="mds6p3">6.3 Pitch</span>

The pitch is a signed, 16-bit integer value that indicates the pitch of the note as a count of semitones away from middle C.  The valid range is \[-39, 48\], which is the range of an 88-key keyboard.  When the duration is zero, the pitch field may have some other meaning not defined here.

### <span id="mds6p4">6.4 Articulation</span>

The articulation is an unsigned, 16-bit integer value that indicates the articulation that is assigned to the note.  The following table shows how articulation keys are mapped to numeric articulation values:

     Key | Value || Key | Value || Key | Value || Key | Value
    -----|-------||-----|-------||-----|-------||-----|-------
      0  |    0  ||  K  |   20  ||  e  |   40  ||  y  |   60
      1  |    1  ||  L  |   21  ||  f  |   41  ||  z  |   61
      2  |    2  ||  M  |   22  ||  g  |   42
      3  |    3  ||  N  |   23  ||  h  |   43
      4  |    4  ||  O  |   24  ||  i  |   44
      5  |    5  ||  P  |   25  ||  j  |   45
      6  |    6  ||  Q  |   26  ||  k  |   46
      7  |    7  ||  R  |   27  ||  l  |   47
      8  |    8  ||  S  |   28  ||  m  |   48
      9  |    9  ||  T  |   29  ||  n  |   49
      A  |   10  ||  U  |   30  ||  o  |   50
      B  |   11  ||  V  |   31  ||  p  |   51
      C  |   12  ||  W  |   32  ||  q  |   52
      D  |   13  ||  X  |   33  ||  r  |   53
      E  |   14  ||  Y  |   34  ||  s  |   54
      F  |   15  ||  Z  |   35  ||  t  |   55
      G  |   16  ||  a  |   36  ||  u  |   56
      H  |   17  ||  b  |   37  ||  v  |   57
      I  |   18  ||  c  |   38  ||  w  |   58
      J  |   19  ||  d  |   39  ||  x  |   59

When the duration is zero, the articulation field may have some other meaning not defined here.

### <span id="mds6p5">6.5 Section</span>

The section is an unsigned, 16-bit integer value that identifies the number of the section the note was defined in.

### <span id="mds6p6">6.6 Layer</span>

The layer is an unsigned, 16-bit integer value that identifies the layer number within the section defined by the section field.  The numeric value stored in this field is one less than the layer number, such that layer one has a numeric value of zero, layer two has a numeric value of one, and so forth.
