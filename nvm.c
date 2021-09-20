/* 
 * nvm.c
 * 
 * Implementation of nvm.h
 * 
 * See the header for further information.
 */

#include "nvm.h"
#include "event.h"
#include <stdlib.h>
#include <string.h>

/*
 * Constants
 * =========
 */

/*
 * The maximum number of elements that can be kept on interpreter
 * stacks.
 */
#define NVM_MAXSTACK (1024)

/*
 * The initial capacity of interpreter stacks.
 */
#define NVM_INITCAP (8)

/*
 * Type declarations
 * =================
 */

/*
 * A layer register.
 */
typedef struct {
  
  /*
   * The section number.
   */
  uint16_t sect;
  
  /*
   * One less than the layer index.
   */
  uint16_t layer_i;
  
} NVM_LAYERREG;

/*
 * An integer stack.
 * 
 * Use the nvm_istack functions to manipulate.
 */
typedef struct {
  
  /*
   * The capacity of the stack in elements.
   */
  int32_t cap;
  
  /*
   * The number of elements on the stack.
   */
  int32_t count;
  
  /*
   * Pointer to the dynamically-allocated stack.
   */
  int32_t *pst;

} NVM_ISTACK;

/*
 * A layer stack.
 * 
 * Use the nvm_lstack functions to manipulate.
 */
typedef struct {
  
  /*
   * The capacity of the stack in elements.
   */
  int32_t cap;
  
  /*
   * The number of elements on the stack.
   */
  int32_t count;
  
  /*
   * Pointer to the dynamically-allocated stack.
   */
  NVM_LAYERREG *pst;
  
} NVM_LSTACK;

/*
 * Static data
 * ===========
 */

/*
 * Flag indicating whether the module has been initialized yet.
 */
static int m_nvm_init = 0;

/*
 * The cursor position.
 * 
 * Only valid if m_nvm_init.
 */
static int32_t m_nvm_cursor = 0;

/*
 * The current pitch register.
 * 
 * Only valid if m_nvm_init.
 * 
 * m_nvm_pitch_filled indicates whether the pitch register is holding a
 * value.  If zero, it means the pitch register is undefined.
 */
static int m_nvm_pitch_filled = 0;
static NVM_PITCHSET m_nvm_pitch;

/*
 * The current duration register.
 * 
 * Only valid if m_nvm_init.
 * 
 * The special value of -1 means the register is undefined.
 */
static int32_t m_nvm_dur = -1;

/*
 * The current section register.
 * 
 * Only valid if m_nvm_init.
 */
static int32_t m_nvm_sect = 0;

/*
 * The base time register.
 * 
 * Only valid if m_nvm_init.
 */
static int32_t m_nvm_baset = 0;

/*
 * The location stack.
 * 
 * Only valid if m_nvm_init.
 */
static NVM_ISTACK m_nvm_locstack;

/*
 * The transposition stack.
 * 
 * Only valid if m_nvm_init.
 */
static NVM_ISTACK m_nvm_transstack;

/*
 * The layer stack.
 * 
 * Only valid if m_nvm_init.
 */
static NVM_LSTACK m_nvm_layerstack;

/*
 * The base layer.
 * 
 * Only valid if m_nvm_init.
 */
static NVM_LAYERREG m_nvm_baselayer;

/*
 * The articulation stack.
 * 
 * Only valid if m_nvm_init.
 */
static NVM_ISTACK m_nvm_artstack;

/*
 * The immediate articulation register.
 * 
 * Only valid if m_nvm_init.
 * 
 * The special value of -1 means the register is empty.
 */
static int32_t m_nvm_immart = -1;

/*
 * The grace note count register.
 * 
 * Only valid if m_nvm_init.
 */
static int32_t m_nvm_gracecount = 0;

/*
 * The grace note offset register.
 * 
 * Only valid if m_nvm_init.
 */
static int32_t m_nvm_graceoffset = 0;

/*
 * Local functions
 * ===============
 */

/* Prototypes */
static void nvm_graceFlush(void);
static void nvm_resetCurrent(void);
static void nvm_init(void);

static void nvm_lstack_init(NVM_LSTACK *ps);
static int nvm_lstack_isEmpty(NVM_LSTACK *ps);
static int nvm_lstack_push(NVM_LSTACK *ps, const NVM_LAYERREG *pv);
static int nvm_lstack_pop(NVM_LSTACK *ps);
static int nvm_lstack_peek(NVM_LSTACK *ps, NVM_LAYERREG *pv);

static void nvm_istack_init(NVM_ISTACK *ps);
static int nvm_istack_isEmpty(NVM_ISTACK *ps);
static int nvm_istack_push(NVM_ISTACK *ps, int32_t v);
static int nvm_istack_pop(NVM_ISTACK *ps);
static int nvm_istack_peek(NVM_ISTACK *ps, int32_t *pv);

static int nvm_bit_most(uint64_t v);
static int nvm_bit_least(uint64_t v);

/*
 * Flush a grace note sequence, if necessary.
 */
static void nvm_graceFlush(void) {
  
  /* Initialize if needed */
  nvm_init();
  
  /* Flush grace notes if necessary */
  if (m_nvm_gracecount > 0) {
    event_flip(m_nvm_gracecount, m_nvm_graceoffset);
  }
  
  /* Clear grace note state */
  m_nvm_gracecount = 0;
  m_nvm_graceoffset = 0;
}

/*
 * Reset the current pitch and current duration registers to empty.
 * 
 * A grace note flush is performed if necessary.
 */
static void nvm_resetCurrent(void) {
  
  /* Initialize if needed */
  nvm_init();
  
  /* Grace note flush if necessary */
  nvm_graceFlush();
  
  /* Clear registers */
  m_nvm_pitch_filled = 0;
  nvm_pitchset_clear(&m_nvm_pitch);
  m_nvm_dur = -1;
}

