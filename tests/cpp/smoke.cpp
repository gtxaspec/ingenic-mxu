/* Layer F: C++ smoke. Templates with vector members, exceptions
   across vector live, virtual functions returning vectors. */
#include <mxu2.h>
#include <stdint.h>

/* --- template with vector member --- */
template<typename T>
struct VecBox {
    T data;
    VecBox(T x) : data(x) {}
    T add(const VecBox &o) const { return data + o.data; }
};

/* --- virtual function returning vector --- */
struct Base {
    virtual v4i32 op(v4i32 a, v4i32 b) const = 0;
    virtual ~Base() {}
};
struct Adder : Base {
    v4i32 op(v4i32 a, v4i32 b) const override {
        return __builtin_mxu2_add_w(a, b);
    }
};

/* --- exception across vector live --- */
__attribute__((noinline))
v4i32 may_throw(v4i32 a, int x) {
    v4i32 r = __builtin_mxu2_add_w(a, a);
    if (x == 42) throw 1;
    return __builtin_mxu2_sub_w(r, a);
}

/* --- vector default-arg + reference param --- */
v4i32 with_ref(v4i32 a, const v4i32 &b) {
    return __builtin_mxu2_add_w(a, b);
}

extern "C" int run_cpp_smoke(void) {
    int fail = 0;
    {
        VecBox<v4i32> a((v4i32){1,2,3,4});
        VecBox<v4i32> b((v4i32){10,20,30,40});
        v4i32 r = a.add(b);
        if (r[0]!=11 || r[3]!=44) fail++;
    }
    {
        Adder ad;
        Base *p = &ad;
        v4i32 r = p->op((v4i32){1,2,3,4}, (v4i32){5,6,7,8});
        if (r[0]!=6 || r[3]!=12) fail++;
    }
    {
        try { (void)may_throw((v4i32){1,2,3,4}, 42); fail++; }
        catch (int) { /* ok */ }
        v4i32 r = may_throw((v4i32){1,2,3,4}, 0);
        /* r = (a+a) - a = a; check a[0] = 1 */
        if (r[0] != 1) fail++;
    }
    {
        v4i32 a = {1,2,3,4}, b = {10,20,30,40};
        v4i32 r = with_ref(a, b);
        if (r[0]!=11) fail++;
    }
    return fail;
}
