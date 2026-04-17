#include <mxu2.h>
#include <stdio.h>
extern v4i32 g_state;
extern v4i32 add_global(v4i32 x);

int main(void) {
    v4i32 x = (v4i32){10, 20, 30, 40};
    v4i32 r = add_global(x);
    /* g_state was {1,2,3,4}, x was {10,20,30,40}, r should be {11,22,33,44} */
    if (r[0]==11 && r[1]==22 && r[2]==33 && r[3]==44) puts("LTO OK");
    else { printf("LTO FAIL %d %d %d %d\n", r[0],r[1],r[2],r[3]); return 1; }
    return 0;
}
