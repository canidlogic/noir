/* 
 * nvm.c
 * 
 * Implementation of nvm.h
 * 
 * See the header for further information.
 */

/* @@TODO: */
#include "nvm.h"
#include <stdio.h>
void nvm_pitchset_clear(NVM_PITCHSET *ps) { }
int nvm_pitchset_isEmpty(const NVM_PITCHSET *ps) { return 1; }
void nvm_pitchset_add(NVM_PITCHSET *ps, int32_t pitch) { }
void nvm_pitchset_drop(NVM_PITCHSET *ps, int32_t pitch) { }
int32_t nvm_pitchset_find(const NVM_PITCHSET *ps) { return 0; }
int nvm_pset(const NVM_PITCHSET *ps, int *per) {
  fprintf(stderr, "nvm_pset\n");
  return 1;
}
int nvm_dur(int32_t q, int *per) {
  fprintf(stderr, "nvm_dur %d\n", (int) q);
  return 1;
}
int nvm_eof(int *per) {
  fprintf(stderr, "nvm_eof\n");
  return 1;
}
int nvm_op_repeat(int *per) {
  fprintf(stderr, "nvm_op_repeat\n");
  return 1;
}
int nvm_op_multiple(int32_t t, int *per) {
  fprintf(stderr, "nvm_op_multiple %d\n", (int) t);
  return 1;
}
int nvm_op_section(int *per) {
  fprintf(stderr, "nvm_op_section\n");
  return 1;
}
int nvm_op_return(int *per) {
  fprintf(stderr, "nvm_op_return\n");
  return 1;
}
int nvm_op_pushloc(int *per) {
  fprintf(stderr, "nvm_op_pushloc\n");
  return 1;
}
int nvm_op_retloc(int *per) {
  fprintf(stderr, "nvm_op_retloc\n");
  return 1;
}
int nvm_op_poploc(int *per) {
  fprintf(stderr, "nvm_op_poploc\n");
  return 1;
}
int nvm_op_pushtrans(int32_t t, int *per) {
  fprintf(stderr, "nvm_op_pushtrans %d\n", (int) t);
  return 1;
}
int nvm_op_poptrans(int *per) {
  fprintf(stderr, "nvm_op_poptrans\n");
  return 1;
}
int nvm_op_immart(int art, int *per) {
  fprintf(stderr, "nvm_op_immart %d\n", art);
  return 1;
}
int nvm_op_pushart(int art, int *per) {
  fprintf(stderr, "nvm_op_pushart %d\n", art);
  return 1;
}
int nvm_op_popart(int *per) {
  fprintf(stderr, "nvm_op_popart\n");
  return 1;
}
int nvm_op_setbase(int32_t layer, int *per) {
  fprintf(stderr, "nvm_op_setbase %d\n", (int) layer);
  return 1;
}
int nvm_op_pushlayer(int32_t layer, int *per) {
  fprintf(stderr, "nvm_op_pushlayer %d\n", (int) layer);
  return 1;
}
int nvm_op_poplayer(int *per) {
  fprintf(stderr, "nvm_op_poplayer\n");
  return 1;
}
