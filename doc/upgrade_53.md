# Upgrade path to beta 0.5.3

This document discusses the architectural changes to Noir moving from version 0.5.2 beta to 0.5.3 beta.

An earlier version of this document proposed much more aggressive changes from the 0.5.2 beta to the 0.5.3 beta.  Since then, major work has been done in upgrading Retro and on developing the mid-level "infrared" project.  In light of this, the changes needed for the 0.5.3 beta are actually much less extensive.

## libnmf

The NMF parsing modules are spun off the libnmf so that they can be used in other projects.  NMF tools are spun off to a nmftools project, which replaces the libnrb project.

## Cues

The grave accent character is added to Noir notation with an integer key to indicate that a cue should be be placed.  Cues are specific to sections, so the full cue access is a section followed by a cue number within the section.  Update the NMF specification to note that cues now _can_ be generated from Noir notation.

Cues use the articulation and layer field in combination to store the cue number, which allows for more than four million cues per section (since the layer only allows 64 values).

NMF utilities can gradually be updated to support cues.  Cues will be used within infrared to support graph construction.
