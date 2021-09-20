# Noir
Music notation system based on ASCII plain-text files.

See `noir_spec.md` in the `doc` directory for a specification of the ASCII music notation format.

The main program provided by this distribution is `noir`, which compiles the ASCII music notation format into a special binary NMF format.  See [libnmf](https://www.purl.org/canidtech/r/libnmf) for more about the NMF format.

The `noir` program takes Noir music notation on standard input and writes NMF output to standard output.  The NMF file will have a quantum basis of 96 quanta per quarter note, and it may contain cues (with duration zero) and grace notes (with negative duration).

## Release history

### Version 0.5.3 Beta

Moved NMF module and tools to external `libnmf` project, and added support for cues.

### Version 0.5.2 Beta

Updated documentation to use MarkDown format, along with some minor edits to documentation.

### Version 0.5.1 Beta

A few small fixes, mostly upgrading to use the new Shastina 0.9.2 beta in `nmftempo` (the only place Shastina is currently used).  Also dropped the unimplemented and unfinished Retro configuration Shastina file from the `doc` directory.

### Version 0.5.0 Beta

The Noir version that has already been used for several projects.  This version only includes support for the Retro synthesizer.
