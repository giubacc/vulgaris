/*
 * vulgaris
 * (C) 2018 - giuseppe.baccini@live.com
 *
 */

#include "brk_impl.h"
#include "conn_impl.h"
#include "sbs_impl.h"

#define VLG_INT_AWT_TIMEOUT 1

namespace vlg {

// incoming_connection_factory

incoming_connection_factory *default_conn_factory = nullptr;
incoming_connection_factory &incoming_connection_factory::default_factory()
{
    if(default_conn_factory == nullptr) {
        default_conn_factory = new incoming_connection_factory();
    }
    return *default_conn_factory;
}

incoming_connection_factory::incoming_connection_factory()
{}

incoming_connection_factory::~incoming_connection_factory()
{}

incoming_connection &incoming_connection_factory::make_incoming_connection(broker &p)
{
    return *new incoming_connection(p);
}

}

namespace vlg {

// broker_impl

broker_impl::broker_impl(broker &publ, broker_listener &listener) :
    broker_automa(publ, listener),
    personality_(BrokerPersonality_BOTH),
    srv_exectrs_(0),
    cli_exectrs_(0),
    srv_sbs_exectrs_(0),
    selector_(*this),
    prgr_conn_id_(0),
    nem_((vlg::logger *)&log_),
    pers_enabled_(false),
    pers_mng_(persistence_manager_impl::get_instance()),
    pers_schema_create_(false),
    drop_existing_schema_(false),
    inco_exec_srv_(log_),
    outg_exec_srv_(log_),
    srv_sbs_exec_serv_(log_),
    srv_sbs_nclassid_condesc_set_(HMSz_1031, sngl_ptr_obj_mng(), sizeof(unsigned int)),
    inco_conn_factory_(nullptr)
{
    early_init();
}

RetCode broker_impl::set_params_file_dir(const char *dir)
{
    return broker_conf_ldr_.set_params_file_dir(dir);
}

RetCode broker_impl::init()
{
    RetCode rcode = RetCode_OK;
    RET_ON_KO(selector_.init())
    RET_ON_KO(inco_exec_srv_.init(srv_exectrs_))
    RET_ON_KO(outg_exec_srv_.init(cli_exectrs_))
    RET_ON_KO(srv_sbs_exec_serv_.init(srv_sbs_exectrs_))
#ifdef __APPLE__
#include "TargetConditionals.h"
#if TARGET_IPHONE_SIMULATOR
    //dynamic linking not allowed
#elif TARGET_OS_IPHONE
    //dynamic linking not allowed
#elif TARGET_OS_MAC
    rcode = init_dyna();
#else
#endif
#else
    rcode = init_dyna();
#endif
    if(pers_enabled_) {
        IFLOG(log_, debug(LS_TRL "[loading persistence-configuration]", __func__))
        if((rcode = pers_mng_.load_cfg())) {
            if(rcode == RetCode_IOERR) {
                IFLOG(log_, warn(LS_TRL"[no persistence-configuration file available, proceeding anyway]", __func__))
                rcode = RetCode_OK;
            } else {
                IFLOG(log_, critical(LS_TRL"[critical error:{} loading persistence-configuration]", __func__, rcode))
            }
        } else {
            IFLOG(log_, info(LS_TRL "[persistence configuration loaded]", __func__))
        }
    }
    IFLOG(log_, trace(LS_CLO "[res:{}]", __func__, rcode))
    return rcode;
}

RetCode broker_impl::init_dyna()
{
    RetCode rcode = RetCode_OK;
    IFLOG(log_, debug(LS_TRL "[extending model]", __func__))
    if(model_map_.size() > 0) {
        for(auto it = model_map_.begin(); it != model_map_.end(); it++) {
            RET_ON_KO(extend_model(it->c_str()))
        }
        IFLOG(log_, info(LS_TRL "[model extended]", __func__))
    }
    if(pers_enabled_ && pers_dri_load_.size() > 0) {
        IFLOG(log_, debug(LS_TRL "[loading persistence drivers]", __func__))
        RET_ON_KO(persistence_manager_impl::load_pers_driver_dyna(pers_dri_load_))
        IFLOG(log_, info(LS_TRL "[persistence drivers loaded]", __func__))
    }
    IFLOG(log_, trace(LS_CLO "[res:{}]", __func__, rcode))
    return rcode;
}

RetCode broker_impl::start_exec_services()
{
    RetCode res = RetCode_OK;
    ExecSrvStatus current = ExecSrvStatus_TOINIT;
    if(personality_ == BrokerPersonality_PURE_SERVER || personality_ == BrokerPersonality_BOTH) {
        IFLOG(log_,debug(LS_TRL "[starting server side executor service]",__func__))
        if((res = inco_exec_srv_.start())) {
            IFLOG(log_,critical(LS_CLO "[starting server side, last_err:{}]",__func__,res))
            return RetCode_KO;
        }
        inco_exec_srv_.await_for_status_reached(ExecSrvStatus_STARTED,current);
        IFLOG(log_,debug(LS_TRL "[server side executor service started]",__func__))
    }
    if(personality_ == BrokerPersonality_PURE_CLIENT || personality_ == BrokerPersonality_BOTH) {
        IFLOG(log_,debug(LS_TRL "[starting client side executor service]",__func__))
        if((res = outg_exec_srv_.start())) {
            IFLOG(log_,critical(LS_CLO "[starting client side, last_err:{}]",__func__,res))
            return RetCode_KO;
        }
        outg_exec_srv_.await_for_status_reached(ExecSrvStatus_STARTED,current);
        IFLOG(log_,debug(LS_TRL "[client side executor service started]",__func__))
    }
    return res;
}

// CONFIG SETTERS

void broker_impl::set_cfg_load_model(const char *model)
{
    if(model_map_.find(model) != model_map_.end()) {
        IFLOG(log_, warn(LS_PAR "[model already specified:{}]", model))
    } else {
        model_map_.insert(model);
    }
}

void broker_impl::set_cfg_srv_sin_addr(const char *addr)
{
    selector_.srv_sockaddr_in_.sin_addr.s_addr = inet_addr(addr);
}

void broker_impl::set_cfg_srv_sin_port(int port)
{
    selector_.srv_sockaddr_in_.sin_port = htons(port);
}

void broker_impl::set_cfg_load_pers_driv(const char *driv)
{
    if(pers_dri_load_.find(driv) != pers_dri_load_.end()) {
        IFLOG(log_, warn(LS_PAR "[persistent driver already specified:{}]", driv))
    } else {
        pers_dri_load_.insert(driv);
    }
}

RetCode broker_impl::extend_model(nentity_manager &nem)
{
    RetCode rcode = RetCode_OK;
    if((rcode = nem_.extend(nem))) {
        IFLOG(log_, critical(LS_MDL "[failed to extend nem][res:{}]", __func__, rcode))
    }
    IFLOG(log_, trace(LS_CLO "[res:{}]", __func__, rcode))
    return rcode;
}

RetCode broker_impl::extend_model(const char *model_name)
{
    IFLOG(log_, trace(LS_OPN "[model:{}]", __func__, model_name))
    RetCode rcode = RetCode_OK;
    if((rcode = nem_.extend(model_name))) {
        IFLOG(log_, critical(LS_MDL "[failed to extend nem][model:{}, res:{}]", __func__, model_name, rcode))
    }
    IFLOG(log_, trace(LS_CLO "[res:{}]", __func__, rcode))
    return rcode;
}

RetCode broker_impl::new_incoming_connection(std::shared_ptr<incoming_connection> &new_connection, unsigned int connid)
{
    incoming_connection &publ = inco_conn_factory_->make_incoming_connection(publ_);
    new_connection.reset(&publ);
    return RetCode_OK;
}

incoming_connection_factory &broker_impl::get_incoming_connection_factory() const
{
    return *inco_conn_factory_;
}

void broker_impl::set_incoming_connection_factory(incoming_connection_factory &val)
{
    inco_conn_factory_ = &val;
}

RetCode broker_impl::on_automa_load_config(int pnum,
                                           const char *param,
                                           const char *value)
{
    if(!strcmp(param, "pure_server")) {
        if(personality_ != BrokerPersonality_BOTH) {
            IFLOG(log_, error(LS_PAR"[pure_server] check params"))
        }
        personality_ = BrokerPersonality_PURE_SERVER;
    }
    if(!strcmp(param, "pure_client")) {
        if(personality_ != BrokerPersonality_BOTH) {
            IFLOG(log_, error(LS_PAR"[pure_client] check params"))
        }
        personality_ = BrokerPersonality_PURE_CLIENT;
    }
    if(!strcmp(param, "load_model")) {
        if(value) {
            if(model_map_.find(value) != model_map_.end()) {
                IFLOG(log_, error(LS_PAR"[load_model] model already specified:{}", value))
                return RetCode_BADCFG;
            } else {
                model_map_.insert(value);
            }
        } else {
            IFLOG(log_, error(LS_PAR"[load_model] requires argument"))
            return RetCode_BADCFG;
        }
    }
    if(!strcmp(param, "srv_sin_addr")) {
        if(value) {
            selector_.srv_sockaddr_in_.sin_addr.s_addr = inet_addr(value);
        } else {
            IFLOG(log_, error(LS_PAR"[srv_sin_addr] requires argument"))
            return RetCode_BADCFG;
        }
    }
    if(!strcmp(param, "srv_sin_port")) {
        if(value) {
            selector_.srv_sockaddr_in_.sin_port = htons(atoi(value));
        } else {
            IFLOG(log_, error(LS_PAR"[srv_sin_port] requires argument"))
            return RetCode_BADCFG;
        }
    }
    if(!strcmp(param, "srv_exectrs")) {
        if(value) {
            srv_exectrs_ = atoi(value);
        } else {
            IFLOG(log_, error(LS_PAR"[srv_exectrs] requires argument"))
            return RetCode_BADCFG;
        }
    }
    if(!strcmp(param, "cli_exectrs")) {
        if(value) {
            cli_exectrs_ = atoi(value);
        } else {
            IFLOG(log_, error(LS_PAR"[cli_exectrs] requires argument"))
            return RetCode_BADCFG;
        }
    }
    if(!strcmp(param, "srv_sbs_exectrs")) {
        if(value) {
            srv_sbs_exectrs_ = atoi(value);
        } else {
            IFLOG(log_, error(LS_PAR"[srv_sbs_exectrs] requires argument"))
            return RetCode_BADCFG;
        }
    }
    if(!strcmp(param, "pers_enabled")) {
        pers_enabled_ = true;
    }
    if(!strcmp(param, "pers_schema_create")) {
        pers_schema_create_ = true;
    }
    if(!strcmp(param, "drop_existing_schema")) {
        drop_existing_schema_ = true;
    }
    if(!strcmp(param, "load_pers_driv")) {
        if(value) {
            if(pers_dri_load_.find(value) != pers_dri_load_.end()) {
                IFLOG(log_, warn(LS_PAR"[load_pers_driv] driver already specified:{}, skipping", value))
                return RetCode_OK;
            } else {
                pers_dri_load_.insert(value);
            }
        } else {
            IFLOG(log_, error(LS_PAR"[load_pers_driv] requires argument"))
            return RetCode_BADCFG;
        }
    }
    return listener_.on_load_config(publ_, pnum, param, value);
}

RetCode broker_impl::on_automa_early_init()
{
    rt_init_timers();
#if defined WIN32 && defined _MSC_VER
    RET_ON_KO(WSA_init(log_))
#endif
    return RetCode_OK;
}

const char *broker_impl::get_automa_name()
{
    return publ_.get_name();
}

const unsigned int *broker_impl::get_automa_version()
{
    return publ_.get_version();
}

RetCode broker_impl::on_automa_init()
{
    RetCode rcode = init();
    if(!rcode) {
        listener_.on_init(publ_);
    }
    return rcode;
}

RetCode broker_impl::on_automa_start()
{
    SelectorStatus current = SelectorStatus_UNDEF;
    RetCode rcode = RetCode_OK;
    IFLOG(log_, info(LS_APL"[broker personality: << {} >>]",
                     (personality_ == BrokerPersonality_BOTH) ? "both" :
                     (personality_ == BrokerPersonality_PURE_SERVER) ? "pure-server" : "pure-client"))
    if(personality_ == BrokerPersonality_PURE_SERVER || personality_ == BrokerPersonality_BOTH) {
        ExecSrvStatus current_exc_srv = ExecSrvStatus_TOINIT;
        if((rcode = srv_sbs_exec_serv_.start())) {
            IFLOG(log_, critical(LS_CLO "[starting subscription executor service][res:{}]", __func__, rcode))
            return rcode;
        }
        srv_sbs_exec_serv_.await_for_status_reached(ExecSrvStatus_STARTED, current_exc_srv);
    }
    //persistence driv. begin
    if(pers_enabled_) {
        IFLOG(log_, debug(LS_TRL "[starting all persistence drivers]", __func__))
        RET_ON_KO(pers_mng_.start_all_drivers())
        IFLOG(log_, info(LS_TRL "[persistence drivers started]", __func__))
        if(pers_schema_create_) {
            RetCode db_res = RetCode_OK;
            IFLOG(log_, debug(LS_TRL "[creating persistence schema]", __func__))
            db_res = create_persistent_schema(drop_existing_schema_ ?
                                              PersistenceAlteringMode_DROP_IF_EXIST :
                                              PersistenceAlteringMode_CREATE_ONLY);
            if(db_res) {
                if(db_res != RetCode_DBOPFAIL) {
                    IFLOG(log_, critical(LS_TRL "[error:{} creating persistence schema]", __func__, db_res))
                    return db_res;
                }
            }
            IFLOG(log_, info(LS_TRL "[persistence schema created]", __func__))
        }
    }
    if((rcode = start_exec_services())) {
        IFLOG(log_,error(LS_CLO "[starting executor services][res:{}]",__func__,rcode))
        return rcode;
    }
    IFLOG(log_, debug(LS_TRL "[start selector thread]", __func__))
    selector_.start();
    IFLOG(log_, debug(LS_TRL "[wait selector go init]", __func__))
    selector_.await_for_status_reached(SelectorStatus_INIT,
                                       current,
                                       VLG_INT_AWT_TIMEOUT,
                                       0);
    if(!rcode) {
        rcode = listener_.on_starting(publ_);
    }
    IFLOG(log_, trace(LS_CLO "[res:{}]", __func__, rcode))
    return rcode;
}

RetCode broker_impl::on_automa_move_running()
{
    SelectorStatus current = SelectorStatus_UNDEF;
    RetCode rcode = RetCode_OK;
    if((rcode = selector_.on_broker_move_running_actions())) {
        IFLOG(log_, error(LS_CLO "[selector failed running actions][res:{}]", __func__, rcode))
        return rcode;
    }
    IFLOG(log_, debug(LS_TRL "[request selector go ready]", __func__))
    selector_.set_status(SelectorStatus_REQUEST_READY);
    IFLOG(log_, debug(LS_TRL "[wait selector go ready]", __func__))
    selector_.await_for_status_reached(SelectorStatus_READY,
                                       current,
                                       VLG_INT_AWT_TIMEOUT,
                                       0);
    IFLOG(log_, debug(LS_TRL "[request selector go selecting]", __func__))
    selector_.set_status(SelectorStatus_REQUEST_SELECT);
    if(!rcode) {
        rcode = listener_.on_move_running(publ_);
    }
    return rcode;
}

RetCode broker_impl::on_automa_stop()
{
    RetCode rcode = RetCode_OK;
    if(!selector_.inco_conn_map_.empty() || !selector_.outg_conn_map_.empty()) {
        if(!force_disconnect_on_stop_) {
            IFLOG(log_, error(LS_CLO "[active connections detected, cannot stop broker]", __func__))
            return RetCode_KO;
        }
    }
    IFLOG(log_, debug(LS_TRL "[request selector to stop]", __func__))
    selector_.set_status(SelectorStatus_REQUEST_STOP);
    selector_.interrupt();
    SelectorStatus current = SelectorStatus_UNDEF;
    selector_.await_for_status_reached(SelectorStatus_STOPPED, current);
    IFLOG(log_, debug(LS_TRL "[selector stopped]", __func__))
    selector_.set_status(SelectorStatus_INIT);
    inco_exec_srv_.shutdown();
    inco_exec_srv_.await_termination();
    outg_exec_srv_.shutdown();
    outg_exec_srv_.await_termination();
    srv_sbs_exec_serv_.shutdown();
    srv_sbs_exec_serv_.await_termination();
    if(!rcode) {
        rcode = listener_.on_stopping(publ_);
    }
    IFLOG(log_, trace(LS_CLO "[res:{}]", __func__, rcode))
    return rcode;
}

void broker_impl::on_automa_error()
{
    listener_.on_error(publ_);
}

// per_nclass_id_conn_set

per_nclass_id_conn_set::per_nclass_id_conn_set() :
    sbsevtid_(0),
    ts0_(0),
    ts1_(0),
    connid_condesc_set_(HMSz_1031, conn_std_shp_omng, sizeof(unsigned int))
{}

void per_nclass_id_conn_set::next_time_stamp(unsigned int &ts_0,
                                             unsigned int &ts_1)
{
    std::unique_lock<std::mutex> lck(mtx_);
    ts_0 = (unsigned int)time(nullptr);
    if(ts_0 == ts0_) {
        ts_1 = ++ts1_;
    } else {
        ts0_ = ts_0;
        ts_1 = ts1_ = 0;
    }
}

RetCode broker_impl::add_subscriber(incoming_subscription_impl *sbsdesc)
{
    per_nclass_id_conn_set *sdrec = nullptr;
    if(srv_sbs_nclassid_condesc_set_.get(&sbsdesc->nclassid_, &sdrec)) {
        sdrec = new per_nclass_id_conn_set();
        srv_sbs_nclassid_condesc_set_.put(&sbsdesc->nclassid_, &sdrec);
    }
    sdrec->connid_condesc_set_.put(&sbsdesc->conn_->connid_, &sbsdesc->conn_sh_);
    IFLOG(log_, debug(LS_SBS"[added subscriber: connid:{}, nclass_id:{}]",
                      sbsdesc->conn_->connid_,
                      sbsdesc->nclassid_))
    return RetCode_OK;
}

RetCode broker_impl::remove_subscriber(incoming_subscription_impl *sbsdesc)
{
    per_nclass_id_conn_set *sdrec = nullptr;
    if(srv_sbs_nclassid_condesc_set_.get(&sbsdesc->nclassid_, &sdrec)) {
        IFLOG(log_, critical(LS_CLO"[subscriber: connid:{} not found for nclass_id:{}]",
                             __func__,
                             sbsdesc->conn_->connid_,
                             sbsdesc->nclassid_))
        return RetCode_GENERR;
    }
    if(sdrec->connid_condesc_set_.remove(&sbsdesc->conn_->connid_, nullptr)) {
        IFLOG(log_, critical(LS_CLO"[subscriber: connid:{} not found for nclass_id:{}]",
                             __func__,
                             sbsdesc->conn_->connid_,
                             sbsdesc->nclassid_))
        return RetCode_GENERR;
    }
    return RetCode_OK;
}

struct broker_sbs_task;

struct srv_connid_condesc_set_ashsnd_rud {
    unsigned int nclass_id;
    broker_sbs_task *tsk;
    RetCode rcode;
};

struct broker_sbs_task : public task {
    broker_sbs_task(broker_impl &broker,
                    subscription_event_impl &sbs_evt,
                    s_hm &connid_condesc_set) :
        broker_(broker),
        sbs_evt_(new subscription_event(sbs_evt)),
        connid_condesc_set_(connid_condesc_set) {}