/*
 * Initialize the virtual machine module if not already initialized.
 */
static void nvm_init(void) {
  
  /* Only proceed if not yet initialized */
  if (!m_nvm_init) {
    
    /* Set initial state */
    m_nvm_cursor = 0;
    
    m_nvm_pitch_filled = 0;
    nvm_pitchset_clear(&m_nvm_pitch);
    m_nvm_dur = -1;
    
    m_nvm_sect = 0;
    m_nvm_baset = 0;
    
    nvm_istack_init(&m_nvm_locstack);
    nvm_istack_init(&m_nvm_transstack);
    nvm_lstack_init(&m_nvm_layerstack);
    
    memset(&m_nvm_baselayer, 0, sizeof(NVM_LAYERREG));
    m_nvm_baselayer.sect = 0;
    m_nvm_baselayer.layer_i = 0;
    
    nvm_istack_init(&m_nvm_artstack);
    m_nvm_immart = -1;
    
    m_nvm_gracecount = 0;
    m_nvm_graceoffset = 0;
    
    /* Set the initialized flag */
    m_nvm_init = 1;
  }
}

/*
 * Initialize the given layer stack structure.
 * 
 * Dynamic memory is allocated for the stack data, but there is no
 * method provided for freeing this memory.
 * 
 * Do not initialize the same stack structure more than once.
 * 
 * Parameters:
 * 
 *   ps - the stack structure to initialize
 */
static void nvm_lstack_init(NVM_LSTACK *ps) {
  
  /* Check parameter */
  if (ps == NULL) {
    abort();
  }
  
  /* Clear structure */
  memset(ps, 0, sizeof(NVM_LSTACK));
  
  /* Initialize capacity and count */
  ps->cap = NVM_INITCAP;
  ps->count = 0;
  
  /* Allocate stack data */
  ps->pst = (NVM_LAYERREG *) calloc(ps->cap, sizeof(NVM_LAYERREG));
  if (ps->pst == NULL) {
    abort();
  }
}

/*
 * Check whether the given stack is empty.
 * 
 * The stack must be initialized first.
 * 
 * Parameters:
 * 
 *   ps - the stack to check
 * 
 * Return:
 * 
 *   non-zero if empty, zero if not
 */
static int nvm_lstack_isEmpty(NVM_LSTACK *ps) {
  
  int result = 0;
  
  /* Check parameter */
  if (ps == NULL) {
    abort();
  }
  
  /* Check if empty */
  if (ps->count < 1) {
    result = 1;
  }
  
  /* Return result */
  return result;
}

/*
 * Push a layer register value onto the stack.
 * 
 * pv is the value to push.  ps must be an initialized stack.
 * 
 * The function fails if the stack has exceeded NVM_MAXSTACK elements.
 * 
 * Parameters:
 * 
 *   ps - the stack structure
 * 
 *   pv - the integer value to push
 * 
 * Return:
 * 
 *   non-zero if successful, zero if stack is full
 */
static int nvm_lstack_push(NVM_LSTACK *ps, const NVM_LAYERREG *pv) {
  
  int status = 1;
  int32_t newcap = 0;
  
  /* Check parameters */
  if ((ps == NULL) || (pv == NULL)) {
    abort();
  }
  
  /* Only proceed if we haven't reached limit */
  if (ps->count < NVM_MAXSTACK) {
    
    /* Expand stack if capacity full */
    if (ps->count >= ps->cap) {
      
      /* First, try to double capacity */
      newcap = ps->cap * 2;
      
      /* If doubled capacity exceeds limit, set to limit */
      if (newcap > NVM_MAXSTACK) {
        newcap = NVM_MAXSTACK;
      }
      
      /* Reallocate */
      ps->pst = (NVM_LAYERREG *) realloc(
                  ps->pst, newcap * sizeof(NVM_LAYERREG));
      if (ps->pst == NULL) {
        abort();
      }
      
      /* Clear new space */
      memset(
        &((ps->pst)[ps->cap]),
        0,
        (newcap - ps->cap) * sizeof(NVM_LAYERREG));
      
      /* Update capacity */
      ps->cap = newcap;
    }
    
    /* Add new element */
    memcpy(&((ps->pst)[ps->count]), pv, sizeof(NVM_LAYERREG));
    (ps->count)++;
    
  } else {
    /* Stack is full to limit */
    status = 0;
  }
  
  /* Return status */
  return status;
}

/*
 * Remove an element from the top of the stack.
 * 
 * ps is the stack to modify.  It must be initialized.
 * 
 * The function fails if the stack is currently empty.
 * 
 * Parameters:
 * 
 *   ps - the stack
 * 
 * Return:
 * 
 *   non-zero if successful, zero if stack was already empty
 */
static int nvm_lstack_pop(NVM_LSTACK *ps) {
  
  int status = 1;
  
  /* Check parameter */
  if (ps == NULL) {
    abort();
  }
  
  /* Only proceed if stack is not empty */
  if (ps->count > 0) {
    /* Just decrease the count by one */
    (ps->count)--;
    
  } else {
    /* Stack already empty */
    status = 0;
  }
  
  /* Return status */
  return status;
}

/*
 * Peek at the element on top of the stack without removing it.
 * 
 * ps is the initialized stack.  pv points to the variable to receive
 * the peeked value.
 * 
 * The function fails if the stack is currently empty.
 * 
 * Parameters:
 * 
 *   ps - the stack
 * 
 *   pv - pointer to variable to receive the top stack element
 * 
 * Return:
 * 
 *   non-zero if successful, zero if stack was empty
 */
static int nvm_lstack_peek(NVM_LSTACK *ps, NVM_LAYERREG *pv) {
  
  int status = 1;
  
  /* Check parameters */
  if ((ps == NULL) || (pv == NULL)) {
    abort();
  }
  
  /* Only proceed if stack is not empty */
  if (ps->count > 0) {
    /* Stack not empty, set return value */
    memcpy(pv, &((ps->pst)[ps->count - 1]), sizeof(NVM_LAYERREG));
    
  } else {
    /* Stack is empty */
    status = 0;
  }
  
  /* Return status */
  return status;
}

