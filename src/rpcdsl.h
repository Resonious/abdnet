#include "net.h"

#ifndef ABD_RPCDSL_H
#define ABD_RPCDSL_H

#define RPC_ARG(argtype, argname)\
    argtype##_t argname;\
    if (_rpctarget.rw == ABD_WRITE) {\
        argname = va_arg(_va, argtype##_t);\
    }\
    if (_rpctarget.buf){\
        abd_transfer(_rpctarget.rw, argtype, _rpctarget.buf, &argname, NULL);\
    }

#ifndef RA
#define RA RPC_ARG
#endif
#ifndef A
#define A RPC_ARG
#endif

#define SET_RPC(rpc_list, rpc_func, index) { int _i = index; (rpc_list)[_i] = rpc_func; rpcid_##rpc_func = _i; }

#define DEF_RPC(funcname, argdefs)\
    int rpcid_##funcname;\
\
    void funcname(RpcTarget _rpctarget, ...) {\
        va_list _va = NULL;\
\
        if (_rpctarget.buf) {\
            int rpc_id = rpcid_##funcname;\
            abd_transfer(_rpctarget.rw, ABDT_U16, _rpctarget.buf, &rpc_id, NULL);\
            abd_assert(rpc_id == rpcid_##funcname);\
        }\
\
        if (_rpctarget.rw == ABD_WRITE) va_start(_va, _rpctarget);\
        argdefs;\
        if (_rpctarget.rw == ABD_WRITE) va_end(_va);\
        if (_rpctarget.buf == NULL || _rpctarget.rw == ABD_READ) {

#define END_RPC }}

// TODO define these.
#define ON_CLIENT(n) (RpcTarget){NULL, 0}
#define ON_ALL_CLIENTS (RpcTarget){NULL, 0}
#define LOCALLY (RpcTarget){NULL, 0}
#define EVERYWHERE (RpcTarget){NULL, 0}

#endif
