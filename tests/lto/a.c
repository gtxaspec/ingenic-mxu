/* TU 'a': defines a global vector and a function that uses it.
   Tested standalone and with -flto.  */
#include <mxu2.h>
#include <stdint.h>

v4i32 g_state = (v4i32){1, 2, 3, 4};

v4i32 add_global(v4i32 x) {
    return __builtin_mxu2_add_w(x, g_state);
}