/*
 * Initialize the given integer stack structure.
 * 
 * Dynamic memory is allocated for the stack data, but there is no
 * method provided for freeing this memory.
 * 
 * Do not initialize the same stack structure more than once.
 * 
 * Parameters:
 * 
 *   ps - the stack structure to initialize
 */
static void nvm_istack_init(NVM_ISTACK *ps) {
  
  /* Check parameter */
  if (ps == NULL) {
    abort();
  }
  
  /* Clear structure */
  memset(ps, 0, sizeof(NVM_ISTACK));
  
  /* Initialize capacity and count */
  ps->cap = NVM_INITCAP;
  ps->count = 0;
  
  /* Allocate stack data */
  ps->pst = (int32_t *) calloc(ps->cap, sizeof(int32_t));
  if (ps->pst == NULL) {
    abort();
  }
}

/*
 * Check whether the given stack is empty.
 * 
 * The stack must be initialized first.
 * 
 * Parameters:
 * 
 *   ps - the stack to check
 * 
 * Return:
 * 
 *   non-zero if empty, zero if not
 */
static int nvm_istack_isEmpty(NVM_ISTACK *ps) {
  
  int result = 0;
  
  /* Check parameter */
  if (ps == NULL) {
    abort();
  }
  
  /* Check if empty */
  if (ps->count < 1) {
    result = 1;
  }
  
  /* Return result */
  return result;
}

/*
 * Push an integer value onto the stack.
 * 
 * v is the value to push.  ps must be an initialized stack.
 * 
 * The function fails if the stack has exceeded NVM_MAXSTACK elements.
 * 
 * Parameters:
 * 
 *   ps - the stack structure
 * 
 *   v - the integer value to push
 * 
 * Return:
 * 
 *   non-zero if successful, zero if stack is full
 */
static int nvm_istack_push(NVM_ISTACK *ps, int32_t v) {
  
  int status = 1;
  int32_t newcap = 0;
  
  /* Check parameters */
  if (ps == NULL) {
    abort();
  }
  
  /* Only proceed if we haven't reached limit */
  if (ps->count < NVM_MAXSTACK) {
    
    /* Expand stack if capacity full */
    if (ps->count >= ps->cap) {
      
      /* First, try to double capacity */
      newcap = ps->cap * 2;
      
      /* If doubled capacity exceeds limit, set to limit */
      if (newcap > NVM_MAXSTACK) {
        newcap = NVM_MAXSTACK;
      }
      
      /* Reallocate */
      ps->pst = (int32_t *) realloc(ps->pst, newcap * sizeof(int32_t));
      if (ps->pst == NULL) {
        abort();
      }
      
      /* Clear new space */
      memset(
        &((ps->pst)[ps->cap]),
        0,
        (newcap - ps->cap) * sizeof(int32_t));
      
      /* Update capacity */
      ps->cap = newcap;
    }
    
    /* Add new element */
    (ps->pst)[ps->count] = v;
    (ps->count)++;
    
  } else {
    /* Stack is full to limit */
    status = 0;
  }
  
  /* Return status */
  return status;
}

/*
 * Remove an element from the top of the stack.
 * 
 * ps is the stack to modify.  It must be initialized.
 * 
 * The function fails if the stack is currently empty.
 * 
 * Parameters:
 * 
 *   ps - the stack
 * 
 * Return:
 * 
 *   non-zero if successful, zero if stack was already empty
 */
static int nvm_istack_pop(NVM_ISTACK *ps) {
  
  int status = 1;
  
  /* Check parameter */
  if (ps == NULL) {
    abort();
  }
  
  /* Only proceed if stack is not empty */
  if (ps->count > 0) {
    /* Just decrease the count by one */
    (ps->count)--;
    
  } else {
    /* Stack already empty */
    status = 0;
  }
  
  /* Return status */
  return status;
}

/*
 * Peek at the element on top of the stack without removing it.
 * 
 * ps is the initialized stack.  pv points to the variable to receive
 * the peeked value.
 * 
 * The function fails if the stack is currently empty.
 * 
 * Parameters:
 * 
 *   ps - the stack
 * 
 *   pv - pointer to variable to receive the top stack element
 * 
 * Return:
 * 
 *   non-zero if successful, zero if stack was empty
 */
static int nvm_istack_peek(NVM_ISTACK *ps, int32_t *pv) {
  
  int status = 1;
  
  /* Check parameters */
  if ((ps == NULL) || (pv == NULL)) {
    abort();
  }
  
  /* Only proceed if stack is not empty */
  if (ps->count > 0) {
    /* Stack not empty, set return value */
    *pv = (ps->pst)[ps->count - 1];
    
  } else {
    /* Stack is empty */
    status = 0;
  }
  
  /* Return status */
  return status;
}

/*
 * Return the offset of the most significant bit that is set in the
 * given unsigned value.
 * 
 * An offset of zero means the least significant bit, an offset of one
 * means the second least significant bit, and so forth.
 * 
 * If v is zero, -1 is returned.
 * 
 * Parameters:
 * 
 *   v - the binary value to check
 * 
 * Return:
 * 
 *   the offset of the most significant bit that is set, or -1 if all
 *   bits are zero
 */