    ~broker_sbs_task() {}

    static void enum_srv_connid_connection_map_ashsnd(const s_hm &map,
                                                      const void *key,
                                                      void *ptr,
                                                      void *ud) {
        srv_connid_condesc_set_ashsnd_rud *rud = static_cast<srv_connid_condesc_set_ashsnd_rud *>(ud);
        std::shared_ptr<incoming_connection> *conn = (std::shared_ptr<incoming_connection> *)ptr;
        std::shared_ptr<incoming_subscription> sbs_sh;
        if((*conn)->impl_->inco_nclassid_sbs_map_.get(&rud->nclass_id, &sbs_sh)) {
            IFLOG((*conn)->impl_->broker_->log_, warn(LS_EXE "[no more active subscriptions on connection:{}]", __func__,
                                                      (*conn)->get_id()))
            return;
        }
        sbs_sh->impl_->submit_evt(rud->tsk->sbs_evt_);
    }

    virtual RetCode execute() override {
        srv_connid_condesc_set_ashsnd_rud rud;
        rud.nclass_id = sbs_evt_->get_data()->get_id();
        rud.tsk = this;
        rud.rcode = RetCode_OK;
        connid_condesc_set_.enum_elements_safe_read(enum_srv_connid_connection_map_ashsnd, &rud);
        return RetCode_OK;
    }

