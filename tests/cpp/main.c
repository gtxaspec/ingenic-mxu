#include <stdio.h>
extern int run_cpp_smoke(void);
int main(void) {
    int rc = run_cpp_smoke();
    if (rc == 0) puts("CPP_SMOKE OK");
    else printf("CPP_SMOKE FAIL %d\n", rc);
    return rc;
}