static int nvm_bit_most(uint64_t v) {
  
  int result = 0;
  
  /* Only proceed if v is non-zero */
  if (v != 0) {
    
    /* If most significant 32 bits are non-zero, then shift v right by
     * 32 and add 32 to result */
    if (v & UINT64_C(0xffffffff00000000)) {
      v >>= 32;
      result += 32;
    }
    
    /* v now confined to 32 bits; if most significant 16 bits are
     * non-zero, then shift v right by 16 and add 16 to result */
    if (v & UINT64_C(0xffff0000)) {
      v >>= 16;
      result += 16;
    }
    
    /* v now confined to 16 bits; if most significant 8 bits are
     * non-zero, then shift v right by 8 and add 8 to result */
    if (v & UINT64_C(0xff00)) {
      v >>= 8;
      result += 8;
    }
    
    /* v now confined to 8 bits; if most significant 4 bits are
     * non-zero, then shift v right by 4 and add 4 to result */
    if (v & UINT64_C(0xf0)) {
      v >>= 4;
      result += 4;
    }
    
    /* v now confined to 4 bits; if most significant 2 bits are
     * non-zero, then shift v right by 2 and add 2 to result */
    if (v & UINT64_C(0xc)) {
      v >>= 2;
      result += 2;
    }
    
    /* v now confined to 2 bits; if most significant bit is non-zero,
     * then shift v right by 1 and add 1 to result */
    if (v & UINT64_C(0x2)) {
      v >>= 1;
      result++;
    }
    
  } else {
    /* v is zero, so return -1 */
    result = -1;
  }
  
  /* Return result */
  return result;
}

/*
 * Return the offset of the least significant bit that is set in the
 * given unsigned value.
 * 
 * An offset of zero means the least significant bit, an offset of one
 * means the second least significant bit, and so forth.
 * 
 * If v is zero, -1 is returned.
 * 
 * Parameters:
 * 
 *   v - the binary value to check
 * 
 * Return:
 * 
 *   the offset of the least significant bit that is set, or -1 if all
 *   bits are zero
 */
static int nvm_bit_least(uint64_t v) {
  
  int result = 0;

  /* Only proceed if v is non-zero */
  if (v != 0) {
    
    /* If least significant 32 bits are zero, then shift v right by 32
     * and add 32 to result */
    if ((v & UINT64_C(0xffffffff)) == 0) {
      v >>= 32;
      result += 32;
    }

    /* Low 32 bits are now non-zero; if least significant 16 bits are
     * zero, then shift v right by 16 and add 16 to result */
    if ((v & UINT64_C(0xffff)) == 0) {
      v >>= 16;
      result += 16;
    }

    /* Low 16 bits are now non-zero; if least significant 8 bits are
     * zero, then shift v right by 8 and add 8 to result */
    if ((v & UINT64_C(0xff)) == 0) {
      v >>= 8;
      result += 8;
    }

    /* Low 8 bits are now non-zero; if least significant 4 bits are 
     * zero, then shift v right by 4 and add 4 to result */
    if ((v & UINT64_C(0xf)) == 0) {
      v >>= 4;
      result += 4;
    }

    /* Low 4 bits are now non-zero; if least significant 2 bits are
     * zero, then shift v right by 2 and add 2 to result */
    if ((v & UINT64_C(0x3)) == 0) {
      v >>= 2;
      result += 2;
    }

    /* Low 2 bits are now non-zero; if least significant bit is zero,
     * then shift v right by 1 and add 1 to result */
    if ((v & UINT64_C(0x1)) == 0) {
      v >>= 1;
      result++;
    }
    
  } else {
    /* v is zero, so return -1 */
    result = -1;
  }
  
  /* Return result */
  return result;
}

/*
 * Public function implementations
 * ===============================
 * 
 * See the header for specifications.
 */

/*
 * nvm_pitchset_clear function.
 */
void nvm_pitchset_clear(NVM_PITCHSET *ps) {
  
  /* Check parameter */
  if (ps == NULL) {
    abort();
  }
  
  /* Clear pitch set */
  memset(ps, 0, sizeof(NVM_PITCHSET));
  ps->a = 0;
  ps->b = 0;
}

/*
 * nvm_pitchset_isEmpty function.
 */
int nvm_pitchset_isEmpty(const NVM_PITCHSET *ps) {
  
  int result = 0;
  
  /* Check parameter */
  if (ps == NULL) {
    abort();
  }
  
  /* Check if empty */
  if ((ps->a == 0) && (ps->b == 0)) {
    result = 1;
  }
  
  /* Return result */
  return result;
}

/*
 * nvm_pitchset_add function.
 */
void nvm_pitchset_add(NVM_PITCHSET *ps, int32_t pitch) {
  
  int shv = 0;
  
  /* Check parameters */
  if ((ps == NULL) ||
      (pitch < NMF_MINPITCH) || (pitch > NMF_MAXPITCH)) {
    abort();
  }
  
  /* Set appropriate bit */
  if (pitch < 0) {
    /* Pitch is less than zero, so we are using the A register; invert
     * the pitch and decrement it to get the shift-left value */
    shv = (int) (-(pitch) - 1);
    
    /* Add the pitch if not already present */
    ps->a |= (((uint64_t) 1) << shv);
    
  } else {
    /* Pitch is zero or greater, so we are using the B register; the
     * pitch is the shift-right value from the most significant bit */
    shv = (int) pitch;
    
    /* Add the pitch if not already present */
    ps->b |= (UINT64_C(0x8000000000000000) >> shv);
  }
}

/*
 * nvm_pitchset_drop function.
 */
void nvm_pitchset_drop(NVM_PITCHSET *ps, int32_t pitch) {
  
  int shv = 0;
  
  /* Check parameters */
  if ((ps == NULL) ||
      (pitch < NMF_MINPITCH) || (pitch > NMF_MAXPITCH)) {
    abort();
  }
  
  /* Clear appropriate bit */
  if (pitch < 0) {
    /* Pitch is less than zero, so we are using the A register; invert
     * the pitch and decrement it to get the shift-left value */
    shv = (int) (-(pitch) - 1);
    
    /* Drop the pitch if present */
    ps->a &= ~(((uint64_t) 1) << shv);
    
  } else {
    /* Pitch is zero or greater, so we are using the B register; the
     * pitch is the shift-right value from the most significant bit */
    shv = (int) pitch;
    
    /* Drop the pitch if present */
    ps->b &= ~(UINT64_C(0x8000000000000000) >> shv);
  }
}