    broker_impl &get_broker() {
        return broker_;
    }

    broker_impl &broker_;
    std::shared_ptr<subscription_event> sbs_evt_;
    s_hm &connid_condesc_set_;  //connid --> condesc [uint --> sh_ptr]
};

RetCode broker_impl::get_per_nclassid_helper_rec(unsigned int nclass_id, per_nclass_id_conn_set **out)
{
    RetCode rcode = RetCode_OK;
    per_nclass_id_conn_set *sdrec = nullptr;
    if((rcode = srv_sbs_nclassid_condesc_set_.get(&nclass_id, &sdrec))) {
        sdrec = new per_nclass_id_conn_set();
        rcode = srv_sbs_nclassid_condesc_set_.put(&nclass_id, &sdrec);
    }
    *out = sdrec;
    return rcode;
}

RetCode broker_impl::submit_inco_evt_task(std::shared_ptr<incoming_connection> &conn_sh,
                                          vlg_hdr_rec &pkt_hdr,
                                          std::unique_ptr<g_bbuf> &pkt_body)
{
    std::shared_ptr<task> task(new inco_conn_task(conn_sh,
                                                  pkt_hdr,
                                                  pkt_body));
    return inco_exec_srv_.submit(task);
}

RetCode broker_impl::submit_outg_evt_task(outgoing_connection_impl *oconn,
                                          vlg_hdr_rec &pkt_hdr,
                                          std::unique_ptr<g_bbuf> &pkt_body)
{
    std::shared_ptr<task> task(new outg_conn_task(*oconn->opubl_,
                                                  pkt_hdr,
                                                  pkt_body));
    return outg_exec_srv_.submit(task);
}

RetCode broker_impl::submit_sbs_evt_task(subscription_event_impl &sbs_evt,
                                         s_hm &connid_condesc_set)
{
    std::shared_ptr<task> stsk(new broker_sbs_task(*this, sbs_evt, connid_condesc_set));
    return srv_sbs_exec_serv_.submit(stsk);
}

// #VER#

const char *BROKERLIB_VER(void)
{
    static char str[] = "lib.vlg.ver.0.0.0.date:" __DATE__;
    return str;
}

}

