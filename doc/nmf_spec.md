# Noir Music File (NMF) specification

- &#x200b;1. [Introduction](#mds1)
- &#x200b;2. [Data types](#mds2)
  - &#x200b;2.1 [Unsigned 32-bit integer](#mds2p1)
  - &#x200b;2.2 [Biased 32-bit integer](#mds2p2)
  - &#x200b;2.3 [Unsigned 16-bit integer](#mds2p3)
  - &#x200b;2.4 [Biased 16-bit integer](#mds2p4)
- &#x200b;3. [Quanta](#mds3)
- &#x200b;4. [File header](#mds4)
- &#x200b;5. [Section table](#mds5)
- &#x200b;6. [Note table](#mds6)

## <span id="mds1">1. Introduction</span>

The Noir Music Format (NMF) is a binary file format that is designed to store the results of interpreting a Noir notation file.  (See the Noir notation specification for more information about Noir notation.)  While Noir notation is a text format that is designed to be written manually by the user, NMF is designed to be easy to access from computer programs.

The intended architecture is that there is a Noir compilation program that converts Noir notation into binary NMF, and then all other Noir programs use the NMF file, saving them from having to interpret the textual Noir notation.

## <span id="mds2">2. Data types</span>

This section documents the data types used in NMF and how they are stored in an NMF byte stream.

### <span id="mds2p1">2.1 Unsigned 32-bit integer</span>

An unsigned 32-bit integer is stored with four bytes in big endian order, with the most significant byte first and the least significant byte last.  The most significant bit of the integer must always be zero, so the range is zero up to and including 2,147,483,647.

### <span id="mds2p2">2.2 Biased 32-bit integer</span>

A biased 32-bit integer is stored with four bytes in big endian order, with the most significant byte first and the least significant byte last.  The most significant bit need not be zero, but at least one bit must be non-zero, so the raw numeric range is one up to and including 4,294,967,295.

The actual value stored in the biased integer is the raw numeric value subtracted by 2,147,483,648.  This results in an actual value range of -2,147,483,647 up to and including 2,147,483,647.

### <span id="mds2p3">2.3 Unsigned 16-bit integer</span>

An unsigned 16-bit integer is stored with two bytes in big endian order, with the most significant byte first and the least significant byte second.  The most significant bit of the integer need not be zero, so the range is zero up to and including 65,535.

### <span id="mds2p4">2.4 Biased 16-bit integer</span>

A biased 16-bit integer is stored with two bytes in big endian order, with the most significant byte first and the least significant byte second.  The most significant bit of the integer need not be zero, but at least one bit must be non-zero, so the range is one up to and including 65,535.

The actual value stored in the baised integer is the raw numeric value subtracted by 32,768.  This results in an actual value range of -32,767 up to and including 32,767.

## <span id="mds3">3. Quanta</span>

Rhythm and time units are counted in _quanta._  The exact meaning of a quanta varies depending on what _basis_ is used in the NMF file.  There are three kinds of basis values currently supported:

1. 96 quanta equals one quarter note
2. 44,100 quanta per second
3. 48,000 quanta per second

The quarter-note basis has variable-length quantum duration, depending on the specific tempo.  The other two bases have fixed-duration quanta.

## <span id="mds4">4. File header</span>

An NMF file begins with the following header:

1. \[Unsigned 32-bit integer\] __Primary signature__
2. \[Unsigned 32-bit integer\] __Secondary signature__
3. \[Unsigned 16-bit integer\] __Quantum basis__
4. \[Unsigned 16-bit integer\] __Section count__
5. \[Unsigned 32-bit integer\] __Note count__

The primary signature must be the value 1,928,196,216.  The secondary signature must be the value 1,313,818,926.  If these two signatures are not present with those values, then the file is not an NMF file.

The quantum basis must have one of the following values, which selects a quantum basis described in &sect;3 [Quanta](#mds3).  A value of zero selects 96 quanta per quarter note.  A value of one selects 44,100 quanta per second.  A value of two selects 48,000 quanta per second.

The section count is the total number of sections in the section table (see &sect;5 [Section table](#mds5)).  The range of the section count is one up to and including 65,535.

The note count is the total number of notes in the note table (see &sect;6 [Note table](#mds6)).  The range of the note count is one up to and including 1,048,576.

## <span id="mds5">5. Section table</span>

The section table stores the starting offset of each section in the composition.  The total number of sections is given in the file header (see &sect;4 [File header](#mds4)).

The section table has one unsigned 32-bit integer for each section.  Each integer indicates how many quanta have elapsed from the start of the composition up to the start of the section.  The first section must always have a value of zero, meaning it starts at the beginning of the composition.

If there is more than one section, each section after the first must have a starting offset that is greater than or equal to the starting offset of the previous section.

The section index of a particular section is its zero-based index in the section table.  Therefore, the first section has section index zero, the second section has section index one, and so forth.  Each note in the note table (see &sect;6 [Note table](#mds6)) has a section index identifying which section it belongs to.  All notes in a section must have starting offsets that are greater than or equal to the starting offset of the section given in the section table.

Note that earlier sections may overlap with later sections.  That is, an event in section 2 might take place after an event in section 3, for example.

## <span id="mds6">6. Note table</span>

The note table stores all the notes of the composition.  The total number of notes is given in the file header (see &sect;4 [File header](#mds4)).  The notes may be in any order; there are no sorting requirements.  Each note has the following structure:

1. \[Unsigned 32-bit integer\] __Time offset__
2. \[Biased 32-bit integer\] __Duration__
3. \[Biased 16-bit integer\] __Pitch__
4. \[Unsigned 16-bit integer\] __Articulation__
5. \[Unsigned 16-bit integer\] __Section index__
6. \[Unsigned 16-bit integer\] __Layer index__

The time offset is the number of quanta from the beginning of the composition to the note, with zero meaning the note occurs at the beginning of the composition.  The time offset does not include any grace note offset (see below), which may shift the note to a position before the indicated time offset.  The time offset must be greater than or equal to the starting offset of the section this note belongs to (see below).

The duration has a different meaning depending on whether it is greater than zero, zero, or less than zero.  If greater than zero, it is the duration of the note in quanta.  If less than zero, the note is a grace note and the absolute value of the duration indicates how far from the upcoming beat the grace note is.  That is, -1 means the grace note immediately before the beat, -2 means the grace note before the grace note at offset -1, -3 means the grace note before the note at offset -2, and so forth.  A duration of zero is never generated from Noir notation, but it is reserved for cue events and other special meanings.

The pitch is the number of semitones (half steps) away from middle C.  A value of zero means middle C, a value of -1 means B below middle C, a value of 2 means D above middle C, and so forth.  The valid range is \[-39, 48\], which covers all pitches on the 88-key piano keyboard.

The articulation is the type of articulation applied to the note.  Articulation zero is the default articulation.  The valid range is \[0, 61\].  NMF does not specify the meaning of any particular articulation.

The section index must be in range \[0, max\], where _max_ is one less than the section count given in the header.  The section index selects a particular section entry in the section table.  The time offset of the note must be greater than or equal to the starting offset of the section.

The layer index selects a layer within the particular section specified for this note.  The layer index is one less than the layer number, so a value of zero selects layer one within this section, one selects layer two within this section, and so forth.
