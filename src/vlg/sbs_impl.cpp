/*
* vulgaris
* (C) 2018 - giuseppe.baccini@live.com
*
*/

#include "brk_impl.h"
#include "conn_impl.h"
#include "sbs_impl.h"

namespace vlg {

const std_shared_ptr_obj_mng<incoming_subscription> sbs_std_shp_omng;

}

namespace vlg {

//subscription_event_impl

subscription_event_impl::subscription_event_impl(unsigned int sbsid,
                                                 unsigned int evtid,
                                                 SubscriptionEventType set,
                                                 ProtocolCode pc,
                                                 unsigned int ts_0,
                                                 unsigned int ts_1,
                                                 Action act,
                                                 const nclass &data) :
    sbs_sbsid_(sbsid),
    sbs_evtid_(evtid),
    sbs_evttype_(set),
    sbs_protocode_(pc),
    sbs_tmstp0_(ts_0),
    sbs_tmstp1_(ts_1),
    sbs_act_(act),
    sbs_data_(data.clone())
{}

subscription_event_impl::subscription_event_impl(unsigned int sbsid,
                                                 unsigned int evtid,
                                                 SubscriptionEventType set,
                                                 ProtocolCode pc,
                                                 unsigned int ts_0,
                                                 unsigned int ts_1,
                                                 Action act,
                                                 std::unique_ptr<nclass> &data) :
    sbs_sbsid_(sbsid),
    sbs_evtid_(evtid),
    sbs_evttype_(set),
    sbs_protocode_(pc),
    sbs_tmstp0_(ts_0),
    sbs_tmstp1_(ts_1),
    sbs_act_(act),
    sbs_data_(std::move(data))
{}

sbs_impl::sbs_impl(incoming_subscription &publ,
                   incoming_connection &conn,
                   incoming_subscription_listener &listener) :
    conn_(conn.impl_.get()),
    sbsid_(0),
    reqid_(0),
    status_(SubscriptionStatus_INITIALIZED),
    start_stop_evt_occur_(false),
    sbstyp_(SubscriptionType_UNDEFINED),
    sbsmod_(SubscriptionMode_UNDEFINED),
    flotyp_(SubscriptionFlowType_UNDEFINED),
    dwltyp_(SubscriptionDownloadType_UNDEFINED),
    enctyp_(Encode_UNDEFINED),
    nclassid_(0),
    open_tmstp0_(0),
    open_tmstp1_(0),
    sbresl_(SubscriptionResponse_UNDEFINED),
    last_vlgcod_(ProtocolCode_SUCCESS),
    ipubl_(&publ),
    opubl_(nullptr),
    ilistener_(&listener),
    olistener_(nullptr)
{}

sbs_impl::sbs_impl(outgoing_subscription &publ,
                   outgoing_subscription_listener &listener) :
    conn_(nullptr),
    sbsid_(0),
    reqid_(0),
    status_(SubscriptionStatus_INITIALIZED),
    start_stop_evt_occur_(false),
    sbstyp_(SubscriptionType_UNDEFINED),
    sbsmod_(SubscriptionMode_UNDEFINED),
    flotyp_(SubscriptionFlowType_UNDEFINED),
    dwltyp_(SubscriptionDownloadType_UNDEFINED),
    enctyp_(Encode_UNDEFINED),
    nclassid_(0),
    open_tmstp0_(0),
    open_tmstp1_(0),
    sbresl_(SubscriptionResponse_UNDEFINED),
    last_vlgcod_(ProtocolCode_SUCCESS),
    ipubl_(nullptr),
    opubl_(&publ),
    ilistener_(nullptr),
    olistener_(&listener)
{}

inline void sbs_impl::ntfy_sel_snd_pkt()
{
    sel_evt *evt = new sel_evt(VLG_SELECTOR_Evt_SendPacket, conn_);
    conn_->broker_->selector_.notify(evt);
}

RetCode sbs_impl::set_started()
{
    IFLOG(conn_->broker_->log_, info(LS_SBS"[CONNID:{}-SBSID:{}][started]", conn_->connid_, sbsid_))
    set_status(SubscriptionStatus_STARTED);
    if(opubl_) {
        olistener_->on_start(*opubl_);
    }
    return RetCode_OK;
}

RetCode sbs_impl::set_stopped()
{
    IFLOG(conn_->broker_->log_, info(LS_SBS"[CONNID:{}-SBSID:{}][stopped]", conn_->connid_, sbsid_))
    set_status(SubscriptionStatus_STOPPED);
    if(ipubl_) {
        ilistener_->on_stop(*ipubl_);
    } else {
        olistener_->on_stop(*opubl_);
    }
    return RetCode_OK;
}

RetCode sbs_impl::set_released()
{
    IFLOG(conn_->broker_->log_, debug(LS_SBS"[CONNID:{}-SBSID:{}][released]", conn_->connid_, sbsid_))
    set_status(SubscriptionStatus_RELEASED);
    return RetCode_OK;
}

inline RetCode sbs_impl::set_status(SubscriptionStatus status)
{
    std::unique_lock<std::mutex> lck(mtx_);
    status_ = status;
    if(ipubl_) {
        ilistener_->on_status_change(*ipubl_, status_);
    } else {
        olistener_->on_status_change(*opubl_, status_);
    }
    cv_.notify_all();
    return RetCode_OK;
}

RetCode sbs_impl::await_for_status_reached(SubscriptionStatus test,
                                           SubscriptionStatus &current,
                                           time_t sec,
                                           long nsec)
{
    RetCode rcode = RetCode_OK;
    std::unique_lock<std::mutex> lck(mtx_);
    if(status_ < SubscriptionStatus_INITIALIZED) {
        return RetCode_BADSTTS;
    }
    if(sec<0) {
        cv_.wait(lck,[&]() {
            return status_ >= test;
        });
    } else {
        rcode = cv_.wait_for(lck,std::chrono::seconds(sec) + std::chrono::nanoseconds(nsec),[&]() {
            return status_ >= test;
        }) ? RetCode_OK : RetCode_TIMEOUT;
    }
    current = status_;
    IFLOG(conn_->broker_->log_, trace(LS_CLO "test:{} [{}] current:{}", __func__, test, !rcode ? "reached" : "timeout", status_))
    return rcode;
}

RetCode sbs_impl::await_for_start_result(SubscriptionResponse &sbs_start_result,
                                         ProtocolCode &sbs_start_protocode,
                                         time_t sec,
                                         long nsec)
{
    RetCode rcode = RetCode_OK;
    std::unique_lock<std::mutex> lck(mtx_);
    if(status_ < SubscriptionStatus_INITIALIZED) {
        return RetCode_BADSTTS;
    }
    if(sec<0) {
        cv_.wait(lck,[&]() {
            return start_stop_evt_occur_;
        });
    } else {
        rcode = cv_.wait_for(lck,std::chrono::seconds(sec) + std::chrono::nanoseconds(nsec),[&]() {
            return start_stop_evt_occur_;
        }) ? RetCode_OK : RetCode_TIMEOUT;
    }
    sbs_start_result = sbresl_;
    sbs_start_protocode = last_vlgcod_;
    IFLOG(conn_->broker_->log_, trace(LS_CLO
                                      "[sbsid:{}, res:{}, status:{}][incoming_subscription start result available][sbs_start_result:{}, sbs_start_protocode:{}]",
                                      __func__,
                                      sbsid_,
                                      rcode,
                                      status_,
                                      sbresl_,
                                      last_vlgcod_))
    start_stop_evt_occur_ = false;
    return rcode;
}

RetCode sbs_impl::await_for_stop_result(SubscriptionResponse &sbs_stop_result,
                                        ProtocolCode &sbs_stop_protocode,
                                        time_t sec,
                                        long nsec)
{
    RetCode rcode = RetCode_OK;
    std::unique_lock<std::mutex> lck(mtx_);
    if(status_ < SubscriptionStatus_INITIALIZED) {
        return RetCode_BADSTTS;
    }
    if(sec<0) {
        cv_.wait(lck,[&]() {
            return start_stop_evt_occur_;
        });
    } else {
        rcode = cv_.wait_for(lck,std::chrono::seconds(sec) + std::chrono::nanoseconds(nsec),[&]() {
            return start_stop_evt_occur_;
        }) ? RetCode_OK : RetCode_TIMEOUT;
    }
    sbs_stop_result = sbresl_;
    sbs_stop_protocode = last_vlgcod_;
    IFLOG(conn_->broker_->log_, trace(LS_CLO
                                      "[sbsid:{}, res:{}, status:{}][incoming_subscription stop result available][sbs_stop_result:{}, sbs_stop_protocode:{}]",
                                      __func__,
                                      sbsid_,
                                      rcode,
                                      status_,
                                      sbresl_,
                                      last_vlgcod_))
    start_stop_evt_occur_ = false;
    return rcode;
}

RetCode sbs_impl::notify_for_start_stop_result()
{
    std::unique_lock<std::mutex> lck(mtx_);
    start_stop_evt_occur_ = true;
    cv_.notify_all();
    return RetCode_OK;
}

RetCode sbs_impl::stop()
{
    if(status_ != SubscriptionStatus_STARTED) {
        return RetCode_BADSTTS;
    }
    start_stop_evt_occur_ = false;
    g_bbuf gbb;
    build_PKT_SBSTOP(sbsid_, &gbb);
    std::unique_ptr<conn_pkt> cpkt(new conn_pkt(nullptr, std::move(gbb)));
    conn_->pkt_sending_q_.put(&cpkt);
    ntfy_sel_snd_pkt();
    return RetCode_OK;
}

//incoming_subscription_impl

incoming_subscription_impl::incoming_subscription_impl(incoming_subscription &publ,
                                                       std::shared_ptr<incoming_connection> &conn,
                                                       incoming_subscription_listener &listener) :
    sbs_impl(publ, *conn, listener),
    conn_sh_(conn),
    initial_query_(nullptr),
    initial_query_ended_(true)
{}

incoming_subscription_impl::~incoming_subscription_impl()
{
    release_initial_query();
}

inline void incoming_subscription_impl::ntfy_sel_snd_pkt()
{
    sel_evt *evt = new sel_evt(VLG_SELECTOR_Evt_SendPacket, conn_sh_);
    conn_->broker_->selector_.notify(evt);
}

inline void incoming_subscription_impl::release_initial_query()
{
    if(!initial_query_ended_) {
        if(initial_query_) {
            initial_query_->release();
        }
        initial_query_ = nullptr;
        initial_query_ended_ = true;
    }
}

RetCode incoming_subscription_impl::send_start_response()
{
    g_bbuf gbb;
    build_PKT_SBSRES(sbresl_,
                     last_vlgcod_,
                     reqid_,
                     sbsid_,
                     &gbb);
    std::unique_ptr<conn_pkt> cpkt(new conn_pkt(nullptr, std::move(gbb)));
    conn_->pkt_sending_q_.put(&cpkt);
    ntfy_sel_snd_pkt();
    return RetCode_OK;
}

void incoming_subscription_impl::enq_event(std::shared_ptr<subscription_event> &sbs_evt)
{
    g_bbuf gbb;
    build_PKT_SBSEVT(sbsid_,
                     sbs_evt->impl_->sbs_evttype_,
                     sbs_evt->impl_->sbs_act_,
                     sbs_evt->impl_->sbs_protocode_,
                     sbs_evt->impl_->sbs_evtid_,
                     sbs_evt->impl_->sbs_tmstp0_,
                     sbs_evt->impl_->sbs_tmstp1_,
                     &gbb);

    long totbytes = sbs_evt->impl_->sbs_data_ ?
                    sbs_evt->impl_->sbs_data_->serialize(enctyp_, nullptr, &gbb) :
                    (long)gbb.pos_;

    std::unique_ptr<char> key;
    totbytes = htonl(totbytes);
    if(sbs_evt->impl_->sbs_data_) {
        gbb.put(&totbytes, (6*4), 4);
        sbs_evt->impl_->sbs_data_.get()->get_primary_key_value_as_string(key);
    }
    std::unique_ptr<conn_pkt> cpkt(new conn_pkt(&key,
                                                std::move(gbb),
                                                sbs_evt->impl_->sbs_tmstp0_,
                                                sbs_evt->impl_->sbs_tmstp1_));

    if(flotyp_ == SubscriptionFlowType_LAST) {
        conn_->pkt_sending_q_.put_or_update(&cpkt);
    } else {
        conn_->pkt_sending_q_.put(&cpkt);
    }

    if(sbs_evt->impl_->sbs_evttype_ == SubscriptionEventType_LIVE) {
        ntfy_sel_snd_pkt();
    }
}

RetCode incoming_subscription_impl::send_initial_query()
{
    RetCode rcode = RetCode_OK;
    per_nclass_id_conn_set *sdr = nullptr;
    subscription_event_impl *sbs_dwnl_evt_impl = nullptr;
    std::unique_ptr<nclass> dwnl_obj;
    unsigned int ts_0 = 0, ts_1 = 0;
    if((rcode = conn_->broker_->get_per_nclassid_helper_rec(nclassid_, &sdr))) {
        IFLOG(conn_->broker_->log_, critical(LS_CLO "[failed get per-nclass_id helper class][res:{}]", __func__, rcode))
        return rcode;
    }

    do {
        conn_->broker_->nem_.new_nclass_instance(nclassid_, dwnl_obj);
        if((rcode = initial_query_->load_next_entity(ts_0, ts_1, *dwnl_obj)) == RetCode_DBROW) {
            sbs_dwnl_evt_impl = new subscription_event_impl(sbsid_,
                                                            sdr->next_sbs_evt_id(),
                                                            SubscriptionEventType_DOWNLOAD,
                                                            ProtocolCode_SUCCESS,
                                                            ts_0,
                                                            ts_1,
                                                            Action_INSERT,
                                                            dwnl_obj);
            std::shared_ptr<subscription_event> sbs_evt(new subscription_event(*sbs_dwnl_evt_impl));
            submit_evt(sbs_evt);
        } else {
            break;
        }
    } while(true);
    ntfy_sel_snd_pkt();

    if((rcode = initial_query_->release())) {
        IFLOG(conn_->broker_->log_, critical(LS_TRL "[download query failed to release][res:{}]", __func__, rcode))
    }

    initial_query_.release();
    initial_query_ended_ = true;
    return rcode;
}

RetCode incoming_subscription_impl::execute_initial_query()
{
    RetCode rcode = RetCode_OK;
    const nentity_desc *nclass_desc = conn_->broker_->nem_.get_nentity_descriptor(nclassid_);
    if(nclass_desc) {
        if(nclass_desc->is_persistent()) {
            persistence_driver *driv = nullptr;
            if((driv = conn_->broker_->pers_mng_.available_driver(nclassid_))) {
                persistence_connection_impl *conn = nullptr;
                if((conn = driv->available_connection(nclassid_))) {
                    std::stringstream ss;
                    ss << "select * from " << nclass_desc->get_nentity_name();
                    if(dwltyp_ == SubscriptionDownloadType_PARTIAL) {
                        ss << " where (" P_F_TS0" = "
                           << open_tmstp0_
                           << " and " P_F_TS1" > "
                           << open_tmstp1_
                           << ") or (" P_F_TS0" > "
                           << open_tmstp0_
                           << ")";
                    }
                    ss << ";";
                    rcode = conn->execute_query(ss.str().c_str(), conn_->broker_->nem_, initial_query_);
                } else {
                    IFLOG(conn_->broker_->log_, error(LS_TRL "[no available persistence-connection for nclass_id:{}]", __func__,
                                                      nclassid_))
                    rcode = RetCode_KO;
                }
            } else {
                IFLOG(conn_->broker_->log_, error(LS_TRL "[no available persistence-driver for nclass_id:{}]", __func__, nclassid_))
                rcode = RetCode_KO;
            }
        } else {
            IFLOG(conn_->broker_->log_, error(LS_TRL "[nclass is not persistent][nclass_id:{}]", __func__, nclassid_))
            rcode = RetCode_KO;
        }
    } else {
        IFLOG(conn_->broker_->log_, critical(LS_TRL "[nclass descriptor not found][nclass_id:{}]", __func__, nclassid_))
    }
    if(rcode) {
        initial_query_ended_ = true;
        if(initial_query_) {
            initial_query_->release();
        }
        initial_query_.release();
        IFLOG(conn_->broker_->log_, error(LS_CLO "[sbsid:{} - download query failed][res:{}].", __func__, sbsid_, rcode))
    }
    return rcode;
}

}