/*
 * nvm_pitchset_least function.
 */
int32_t nvm_pitchset_least(const NVM_PITCHSET *ps) {
  
  int32_t result = 0;
  
  /* Check parameter */
  if (ps == NULL) {
    abort();
  }
  
  /* Check that pitch set is not empty */
  if (nvm_pitchset_isEmpty(ps)) {
    abort();
  }
  
  /* Determine which register to use */
  if (ps->a) {
    /* Register A is non-zero, so determine lowest pitch there */
    result = (int32_t) nvm_bit_most(ps->a);
    if (result < 0) {
      abort();  /* Shouldn't happen */
    }
    
    /* Add one to result and then invert it to get the pitch offset */
    result = -(result + 1);
    
  } else {
    /* Register B is non-zero, so determine lowest pitch there */
    result = (int32_t) nvm_bit_most(ps->b);
    if (result < 0) {
      abort();  /* Shouldn't happen */
    }
    
    /* Determine the pitch from the bit offset */
    result = 63 - result;
  }
  
  /* Verify that result is in range */
  if ((result > NMF_MAXPITCH) || (result < NMF_MINPITCH)) {
    abort();  /* invalid pitch set */
  }
  
  /* Return result */
  return result;
}

/*
 * nvm_pitchset_most function.
 */
int32_t nvm_pitchset_most(const NVM_PITCHSET *ps) {
  
  int32_t result = 0;
  
  /* Check parameter */
  if (ps == NULL) {
    abort();
  }
  
  /* Check that pitch set is not empty */
  if (nvm_pitchset_isEmpty(ps)) {
    abort();
  }

  /* Determine which register to use */
  if (ps->b) {
    /* Register B is non-zero, so determine highest pitch there */
    result = (int32_t) nvm_bit_least(ps->b);

    if (result < 0) {
      abort();  /* Shouldn't happen */
    }

    /* Determine the pitch from the bit offset */
    result = 63 - result;
           
  } else {
    /* Register A is non-zero, so determine highest pitch there */
    result = (int32_t) nvm_bit_least(ps->a);
    if (result < 0) {
      abort();  /* Shouldn't happen */
    }
    
    /* Add one to result and then invert it to get the pitch offset */
    result = -(result + 1);
  }
  
  /* Verify that result is in range */
  if ((result > NMF_MAXPITCH) || (result < NMF_MINPITCH)) {
    abort();  /* invalid pitch set */
  }
  
  /* Return result */
  return result;
}

/*
 * nvm_pitchset_transpose function.
 */
int nvm_pitchset_transpose(NVM_PITCHSET *ps, int32_t offset) {
  
  int status = 1;
  int32_t bound_pitch = 0;
  int shv = 0;
  
  /* Check parameters */
  if (ps == NULL) {
    abort();
  }
  
  /* Only do something if offset is non-zero and pitch set non-empty */
  if ((offset != 0) && (!nvm_pitchset_isEmpty(ps))) {

    /* Determine whether transposing up or down */
    if (offset < 0) {

      /* Transposing down -- find lowest pitch in set */
      bound_pitch = nvm_pitchset_least(ps);
      
      /* Only proceed if lowest pitch transposed down is not out of
       * range; otherwise, fail */
      if (offset >= NMF_MINPITCH - bound_pitch) {
        
        /* We can safely transpose down without anything going out of
         * range; shift value is inverse of the offset */
        shv = (int) (-(offset));
        
        /* Begin by shifting register A left */
        ps->a <<= shv;
        
        /* Transfer the top bits of register B to the bottom of A */
        ps->a |= (ps->b >> (64 - shv));
        
        /* Shift register B left */
        ps->b <<= shv;
        
      } else {
        /* Transposition would take pitch out of range */
        status = 0;
      }
      
    } else {
      /* Transposing up -- find highest pitch in set */
      bound_pitch = nvm_pitchset_most(ps);
      
      /* Only proceed if highest pitch transposed up is not out of
       * range; otherwise, fail */
      if (offset <= NMF_MAXPITCH - bound_pitch) {
        
        /* We can safely tranpose up without anything going out of
         * range; shift value is the offset */
        shv = (int) offset;
        
        /* Begin by shifting register B right */
        ps->b >>= shv;
        
        /* Transfer the bottom bits of register A to the top of B */
        ps->b |= (ps->a << (64 - shv));
        
        /* Shift register A right */
        ps->a >>= shv;
      
      } else {
        /* Transposition would take pitch out of range */
        status = 0;
      }
    }
  }
  
  /* Return status */
  return status;
}

/*
 * nvm_pset function.
 */
int nvm_pset(const NVM_PITCHSET *ps, int *per) {
  
  int status = 1;
  NVM_PITCHSET pss;
  int32_t tranv = 0;
  
  /* Initialize structures */
  nvm_pitchset_clear(&pss);
  
  /* Check parameters */
  if ((ps == NULL) || (per == NULL)) {
    abort();
  }
  
  /* Initialize if necessary */
  nvm_init();
  
  /* Copy given pitch set to local variable */
  memcpy(&pss, ps, sizeof(NVM_PITCHSET));

  /* If transposition stack is not empty, get value on top of it; else,
   * set transposition value to zero */
  if (!nvm_istack_isEmpty(&m_nvm_transstack)) {
    /* Transposition stack not empty */
    if (!nvm_istack_peek(&m_nvm_transstack, &tranv)) {
      abort();  /* shouldn't happen */
    }
    
  } else {
    /* Transposition stack empty */
    tranv = 0;
  }

  /* Apply current transposition setting */
  if (!nvm_pitchset_transpose(&pss, tranv)) {
    status = 0;
    *per = ERR_TRANSRNG;
  }

  /* Copy new value to pitch register */
  if (status) {
    memcpy(&m_nvm_pitch, &pss, sizeof(NVM_PITCHSET));
    m_nvm_pitch_filled = 1;
  }

  /* Rest of operation is equivalent to running a repeat operation
   * here */
  if (status) {
    if (!nvm_op_repeat(per)) {
      status = 0;
    }
  }
  
  /* Return status */
  return status;
}

