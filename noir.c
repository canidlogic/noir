/*
 * noir.c
 * ======
 * 
 * The Noir notation program compiles a textual description of music
 * into a sequencer program that can then be synthesizer.
 * 
 * Syntax
 * ------
 * 
 *   noir [config] [template]
 * 
 * [config] is the path to a Shastina %noir-config; configuration file
 * to read.  This configuration file sets all the compilation
 * parameters.
 * 
 * [template] is the template to use for sequencer output.
 * 
 * Operation
 * ---------
 * 
 * The configuration file is interpreted to set up all the compilation
 * parameters.  Then, a text file containing Noir notation is read from
 * standard input and compiled into a sequence of events and a set of
 * layers using the compilation parameters.
 * 
 * Next, the template file is used to establish the format of the
 * output, which is written to standard output.  At the indicated places
 * in the template, Noir inserts the compiled events and layers.
 * 
 * The result is a sequencer file written to standard output.
 * 
 * Input file syntax
 * -----------------
 * 
 * See the example configuration file for documentation of the
 * configuration file.  The basic syntax is Shastina.
 * 
 * The template file is a binary file where escape commands begin with
 * the ASCII grave accent code.  The following escape commands are
 * available (case sensitive):
 * 
 *   `` - output a literal ASCII grave accent
 *   `N - output all the sequenced note commands
 *   `L - output all the sequenced layer commands
 * 
 * The N and L escape commands must each be used exactly once in the
 * template file.  Noir currently outputs all note and layer commands
 * using Retro synthesizer syntax.
 * 
 * See the Noir notation specification for the details of how the input
 * text file that is read from standard input should be arranged.
 * 
 * @@TODO:
 */
