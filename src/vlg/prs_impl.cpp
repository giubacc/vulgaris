/*
 * vulgaris
 * (C) 2018 - giuseppe.baccini@live.com
 *
 */

#include "prs_impl.h"

namespace vlg {

RetCode persistence_task::execute()
{
    switch(op_code_) {
        case VLG_PERS_TASK_OP_CONNECT:
            op_res_ = do_connect();
            break;
        case VLG_PERS_TASK_OP_CREATETABLE:
            op_res_ = do_create_table();
            break;
        case VLG_PERS_TASK_OP_SELECT:
            op_res_ = do_select();
            break;
        case VLG_PERS_TASK_OP_UPDATE:
            op_res_ = do_update();
            break;
        case VLG_PERS_TASK_OP_DELETE:
            op_res_ = do_delete();
            break;
        case VLG_PERS_TASK_OP_INSERT:
            op_res_ = do_insert();
            break;
        case VLG_PERS_TASK_OP_EXECUTEQUERY:
            op_res_ = do_execute_query();
            break;
        case VLG_PERS_TASK_OP_RELEASEQUERY:
            op_res_ = do_release_query();
            break;
        case VLG_PERS_TASK_OP_NEXTENTITYFROMQUERY:
            op_res_ = do_next_entity_from_query();
            break;
        case VLG_PERS_TASK_OP_EXECUTESTATEMENT:
            op_res_ = do_execute_statement();
            break;
        default:
            op_res_ = RetCode_UNSP;
            break;
    }
    return op_res_;
}

// persistence_connection_pool
// internal only

persistence_connection_pool::persistence_connection_pool(persistence_driver &driv,
                                                         const char *url,
                                                         const char *usr,
                                                         const char *psswd,
                                                         unsigned int conn_pool_sz,
                                                         unsigned int conn_pool_th_max_sz) :
    driv_(driv),
    conn_pool_sz_(conn_pool_sz),
    conn_pool_curr_idx_(0),
    conn_pool_th_max_sz_(conn_pool_th_max_sz),
    conn_pool_th_curr_sz_(0),
    conn_pool_th_curr_idx_(0),
    conn_pool_th_pool_(nullptr)
{
    url_.assign(url ? url : "");
    usr_.assign(usr ? usr : "");
    psswd_.assign(psswd ? psswd : "");
    if(!conn_pool_th_max_sz) {
        conn_pool_th_pool_ = new persistence_worker*[1];
    } else if(conn_pool_th_max_sz > 0) {
        conn_pool_th_pool_ = new persistence_worker*[conn_pool_th_max_sz];
    }
}

persistence_connection_pool::~persistence_connection_pool()
{
    if(conn_pool_th_max_sz_) {
        for(unsigned int i = 0; i < conn_pool_th_curr_sz_; i++) {
            delete conn_pool_th_pool_[i];
        }
        delete[] conn_pool_th_pool_;
    }
}

RetCode persistence_connection_pool::start()
{
    RetCode rcode = RetCode_OK;
    persistence_connection_impl *conn = nullptr;
    for(unsigned int i=0; i<conn_pool_sz_; i++) {
        if((rcode = driv_.new_connection(*this, &conn))) {
            break;
        }
        conn_idx_conn_hm_[i] = conn;
        conn->connect();
    }
    return rcode;
}

persistence_connection_impl *persistence_connection_pool::request_connection()
{
    persistence_connection_impl *conn = nullptr;
    std::unique_lock<std::mutex> lck(mtx_);
    auto it = conn_idx_conn_hm_.find(conn_pool_curr_idx_);
    conn = it->second;
    conn_pool_curr_idx_ = (conn_pool_curr_idx_+1) % conn_pool_sz_;
    return conn;
}

persistence_worker *persistence_connection_pool::get_worker_rr_can_create_start()
{
    bool surrogate_th = false;
    if(conn_pool_th_max_sz_ == 0) {
        conn_pool_th_max_sz_ = 1;
        surrogate_th = true;
    }
    persistence_worker *wrkr = nullptr;
    if(conn_pool_th_curr_sz_ < conn_pool_th_max_sz_) {
        wrkr = conn_pool_th_pool_[conn_pool_th_curr_sz_] = new persistence_worker(*this, surrogate_th);
        if(!surrogate_th) {
            wrkr->start();
        }
        conn_pool_th_curr_sz_++;
    } else {
        wrkr = conn_pool_th_pool_[conn_pool_th_curr_idx_];
        conn_pool_th_curr_idx_ = (conn_pool_th_curr_idx_ + 1) % conn_pool_th_curr_sz_;
    }
    return wrkr;
}

persistence_worker *persistence_connection_pool::get_worker_rr()
{
    if(conn_pool_th_max_sz_ == 0 || conn_pool_th_curr_sz_ == 0) {
        return nullptr;
    }
    persistence_worker *wrkr = nullptr;
    wrkr = conn_pool_th_pool_[conn_pool_th_curr_idx_];
    conn_pool_th_curr_idx_ = (conn_pool_th_curr_idx_ + 1) % conn_pool_th_curr_sz_;
    return wrkr;
}

// persistence_worker

persistence_worker::persistence_worker(persistence_connection_pool &conn_pool,
                                       bool surrogate_th) :
    conn_pool_(conn_pool),
    task_queue_(sngl_ptr_obj_mng()),
    surrogate_th_(surrogate_th)
{}

RetCode persistence_worker::submit(persistence_task &task)
{
    if(surrogate_th_) {
        task.set_execution_result(task.execute());
        task.set_status(TskStatus_EXECUTED);
        return RetCode_OK;
    }
    RetCode rcode = RetCode_OK;
    persistence_task *task_ptr = &task;
    if((rcode = task_queue_.put(&task_ptr))) {
        task.set_status(TskStatus_REJECTED);
    } else {
        task.set_status(TskStatus_SUBMITTED);
    }
    return rcode;
}

void persistence_worker::run()
{
    task *task = nullptr;
    RetCode rcode = RetCode_OK;
    do {
        if(!(rcode = task_queue_.take(&task))) {
            task->set_execution_result(task->execute());
            task->set_status(TskStatus_EXECUTED);
        } else {
            task->set_status(TskStatus_ERROR);
            break;
        }
    } while(true);
}

// persistence_connection_impl - CONNECTION

unsigned int c_cnt = 0;
persistence_connection_impl::persistence_connection_impl(persistence_connection_pool &conn_pool,
                                                         std::shared_ptr<spdlog::logger> &log) :
    id_(++c_cnt),
    status_(PersistenceConnectionStatus_DISCONNECTED),
    conn_pool_(conn_pool),
    log_(log)
{}

RetCode persistence_connection_impl::connect()
{
    if(status_ != PersistenceConnectionStatus_DISCONNECTED) {
        return RetCode_BADSTTS;
    }
    RetCode rcode = do_connect();
    if(rcode == RetCode_OK) {
        status_ = PersistenceConnectionStatus_CONNECTED;
    }
    IFLOG(log_, trace(LS_CLO "[res:{}]", __func__, rcode))
    return rcode;
}

RetCode persistence_connection_impl::create_entity_schema(PersistenceAlteringMode mode,
                                                          const nentity_manager &nem,
                                                          unsigned int nclass_id)
{
    IFLOG(log_, trace(LS_OPN "[mode:{}, nclass_id:{}]", __func__, mode, nclass_id))
    RetCode rcode = RetCode_OK;
    const nentity_desc *edesc = nem.get_nentity_descriptor(nclass_id);
    if(!edesc || !edesc->is_persistent()) {
        return RetCode_BADARG;
    }
    switch(mode) {
        case PersistenceAlteringMode_CREATE_ONLY:
            rcode = do_create_table(nem, *edesc, false);
            break;
        case PersistenceAlteringMode_DROP_IF_EXIST:
            rcode = do_create_table(nem, *edesc, true);
            break;
        case PersistenceAlteringMode_CREATE_OR_UPDATE:
            rcode = RetCode_UNSP;
            break;
        default:
            rcode = RetCode_BADARG;
    }
    return rcode;
}

RetCode persistence_connection_impl::create_entity_schema(PersistenceAlteringMode mode,
                                                          const nentity_manager &nem,
                                                          const nentity_desc &edesc)
{
    IFLOG(log_, trace(LS_OPN "[mode:{}]", __func__, mode))
    RetCode rcode = RetCode_OK;
    if(!edesc.is_persistent()) {
        return RetCode_BADARG;
    }
    switch(mode) {
        case PersistenceAlteringMode_CREATE_ONLY:
            rcode = do_create_table(nem, edesc, false);
            break;
        case PersistenceAlteringMode_DROP_IF_EXIST:
            rcode = do_create_table(nem, edesc, true);
            break;
        case PersistenceAlteringMode_CREATE_OR_UPDATE:
            rcode = RetCode_UNSP;
            break;
        default:
            rcode = RetCode_BADARG;
    }
    return rcode;
}

RetCode persistence_connection_impl::load_entity(unsigned short key,
                                                 const nentity_manager &nem,
                                                 unsigned int &ts0_out,
                                                 unsigned int &ts1_out,
                                                 nclass &in_out)
{
    RetCode rcode = RetCode_OK;
    const nentity_desc &edesc = in_out.get_nentity_descriptor();
    if(!edesc.is_persistent()) {
        return RetCode_BADARG;
    }
    rcode = do_select(key, nem, ts0_out, ts1_out, in_out);
    return rcode;
}

RetCode persistence_connection_impl::save_entity(const nentity_manager
                                                 &nem,
                                                 unsigned int ts_0,
                                                 unsigned int ts_1,
                                                 const nclass &in)
{
    RetCode rcode = RetCode_OK;
    const nentity_desc &edesc = in.get_nentity_descriptor();
    if(!edesc.is_persistent()) {
        return RetCode_BADARG;
    }
    rcode = do_insert(nem, ts_0, ts_1, in);
    return rcode;
}

RetCode persistence_connection_impl::update_entity(unsigned short key,
                                                   const nentity_manager &nem,
                                                   unsigned int ts_0,
                                                   unsigned int ts_1,
                                                   const nclass &in)
{
    RetCode rcode = RetCode_OK;
    const nentity_desc &edesc = in.get_nentity_descriptor();
    if(!edesc.is_persistent()) {
        return RetCode_BADARG;
    }
    rcode = do_update(key, nem, ts_0, ts_1, in);
    return rcode;
}

RetCode persistence_connection_impl::save_or_update_entity(unsigned short key,
                                                           const nentity_manager &nem,
                                                           unsigned int ts_0,
                                                           unsigned int ts_1,
                                                           const nclass &in)
{
    RetCode rcode = RetCode_OK;
    const nentity_desc &edesc = in.get_nentity_descriptor();
    if(!edesc.is_persistent()) {
        return RetCode_BADARG;
    }
    rcode = do_insert(nem, ts_0, ts_1, in, false);
    if(rcode) {
        rcode = do_update(key, nem, ts_0, ts_1, in);
    }
    return rcode;
}

RetCode persistence_connection_impl::remove_entity(unsigned short key,
                                                   const nentity_manager &nem,
                                                   unsigned int ts_0,
                                                   unsigned int ts_1,
                                                   PersistenceDeletionMode mode,
                                                   const nclass &in)
{
    RetCode rcode = RetCode_OK;
    const nentity_desc &edesc = in.get_nentity_descriptor();
    if(!edesc.is_persistent()) {
        return RetCode_BADARG;
    }
    rcode = do_delete(key, nem, ts_0, ts_1, mode, in);
    return rcode;
}

RetCode persistence_connection_impl::execute_query(const char *sql,
                                                   const nentity_manager &nem,
                                                   std::unique_ptr<persistence_query_impl> &query_out)
{
    return do_execute_query(nem, sql, query_out);
}

RetCode persistence_connection_impl::destroy_query(persistence_query_impl *query,
                                                   bool release_before_destroy)
{
    RetCode rcode = RetCode_OK;
    if(query) {
        if(&query->conn_ == this) {
            if(release_before_destroy) {
                query->release();
            }
            delete query;
        } else {
            rcode = RetCode_BADARG;
        }
    } else {
        rcode = RetCode_BADARG;
    }
    return rcode;
}

RetCode persistence_connection_impl::execute_statement(const char *sql)
{
    return do_execute_statement(sql);
}

// persistence_query_impl

persistence_query_impl::persistence_query_impl(unsigned int id,
                                               persistence_connection_impl &conn,
                                               const nentity_manager &nem,
                                               std::shared_ptr<spdlog::logger> &log) :
    id_(id),
    status_(PersistenceQueryStatus_PREPARED),
    conn_(conn),
    nem_(nem),
    log_(log)
{}

RetCode persistence_query_impl::load_next_entity(unsigned int &ts0_out,
                                                 unsigned int &ts1_out,
                                                 nclass &out)
{
    const nentity_desc &edesc = out.get_nentity_descriptor();
    if(!edesc.is_persistent()) {
        return RetCode_BADARG;
    }
    return conn_.do_next_entity_from_query(*this, ts0_out, ts1_out, out);
}

RetCode persistence_query_impl::release()
{
    return conn_.do_release_query(*this);
}

// persistence_driver_impl - DRIVER

RetCode persistence_driver::load_driver_dyna(const char *drvname,
                                             persistence_driver **driver,
                                             std::shared_ptr<spdlog::logger> &log)
{
    if(!drvname || !strlen(drvname)) {
        return RetCode_BADARG;
    }
#if defined WIN32 && defined _MSC_VER
    wchar_t w_drvname[VLG_DRV_NAME_LEN] = {0};
    swprintf(w_drvname, VLG_DRV_NAME_LEN, L"drv%hs", drvname);
    void *dynalib = dynamic_lib_open(w_drvname);
#endif
#ifdef __linux
    char slib_name[VLG_DRV_NAME_LEN] = {0};
    sprintf(slib_name, "libdrv%s.so", drvname);
    void *dynalib = dynamic_lib_open(slib_name);
#endif
#if defined (__MACH__) || defined (__APPLE__)
    char slib_name[VLG_DRV_NAME_LEN] = {0};
    sprintf(slib_name, "libdrv%s.dylib", drvname);
    void *dynalib = dynamic_lib_open(slib_name);
#endif
    if(!dynalib) {
        IFLOG(log, error(LS_CLO "[failed loading so-lib for driver:{}]", __func__, drvname))
        return RetCode_KO;
    }
    char dri_ep_f[VLG_DRV_NAME_LEN] = {0};
    sprintf(dri_ep_f, "get_pers_driv_%s", drvname);
    persistence_driver_load dri_f = (persistence_driver_load)dynamic_lib_load_symbol(dynalib, dri_ep_f);
    if(!dri_f) {
        IFLOG(log, error(LS_CLO "[failed to locate entrypoint in so-lib for driver:{}]", __func__, drvname))
        return RetCode_KO;
    }
    if(!(*driver = dri_f((logger *)&log))) {
        IFLOG(log, error(LS_CLO "[failed to get driver instance for driver:{}]", __func__, drvname))
        return RetCode_KO;
    } else {
        char driv_f_n[VLG_MDL_NAME_LEN] = {0};
        sprintf(driv_f_n, "get_pers_driv_ver_%s", drvname);
        persistence_driver_version_get driv_f = (persistence_driver_version_get) dynamic_lib_load_symbol(dynalib, driv_f_n);
        IFLOG(log, info(LS_DRV"driver:{} [loaded]", driv_f()))
    }
    return RetCode_OK;
}

persistence_driver::persistence_driver(unsigned int id,
                                       std::shared_ptr<spdlog::logger> &log) : id_(id), log_(log) {}

RetCode persistence_driver::start_all_pools()
{
    RetCode rcode = RetCode_OK;
    for(auto it = conn_pool_hm_.begin(); it != conn_pool_hm_.end(); it++) {
        if((rcode = it->second->start())) {
            IFLOG(log_, critical(LS_TRL "[failed to start conn_pool_name:{}]", __func__, it->first.c_str()))
            break;
        }
    }
    IFLOG(log_, trace(LS_CLO "[res:{}]", __func__, rcode))
    return rcode;
}

RetCode persistence_driver::add_pool(const char *conn_pool_name,
                                     const char *url,
                                     const char *usr,
                                     const char *psswd,
                                     unsigned int conn_pool_sz,
                                     unsigned int conn_pool_th_max_sz)
{
    IFLOG(log_, trace(LS_OPN"[conn_pool_name:{}, url:{}, usr:{}, conn_pool_sz:{}, thread_sz:{}]",
                      __func__,
                      conn_pool_name,
                      url,
                      usr,
                      conn_pool_sz,
                      conn_pool_th_max_sz))
    persistence_connection_pool *conn_pool = new persistence_connection_pool(*this,
                                                                             url,
                                                                             usr,
                                                                             psswd,
                                                                             conn_pool_sz,
                                                                             conn_pool_th_max_sz);
    conn_pool_hm_[conn_pool_name] = conn_pool;
    return RetCode_OK;
}

RetCode persistence_driver::map_nclassid_to_pool(unsigned int nclass_id,
                                                 const char *conn_pool_name)
{
    IFLOG(log_, trace(LS_OPN "[nclass_id:{} -> conn_pool:{}]", __func__, nclass_id, conn_pool_name))
    auto it = conn_pool_hm_.find(conn_pool_name);
    if(it != conn_pool_hm_.end()) {
        nclassid_conn_pool_hm_[nclass_id] = it->second;
    } else {
        IFLOG(log_, critical(LS_TRL "[conn_pool_name:{} is not defined]", __func__, conn_pool_name))
    }
    return RetCode_OK;
}

persistence_connection_impl *persistence_driver::available_connection(unsigned int nclass_id)
{
    auto conn_pool_it = nclassid_conn_pool_hm_.find(nclass_id);
    persistence_connection_impl *conn_out = nullptr;
    if(conn_pool_it != nclassid_conn_pool_hm_.end()) {
        conn_out = conn_pool_it->second->request_connection();
    } else {
        IFLOG(log_, error(LS_TRL "[nclass_id:{} has no connection-pool available]", __func__, nclass_id))
    }
    return conn_out;
}

}