/*
 * nvm_dur function.
 */
int nvm_dur(int32_t q, int *per) {
  
  int status = 1;
  
  /* Check parameters */
  if ((q < 0) || (per == NULL)) {
    abort();
  }
  
  /* Initialize if necessary */
  nvm_init();
  
  /* If duration is being changed from a grace note to something else,
   * then trigger a grace note flush if necessary */
  if ((m_nvm_dur == 0) && (q != 0)) {
    nvm_graceFlush();
  }
  
  /* Set the new duration */
  m_nvm_dur = q;
  
  /* Return status */
  return status;
}

/*
 * nvm_eof function.
 */
int nvm_eof(int *per) {
  
  int status = 1;
  
  /* Check parameter */
  if (per == NULL) {
    abort();
  }
  
  /* Initialize if necessary */
  nvm_init();
  
  /* Location, transposition, layer, and articulation stacks must be
   * empty */
  if ((!nvm_istack_isEmpty(&m_nvm_locstack)) ||
      (!nvm_istack_isEmpty(&m_nvm_transstack)) ||
      (!nvm_lstack_isEmpty(&m_nvm_layerstack)) ||
      (!nvm_istack_isEmpty(&m_nvm_artstack))) {
    status = 0;
    *per = ERR_LINGER;
  }
  
  /* Immediate articulation register must be empty */
  if (status && (m_nvm_immart >= 0)) {
    status = 0;
    *per = ERR_DANGLEART;
  }
  
  /* Perform a grace note flush if necessary */
  if (status) {
    nvm_graceFlush();
  }
  
  /* Return status */
  return status;
}

/*
 * nvm_op_repeat function.
 */
int nvm_op_repeat(int *per) {
  
  int status = 1;
  int32_t durval = 0;
  int32_t art = 0;
  int32_t pitch = 0;
  NVM_LAYERREG lr;
  NVM_PITCHSET ps;
  
  /* Initialize structures */
  memset(&lr, 0, sizeof(NVM_LAYERREG));
  nvm_pitchset_clear(&ps);
  
  /* Check parameters */
  if (per == NULL) {
    abort();
  }
  
  /* Initialize if necessary */
  nvm_init();
  
  /* Current pitch must be defined */
  if (!m_nvm_pitch_filled) {
    status = 0;
    *per = ERR_NOPITCH;
  }
  
  /* Current duration must be defined */
  if (status && (m_nvm_dur < 0)) {
    status = 0;
    *per = ERR_NODUR;
  }
  
  /* If current duration is grace note, then increment grace offset
   * register, watching for overflow */
  if (status && (m_nvm_dur == 0)) {
    if (m_nvm_graceoffset < INT32_MAX) {
      m_nvm_graceoffset++;
    } else {
      status = 0;
      *per = ERR_HUGEGRACE;
    }
  }
  
  /* Determine the duration value that will be used */
  if (status) {
    if (m_nvm_graceoffset > 0) {
      /* Grace note offset, use inverse of that */
      durval = -(m_nvm_graceoffset);
      
    } else {
      /* No grace note offset, just use current duration */
      durval = m_nvm_dur;
    }
  }
  
  /* Determine the articulation value that will be used */
  if (status) {
    if (m_nvm_immart >= 0) {
      /* Immediate articulation, so grab that and clear the immediate
       * articulation register */
      art = m_nvm_immart;
      m_nvm_immart = -1;
      
    } else {
      /* No immediate articulation, try stack next */
      if (!nvm_istack_isEmpty(&m_nvm_artstack)) {
        /* Articulation stack not empty, so use value on top */
        if (!nvm_istack_peek(&m_nvm_artstack, &art)) {
          abort();  /* shouldn't happen */
        }
        
      } else {
        /* Articulation stack empty, so just use articulation zero */
        art = 0;
      }
    }
  }
  
  /* Determine section and layer */
  if (status) {
    if (!nvm_lstack_isEmpty(&m_nvm_layerstack)) {
      /* Layer stack not empty, so peek value on top */
      if (!nvm_lstack_peek(&m_nvm_layerstack, &lr)) {
        abort();  /* shouldn't happen */
      }
      
    } else {
      /* Layer stack empty, so use base layer */
      memcpy(&lr, &m_nvm_baselayer, sizeof(NVM_LAYERREG));
    }
  }
  
  /* Output notes */
  if (status) {
    
    /* Get a local copy of the pitch register */
    memcpy(&ps, &m_nvm_pitch, sizeof(NVM_PITCHSET));
    
    /* Output events until pitch set is empty */
    while (!nvm_pitchset_isEmpty(&ps)) {
      
      /* Get the lowest pitch in the pitch set */
      pitch = nvm_pitchset_least(&ps);

      /* Drop the pitch we just got from the set */
      nvm_pitchset_drop(&ps, pitch);

      /* Report the note event */
      if (!event_note(
              m_nvm_cursor,
              durval,
              pitch,
              art,
              lr.sect,
              ((int32_t) lr.layer_i) + 1)) {
        status = 0;
        *per = ERR_MANYNOTES;
      }

      /* Increase grace note count if grace note */
      if (status && durval < 0) {
        if (m_nvm_gracecount < INT32_MAX) {
          m_nvm_gracecount++;
        } else {
          status = 0;
          *per = ERR_HUGEGRACE;
        }
      }
      
      /* Leave loop if error */
      if (!status) {
        break;
      }
    }
  }
  
  /* If duration not a grace note, advance cursor by that much */
  if (status && (durval > 0)) {
    if (m_nvm_cursor <= INT32_MAX - durval) {
      m_nvm_cursor += durval;
    } else {
      status = 0;
      *per = ERR_LONGPIECE;
    }
  }
  
  /* Return status */
  return status;
}

