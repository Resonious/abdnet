#include "net.h"

#ifndef ABD_RPCDSL_H
#define ABD_RPCDSL_H

#define RPC_ARG(argtype, argname)\
    argtype##_t argname;\
    if (_rpcinfo.rw == ABD_WRITE) {\
        argname = (argtype##_t)va_arg(_va, argtype##_vt);\
    }\
    if (_rpcinfo.target){\
        abd_transfer(_rpcinfo.rw, argtype, &_rpcinfo.target->rpc_buf, &argname, NULL);\
    }

#ifndef RA
#define RA RPC_ARG
#endif
#ifndef A
#define A RPC_ARG
#endif

#define SET_RPC(rpc_list, rpc_func, index) { int _i = index; (rpc_list)[_i] = rpc_func; rpcid_##rpc_func = _i; }

#define DEF_RPC(funcname, argdefs)                                                \
    uint16_t rpcid_##funcname;                                                    \
                                                                                  \
    void funcname(RpcInfo _rpcinfo, ...) {                                        \
        va_list _va = NULL;                                                       \
                                                                                  \
        if (_rpcinfo.target && _rpcinfo.rw == ABD_WRITE) {                        \
            uint16_t rpc_id = rpcid_##funcname;                                             \
            abd_transfer(ABD_WRITE, ABDT_U16, &_rpcinfo.target->rpc_buf, &rpc_id, NULL);  \
            _rpcinfo.target->rpc_count += 1;                                      \
        }                                                                         \
                                                                                  \
        if (_rpcinfo.rw == ABD_WRITE) va_start(_va, _rpcinfo);                    \
        argdefs;                                                                  \
        if (_rpcinfo.rw == ABD_WRITE) va_end(_va);                                \
        AbdConnection* connection = _rpcinfo.con;                                 \
        if (_rpcinfo.flags & RPCF_EXECUTE_LOCALLY) {

#define END_RPC }}

#define DEFINE_RPC DEF_RPC

#define CALL_ON_CLIENT_ID(server, client_id) (RpcInfo){ &(server)->clients[(client_id)].outgoing_rpc, ABD_WRITE, 0, AS_CONNECTION(server) }
#define CALL_ON_CLIENT(joined_client)        (RpcInfo){ &(joined_client)->outgoing_rpc, ABD_WRITE, 0, AS_CONNECTION((joined_client)->server) }
#define CALL_ON_ALL_CLIENTS(server)          (RpcInfo){ &(server)->outgoing_rpc, ABD_WRITE, 0, AS_CONNECTION(server) }
#define CALL_EVERYWHERE(server)              (RpcInfo){ &(server)->outgoing_rpc, ABD_WRITE, RPCF_EXECUTE_LOCALLY, AS_CONNECTION(server) }
#define CALL_LOCALLY(connection)             (RpcInfo){ NULL, ABD_WRITE, RPCF_EXECUTE_LOCALLY, AS_CONNECTION(connection) }
#define CALL_ON_SERVER(client)               (RpcInfo){ &(client)->outgoing_rpc, ABD_WRITE, 0, AS_CONNECTION(client) }

#endif
