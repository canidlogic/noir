# Noir
Music notation system based on ASCII plain-text files.

See `noir_spec.md` in the `doc` directory for a specification of the ASCII music notation format.

The main program provided by this distribution is `noir`, which compiles the ASCII music notation format into a special binary NMF format.  See `nmf_spec.md` in the `doc` directory for the specification of this NMF format.  The `nmf.h` and `nmf.c` files contain a codec for the NMF format.

Also included are a few utility programs that work with NMF files.

To check an NMF file and print a textual listing of its contents, you can use the included `nmfwalk` utility program.

To convert an NMF file with a quantum basis of 96 quanta per quarter note into an NMF file with a 44.1kHz or 48kHz quanta rate, you can use the `nmfrate` and `nmftempo` utility programs.  `nmfrate` uses a fixed tempo for the conversion.  `nmftempo` allows for variable tempi, including gradual tempo changes.  However, you must create a separate tempo map file in order to use `nmftempo`.  See the sample tempo map `tempo.txt` in the `doc` directory for further information.

To convert an NMF file into a data format that can be used with the Retro synthesizer, you can use the `nmfgraph` and `nmfsimple` utility programs.  `nmfsimple` converts an NMF file into a sequence of note events that can be included in a Retro synthesis script.  `nmfgraph` can convert specially-formatted NMF files into dynamic graphs for use with Retro.

## Release history

### Version 0.5.2 Beta

Updated documentation to use MarkDown format, along with some minor edits to documentation.

### Version 0.5.1 Beta

A few small fixes, mostly upgrading to use the new Shastina 0.9.2 beta in `nmftempo` (the only place Shastina is currently used).  Also dropped the unimplemented and unfinished Retro configuration Shastina file from the `doc` directory.

### Version 0.5.0 Beta

The Noir version that has already been used for several projects.  This version only includes support for the Retro synthesizer.