/*
 * nvm_op_multiple function.
 */
int nvm_op_multiple(int32_t t, int *per) {
  
  int status = 1;
  int32_t i = 0;
  
  /* Check parameters */
  if (per == NULL) {
    abort();
  }
  
  /* Range-check t */
  if (t < 1) {
    status = 0;
    *per = ERR_MULTCOUNT;
  }
  
  /* Call through to repeat operation for each time */
  if (status) {
    for(i = 0; i < t; i++) {
      if (!nvm_op_repeat(per)) {
        status = 0;
        break;
      }
    }
  }
  
  /* Return status */
  return status;
}

/*
 * nvm_op_section function.
 */
int nvm_op_section(int *per) {
  
  int status = 1;
  
  /* Check parameter */
  if (per == NULL) {
    abort();
  }
  
  /* Initialize if necessary */
  nvm_init();
  
  /* Location, transposition, layer, and articulation stacks must be
   * empty */
  if ((!nvm_istack_isEmpty(&m_nvm_locstack)) ||
      (!nvm_istack_isEmpty(&m_nvm_transstack)) ||
      (!nvm_lstack_isEmpty(&m_nvm_layerstack)) ||
      (!nvm_istack_isEmpty(&m_nvm_artstack))) {
    status = 0;
    *per = ERR_LINGER;
  }
  
  /* Immediate articulation register must be empty */
  if (status && (m_nvm_immart >= 0)) {
    status = 0;
    *per = ERR_DANGLEART;
  }
  
  /* Increment section register, watching for limit of sections */
  if (status) {
    if (m_nvm_sect < NMF_MAXSECT - 1) {
      m_nvm_sect++;
    } else {
      status = 0;
      *per = ERR_MANYSECT;
    }
  }
  
  /* Report section to event module */
  if (status) {
    if (!event_section(m_nvm_cursor)) {
      status = 0;
      *per = ERR_MANYSECT;
    }
  }
  
  /* Reset current registers, copy cursor to base time offset, set base
   * layer to first layer in new section */
  if (status) {
    nvm_resetCurrent();
    m_nvm_baset = m_nvm_cursor;
    m_nvm_baselayer.sect = (uint16_t) m_nvm_sect;
    m_nvm_baselayer.layer_i = 0;
  }
  
  /* Return status */
  return status;
}

/*
 * nvm_op_return function.
 */
int nvm_op_return(int *per) {
  
  int status = 1;
  
  /* Check parameter */
  if (per == NULL) {
    abort();
  }
  
  /* Initialize if necessary */
  nvm_init();
  
  /* Location, transposition, layer, and articulation stacks must be
   * empty */
  if ((!nvm_istack_isEmpty(&m_nvm_locstack)) ||
      (!nvm_istack_isEmpty(&m_nvm_transstack)) ||
      (!nvm_lstack_isEmpty(&m_nvm_layerstack)) ||
      (!nvm_istack_isEmpty(&m_nvm_artstack))) {
    status = 0;
    *per = ERR_LINGER;
  }
  
  /* Immediate articulation register must be empty */
  if (status && (m_nvm_immart >= 0)) {
    status = 0;
    *per = ERR_DANGLEART;
  }
  
  /* Reset current registers, copy base time offset to cursor, and set
   * base layer to first layer */
  if (status) {
    nvm_resetCurrent();
    m_nvm_cursor = m_nvm_baset;
    m_nvm_baselayer.layer_i = 0;
  }
  
  /* Return status */
  return status;
}

/*
 * nvm_op_pushloc function.
 */
int nvm_op_pushloc(int *per) {
  
  int status = 1;
  
  /* Check parameter */
  if (per == NULL) {
    abort();
  }
  
  /* Initialize if necessary */
  nvm_init();
  
  /* Try pushing cursor location on location stack */
  if (!nvm_istack_push(&m_nvm_locstack, m_nvm_cursor)) {
    status = 0;
    *per = ERR_STACKFULL;
  }
  
  /* Return status */
  return status;
}

/*
 * nvm_op_retloc function.
 */
int nvm_op_retloc(int *per) {
  
  int status = 1;
  int32_t newloc = 0;
  
  /* Check parameter */
  if (per == NULL) {
    abort();
  }
  
  /* Initialize if necessary */
  nvm_init();
  
  /* Immediate articulation register must be empty */
  if (m_nvm_immart >= 0) {
    status = 0;
    *per = ERR_DANGLEART;
  }
  
  /* Try to peek top value of location stack */
  if (status) {
    if (!nvm_istack_peek(&m_nvm_locstack, &newloc)) {
      status = 0;
      *per = ERR_NOLOC;
    }
  }
  
  /* Reset current pitch and duration registers, then jump to new
   * location */
  if (status) {
    nvm_resetCurrent();
    m_nvm_cursor = newloc;
  }
  
  /* Return status */
  return status;
}

/*
 * nvm_op_poploc function.
 */
int nvm_op_poploc(int *per) {
  
  int status = 1;
  
  /* Check parameter */
  if (per == NULL) {
    abort();
  }
  
  /* Initialize if necessary */
  nvm_init();
  
  /* Try to pop the location stack */
  if (!nvm_istack_pop(&m_nvm_locstack)) {
    status = 0;
    *per = ERR_UNDERFLOW;
  }
  
  /* Return status */
  return status;
}

/*
 * nvm_op_pushtrans function.
 */
