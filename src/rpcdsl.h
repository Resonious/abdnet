#include "net.h"

#ifndef ABD_RPCDSL_H
#define ABD_RPCDSL_H

#define abd_defarg_val(argtype, argname) argtype##_t argname
#define abd_defarg_ptr(argtype, argname) argtype##_t _##argname##_space; argtype##_t* argname = &_##argname##_space

#define abd_defarg_ABDT_FLOAT(argtype, argname)  abd_defarg_val(argtype, argname)
#define abd_defarg_ABDT_VEC2(argtype, argname)   abd_defarg_ptr(argtype, argname)
#define abd_defarg_ABDT_VEC4(argtype, argname)   abd_defarg_ptr(argtype, argname)
#define abd_defarg_ABDT_S16(argtype, argname)    abd_defarg_val(argtype, argname)
#define abd_defarg_ABDT_S32(argtype, argname)    abd_defarg_val(argtype, argname)
#define abd_defarg_ABDT_S64(argtype, argname)    abd_defarg_val(argtype, argname)
#define abd_defarg_ABDT_U16(argtype, argname)    abd_defarg_val(argtype, argname)
#define abd_defarg_ABDT_U32(argtype, argname)    abd_defarg_val(argtype, argname)
#define abd_defarg_ABDT_U64(argtype, argname)    abd_defarg_val(argtype, argname)
#define abd_defarg_ABDT_STRING(argtype, argname) char _##argname##_space[256]; char* argname = _##argname##_space

#define va_arg_int_type(va, arg) arg = va_arg(va, int)

#define va_arg_ABDT_FLOAT(va, arg)  arg = (float)va_arg(va, double)
#define va_arg_ABDT_VEC2(va, arg)   arg = va_arg(va, ABDT_VEC2_t*)
#define va_arg_ABDT_VEC4(va, arg)   arg = va_arg(va, ABDT_VEC4_t*)
#define va_arg_ABDT_S16(va, arg)    va_arg_int_type(va, arg)
#define va_arg_ABDT_S32(va, arg)    va_arg_int_type(va, arg)
#define va_arg_ABDT_S64(va, arg)    va_arg_int_type(va, arg)
#define va_arg_ABDT_U16(va, arg)    va_arg_int_type(va, arg)
#define va_arg_ABDT_U32(va, arg)    va_arg_int_type(va, arg)
#define va_arg_ABDT_U64(va, arg)    va_arg_int_type(va, arg)
#define va_arg_ABDT_STRING(va, arg) arg = va_arg(va, char*)

#define abd_ptr_ABDT_FLOAT(arg)  (&(arg))
#define abd_ptr_ABDT_VEC2(arg)   (arg)
#define abd_ptr_ABDT_VEC4(arg)   (arg)
#define abd_ptr_ABDT_S16(arg)    (&(arg))
#define abd_ptr_ABDT_S32(arg)    (&(arg))
#define abd_ptr_ABDT_S64(arg)    (&(arg))
#define abd_ptr_ABDT_U16(arg)    (&(arg))
#define abd_ptr_ABDT_U32(arg)    (&(arg))
#define abd_ptr_ABDT_U64(arg)    (&(arg))
#define abd_ptr_ABDT_STRING(arg) (arg)

#define RPC_ARG(argtype, argname)\
    abd_defarg_##argtype(argtype, argname);\
    if (_rpcinfo.rw == ABD_WRITE) {\
        va_arg_##argtype(_va, argname);\
    }\
    if (_rpcinfo.target){\
        abd_transfer(_rpcinfo.rw, argtype, &_rpcinfo.target->rpc_buf, abd_ptr_##argtype(argname), NULL);\
    }

#ifndef RA
#define RA RPC_ARG
#endif
#ifndef A
#define A RPC_ARG
#endif

#define SET_RPC(rpc_list, rpc_func, index) { int _i = index; (rpc_list)[_i] = rpc_func; rpcid_##rpc_func = _i; }
#define SET_CORE_RPC(rpc_list, rpc_func, index) { int _i = index; (rpc_list)[_i] = rpc_func; rpcid_##rpc_func = -_i; }

#define DEF_RPC(funcname, argdefs)                                                          \
    uint16_t rpcid_##funcname;                                                              \
                                                                                            \
    void funcname(RpcInfo _rpcinfo, ...) {                                                  \
        va_list _va = NULL;                                                                 \
                                                                                            \
        if (_rpcinfo.target && _rpcinfo.rw == ABD_WRITE) {                                  \
            uint16_t rpc_id = rpcid_##funcname;                                             \
            abd_transfer(ABD_WRITE, ABDT_U16, &_rpcinfo.target->rpc_buf, &rpc_id, NULL);    \
            _rpcinfo.target->rpc_count += 1;                                                \
        }                                                                                   \
                                                                                            \
        if (_rpcinfo.rw == ABD_WRITE) va_start(_va, _rpcinfo);                              \
        argdefs;                                                                            \
        if (_rpcinfo.rw == ABD_WRITE) va_end(_va);                                          \
        AbdConnection* connection = _rpcinfo.con;                                           \
        AbdConnection* sender     = _rpcinfo.from;                                          \
        if (_rpcinfo.flags & RPCF_EXECUTE_LOCALLY) {

#define END_RPC }}

#define DEFINE_RPC DEF_RPC

#define CALL_ON_CLIENT_ID(server, client_id) (RpcInfo){ &(server)->clients[client_id].outgoing_rpc, ABD_WRITE, 0, AS_CONNECTION(server), NULL }
#define CALL_ON_CLIENT(joined_client)        (RpcInfo){ &(joined_client)->outgoing_rpc, ABD_WRITE, 0, AS_CONNECTION((joined_client)->server), NULL }
#define CALL_ON_ALL_CLIENTS(server)          (RpcInfo){ &(server)->outgoing_rpc, ABD_WRITE, 0, AS_CONNECTION(server), NULL }
#define CALL_EVERYWHERE(server)              (RpcInfo){ &(server)->outgoing_rpc, ABD_WRITE, RPCF_EXECUTE_LOCALLY, AS_CONNECTION(server), NULL }
#define CALL_LOCALLY(connection)             (RpcInfo){ NULL, ABD_WRITE, RPCF_EXECUTE_LOCALLY, AS_CONNECTION(connection), NULL }
#define CALL_ON_SERVER(client)               (RpcInfo){ &(client)->outgoing_rpc, ABD_WRITE, 0, AS_CONNECTION(client), NULL }

#endif