namespace vlg {

//outgoing_subscription_impl

outgoing_subscription_impl::outgoing_subscription_impl(outgoing_subscription &publ,
                                                       outgoing_subscription_listener &listener) :
    sbs_impl(publ, listener)
{}

outgoing_subscription_impl::~outgoing_subscription_impl()
{
    if(status_ == SubscriptionStatus_REQUEST_SENT ||
            status_ == SubscriptionStatus_STARTED) {
        IFLOG(conn_->broker_->log_, warn(LS_DTR
                                         "[subscription:{} in status:{}, stopping..]",
                                         __func__,
                                         sbsid_,
                                         status_))
        stop();
        SubscriptionResponse sres = SubscriptionResponse_UNDEFINED;
        ProtocolCode spc = ProtocolCode_UNDEFINED;
        await_for_stop_result(sres, spc);
    }

    /*BUG?*/
    if(conn_) {
        static_cast<outgoing_connection_impl *>(conn_)->release_subscription(this);
    }
}

RetCode outgoing_subscription_impl::set_req_sent()
{
    IFLOG(conn_->broker_->log_, info(
              LS_SBO"[CONNID:{}-REQID:{}][SBSTYP:{}, SBSMOD:{}, FLOTYP:{}, DWLTYP:{}, ENCTYP:{}, NCLSSID:{}, TMSTP0:{}, TMSTP1:{}]",
              conn_->connid_,
              reqid_,
              sbstyp_,
              sbsmod_,
              flotyp_,
              dwltyp_,
              enctyp_,
              nclassid_,
              open_tmstp0_,
              open_tmstp1_))
    set_status(SubscriptionStatus_REQUEST_SENT);
    return RetCode_OK;
}

RetCode outgoing_subscription_impl::start()
{
    if(status_ != SubscriptionStatus_INITIALIZED && status_ != SubscriptionStatus_STOPPED) {
        IFLOG(conn_->broker_->log_, error(LS_CLO "[status:{}]", __func__, status_))
        return RetCode_BADSTTS;
    }
    RetCode rcode = RetCode_OK;
    auto &oconn = *static_cast<outgoing_connection_impl *>(conn_);
    reqid_ = oconn.next_reqid();
    outgoing_subscription_impl *self = this;
    rcode = oconn.outg_reqid_sbs_map_.put(&reqid_, &self);
    if((rcode = send_start_request())) {
        IFLOG(conn_->broker_->log_, error(LS_TRL "[send request failed][res:{}]", __func__, rcode))
    }
    return rcode;
}

RetCode outgoing_subscription_impl::start(SubscriptionType sbscr_type,
                                          SubscriptionMode sbscr_mode,
                                          SubscriptionFlowType sbscr_flow_type,
                                          SubscriptionDownloadType  sbscr_dwnld_type,
                                          Encode sbscr_nclass_encode,
                                          unsigned int nclass_id,
                                          unsigned int start_timestamp_0,
                                          unsigned int start_timestamp_1)
{
    IFLOG(conn_->broker_->log_, trace(
              LS_OPN"[sbsid:{}, sbscr_type:{}, sbscr_mode:{}, sbscr_flow_type:{}, sbscr_dwnld_type:{}, sbscr_nclass_encode:{}, nclass_id:{}, ts_0:{}, ts_1:{}]",
              __func__,
              sbsid_,
              sbscr_type,
              sbscr_mode,
              sbscr_flow_type,
              sbscr_dwnld_type,
              sbscr_nclass_encode,
              nclass_id,
              start_timestamp_0,
              start_timestamp_1))
    if(status_ != SubscriptionStatus_INITIALIZED && status_ != SubscriptionStatus_STOPPED) {
        IFLOG(conn_->broker_->log_, error(LS_CLO "[status:{}]", __func__, status_))
        return RetCode_BADSTTS;
    }
    sbstyp_ = sbscr_type;
    sbsmod_ = sbscr_mode;
    flotyp_ = sbscr_flow_type;
    dwltyp_ = sbscr_dwnld_type;
    enctyp_ = sbscr_nclass_encode;
    nclassid_ = nclass_id;
    open_tmstp0_ = start_timestamp_0;
    open_tmstp1_ = start_timestamp_1;
    return start();
}

RetCode outgoing_subscription_impl::send_start_request()
{
    set_req_sent();
    start_stop_evt_occur_ = false;

    g_bbuf gbb;
    build_PKT_SBSREQ(sbstyp_,
                     sbsmod_,
                     flotyp_,
                     dwltyp_,
                     enctyp_,
                     nclassid_,
                     conn_->connid_,
                     reqid_,
                     open_tmstp0_,
                     open_tmstp1_,
                     &gbb);
    std::unique_ptr<conn_pkt> cpkt(new conn_pkt(nullptr, std::move(gbb)));
    conn_->pkt_sending_q_.put(&cpkt);
    ntfy_sel_snd_pkt();
    return RetCode_OK;
}

RetCode outgoing_subscription_impl::receive_event(const vlg_hdr_rec *pkt_hdr,
                                                  g_bbuf *pkt_body)
{
    std::unique_ptr<nclass> nobj;
    if(pkt_hdr->row_2.sevttp.sevttp != SubscriptionEventType_DOWNLOAD_END) {
        RetCode rcode = RetCode_OK;
        if((rcode = conn_->broker_->nem_.new_nclass_instance(nclassid_, nobj))) {
            IFLOG(conn_->broker_->log_, critical(
                      LS_SBS"[incoming_subscription event receive failed: new_nclass_instance fail:{}, nclass_id:{}]",
                      rcode,
                      nclassid_))
            return rcode;
        }
        if((rcode = nobj->restore(&conn_->broker_->nem_, enctyp_, pkt_body))) {
            IFLOG(conn_->broker_->log_, critical(
                      LS_SBS"[incoming_subscription event receive failed: nclass restore fail:{}, nclass_id:{}]",
                      rcode,
                      nclassid_))
            return rcode;
        } else {
            IFLOG(conn_->broker_->log_, debug(LS_SBI"[ACT:{}]{}", pkt_hdr->row_2.sevttp.sbeact, spdlog_nclass_type{*nobj.get()}))
        }
    }

    std::unique_ptr<subscription_event> sbs_evt(new subscription_event(*new subscription_event_impl(
                                                                           pkt_hdr->row_1.sbsrid.sbsrid,
                                                                           pkt_hdr->row_3.sevtid.sevtid,
                                                                           pkt_hdr->row_2.sevttp.sevttp,
                                                                           pkt_hdr->row_2.sevttp.vlgcod,
                                                                           pkt_hdr->row_4.tmstmp.tmstmp,
                                                                           pkt_hdr->row_5.tmstmp.tmstmp,
                                                                           pkt_hdr->row_2.sevttp.sbeact,
                                                                           nobj)));
    olistener_->on_incoming_event(*opubl_, sbs_evt);
    return RetCode_OK;
}

}