int nvm_op_pushtrans(int32_t t, int *per) {
  
  int status = 1;
  int64_t newtrans = 0;
  int32_t curtrans = 0;
  
  /* Check parameters */
  if (per == NULL) {
    abort();
  }
  
  /* Initialize if necessary */
  nvm_init();
  
  /* Compute the new transposition value, which is cumulative with the
   * current top of the stack */
  if (!nvm_istack_isEmpty(&m_nvm_transstack)) {
    /* Transposition stack not empty, so get current transposition */
    if (!nvm_istack_peek(&m_nvm_transstack, &curtrans)) {
      abort();  /* shouldn't happen */
    }
    
    /* New transposition is cumulative */
    newtrans = ((int64_t) curtrans) + ((int64_t) t);
    
    /* Range-check new transposition */
    if ((newtrans < INT32_MIN) || (newtrans > INT32_MAX)) {
      status = 0;
      *per = ERR_HUGETRANS;
    }
    
  } else {
    /* Transposition stack empty, so use new value as-is */
    newtrans = t;
  }
  
  /* Add the new transposition value to the stack */
  if (status) {
    if (!nvm_istack_push(&m_nvm_transstack, (int32_t) newtrans)) {
      status = 0;
      *per = ERR_STACKFULL;
    }
  }
  
  /* Return status */
  return status;
}

/*
 * nvm_op_poptrans function.
 */
int nvm_op_poptrans(int *per) {
  
  int status = 1;
  
  /* Check parameter */
  if (per == NULL) {
    abort();
  }
  
  /* Initialize if necessary */
  nvm_init();
  
  /* Try to pop the transposition stack */
  if (!nvm_istack_pop(&m_nvm_transstack)) {
    status = 0;
    *per = ERR_UNDERFLOW;
  }
  
  /* Return status */
  return status;
}

/*
 * nvm_op_immart function.
 */
int nvm_op_immart(int art, int *per) {
  
  int status = 1;
  
  /* Check parameters */
  if ((per == NULL) || (art < 0) || (art > NMF_MAXART)) {
    abort();
  }
  
  /* Initialize if necessary */
  nvm_init();
  
  /* Set register */
  m_nvm_immart = art;
  
  /* Return status */
  return status;
}

/*
 * nvm_op_pushart function.
 */
int nvm_op_pushart(int art, int *per) {
  
  int status = 1;
  
  /* Check parameters */
  if ((per == NULL) || (art < 0) || (art > NMF_MAXART)) {
    abort();
  }
  
  /* Initialize if necessary */
  nvm_init();
  
  /* Try to push value on stack */
  if (!nvm_istack_push(&m_nvm_artstack, (int32_t) art)) {
    status = 0;
    *per = ERR_STACKFULL;
  }
  
  /* Return status */
  return status;
}

/*
 * nvm_op_popart function.
 */
int nvm_op_popart(int *per) {
  
  int status = 1;
  
  /* Check parameter */
  if (per == NULL) {
    abort();
  }
  
  /* Initialize if necessary */
  nvm_init();
  
  /* Try to pop articulation stack */
  if (!nvm_istack_pop(&m_nvm_artstack)) {
    status = 0;
    *per = ERR_UNDERFLOW;
  }
  
  /* Return status */
  return status;
}

/*
 * nvm_op_setbase function.
 */
int nvm_op_setbase(int32_t layer, int *per) {
  
  int status = 1;
  
  /* Check parameters */
  if (per == NULL) {
    abort();
  }
  
  /* Initialize if necessary */
  nvm_init();
  
  /* Range-check layer */
  if ((layer < 1) || (layer > NOIR_MAXLAYER)) {
    status = 0;
    *per = ERR_BADLAYER;
  }
  
  /* Update layer ID of base layer register */
  if (status) {
    m_nvm_baselayer.layer_i = (uint16_t) (layer - 1);
  }
  
  /* Return status */
  return status;
}

/*
 * nvm_op_pushlayer function.
 */
int nvm_op_pushlayer(int32_t layer, int *per) {
  
  int status = 1;
  NVM_LAYERREG lr;
  
  /* Initialize structure */
  memset(&lr, 0, sizeof(NVM_LAYERREG));
  
  /* Check parameters */
  if (per == NULL) {
    abort();
  }
  
  /* Initialize if necessary */
  nvm_init();
  
  /* Range-check layer */
  if ((layer < 1) || (layer > NOIR_MAXLAYER)) {
    status = 0;
    *per = ERR_BADLAYER;
  }
  
  /* Set layer structure */
  if (status) {
    lr.sect = (uint16_t) m_nvm_sect;
    lr.layer_i = (uint16_t) (layer - 1);
  }
  
  /* Push value on stack */
  if (status) {
    if (!nvm_lstack_push(&m_nvm_layerstack, &lr)) {
      status = 0;
      *per = ERR_STACKFULL;
    }
  }
  
  /* Return status */
  return status;
}

/*
 * nvm_op_poplayer function.
 */
int nvm_op_poplayer(int *per) {
  
  int status = 1;
  
  /* Check parameter */
  if (per == NULL) {
    abort();
  }
  
  /* Initialize if necessary */
  nvm_init();
  
  /* Try to pop the layer stack */
  if (!nvm_lstack_pop(&m_nvm_layerstack)) {
    status = 0;
    *per = ERR_UNDERFLOW;
  }
  
  /* Return status */
  return status;
}

/*
 * nvm_op_cue function.
 */
int nvm_op_cue(int32_t cue_num, int *per) {
  
  int status = 1;
  
  /* Check parameters */
  if ((cue_num < 0) || (cue_num > NOIR_MAXCUE) || (per == NULL)) {
    abort();
  }
  
  /* Initialize if necessary */
  nvm_init();
  
  /* Report the cue event */
  if (!event_cue(
          m_nvm_cursor,
          m_nvm_sect,
          cue_num)) {
    status = 0;
    *per = ERR_MANYNOTES;
  }
  
  /* Return status */
  return status;
}
