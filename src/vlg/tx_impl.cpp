/*
 * vulgaris
 * (C) 2018 - giuseppe.baccini@live.com
 *
 */

#include "brk_impl.h"
#include "conn_impl.h"
#include "tx_impl.h"

#define TX_RES_COMMT    "COMMITTED"
#define TX_RES_FAIL     "FAILED"
#define TX_RES_ABORTED  "ABORTED"
#define TX_NO_OBJ       "NO-OBJ"

namespace vlg {
const std_shared_ptr_obj_mng<incoming_transaction> tx_std_shp_omng;
}

namespace vlg {

tx_impl::tx_impl(incoming_transaction &publ,
                 incoming_connection &conn,
                 incoming_transaction_listener &listener) :
    conn_(conn.impl_.get()),
    status_(TransactionStatus_INITIALIZED),
    tx_res_(TransactionResult_UNDEFINED),
    result_code_(ProtocolCode_SUCCESS),
    txtype_(TransactionRequestType_UNDEFINED),
    txactn_(Action_NONE),
    req_nclassid_(0),
    req_clsenc_(Encode_UNDEFINED),
    res_nclassid_(0),
    res_clsenc_(Encode_UNDEFINED),
    rsclrq_(false),
    rescls_(false),
    start_mark_tim_(0),
    ipubl_(&publ),
    opubl_(nullptr),
    ilistener_(&listener),
    olistener_(nullptr)
{}


tx_impl::tx_impl(outgoing_transaction &publ,
                 outgoing_transaction_listener &listener) :
    conn_(nullptr),
    status_(TransactionStatus_INITIALIZED),
    tx_res_(TransactionResult_UNDEFINED),
    result_code_(ProtocolCode_SUCCESS),
    txtype_(TransactionRequestType_UNDEFINED),
    txactn_(Action_NONE),
    req_nclassid_(0),
    req_clsenc_(Encode_INDEXED_NOT_ZERO),
    res_nclassid_(0),
    res_clsenc_(Encode_UNDEFINED),
    rsclrq_(false),
    rescls_(false),
    start_mark_tim_(0),
    ipubl_(nullptr),
    opubl_(&publ),
    ilistener_(nullptr),
    olistener_(&listener)
{}

RetCode tx_impl::await_for_status_reached(TransactionStatus test,
                                          TransactionStatus &current,
                                          time_t sec,
                                          long nsec)
{
    RetCode rcode = RetCode_OK;
    std::unique_lock<std::mutex> lck(mtx_);
    if(status_ < TransactionStatus_INITIALIZED) {
        IFLOG(conn_->broker_->log_, error(LS_CLO, __func__))
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

RetCode tx_impl::await_for_closure(time_t sec, long nsec)
{
    RetCode rcode = RetCode_OK;
    std::unique_lock<std::mutex> lck(mtx_);
    if(status_ < TransactionStatus_INITIALIZED) {
        IFLOG(conn_->broker_->log_, error(LS_CLO, __func__))
        return RetCode_BADSTTS;
    }
    if(sec<0) {
        cv_.wait(lck,[&]() {
            return status_ >= TransactionStatus_CLOSED;
        });
    } else {
        rcode = cv_.wait_for(lck,std::chrono::seconds(sec) + std::chrono::nanoseconds(nsec),[&]() {
            return status_ >= TransactionStatus_CLOSED;
        }) ? RetCode_OK : RetCode_TIMEOUT;
    }
    IFLOG(conn_->broker_->log_, trace(LS_CLO "target:{} [{}]", __func__, TransactionStatus_CLOSED, !rcode ? "closed" : "timeout", status_))
    return rcode;
}

void tx_impl::set_request_obj(const nclass &val)
{
    request_obj_ = val.clone();
    req_nclassid_ = request_obj_->get_id();
}

void tx_impl::set_current_obj(const nclass &val)
{
    current_obj_ = val.clone();
    req_nclassid_ = current_obj_->get_id();
}

void tx_impl::set_result_obj(const nclass &val)
{
    result_obj_ = val.clone();
    res_nclassid_ = result_obj_->get_id();
    rescls_ = true;
}

RetCode tx_impl::set_flying()
{
    if(status_ != TransactionStatus_INITIALIZED) {
        return RetCode_BADSTTS;
    }
    set_status(TransactionStatus_FLYING);
    return RetCode_OK;
}

inline void tx_impl::trace_tx_closure(const char *tx_res_str)
{
    std::stringstream ss;
    rt_time_t fin_mark_tim, dt_mark_tim;
    rt_mark_time(&fin_mark_tim);
    dt_mark_tim = rt_diff_time(start_mark_tim_, fin_mark_tim);
    ss << std::setw(14) << dt_mark_tim;
    if(ipubl_) {
        if(request_obj_) {
            IFLOG(conn_->broker_->log_, info(LS_TXI"[{:08x}{:08x}{:08x}{:08x}]{}",
                                             txid_.txplid,
                                             txid_.txsvid,
                                             txid_.txcnid,
                                             txid_.txprid,
                                             spdlog_nclass_type{*request_obj_.get()}))
        } else {
            IFLOG(conn_->broker_->log_, info(LS_TXI"[{:08x}{:08x}{:08x}{:08x}]{{}}",
                                             txid_.txplid,
                                             txid_.txsvid,
                                             txid_.txcnid,
                                             txid_.txprid,
                                             TX_NO_OBJ))
        }
        if(result_obj_) {
            IFLOG(conn_->broker_->log_, info(LS_TXO"[{:08x}{:08x}{:08x}{:08x}]{}",
                                             txid_.txplid,
                                             txid_.txsvid,
                                             txid_.txcnid,
                                             txid_.txprid,
                                             spdlog_nclass_type{*result_obj_.get()}))
        } else {
            IFLOG(conn_->broker_->log_, info(LS_TXO"[{:08x}{:08x}{:08x}{:08x}]{{}}",
                                             txid_.txplid,
                                             txid_.txsvid,
                                             txid_.txcnid,
                                             txid_.txprid,
                                             TX_NO_OBJ))
        }
    } else {
        if(request_obj_) {
            IFLOG(conn_->broker_->log_, info(LS_TXO"[{:08x}{:08x}{:08x}{:08x}]{}",
                                             txid_.txplid,
                                             txid_.txsvid,
                                             txid_.txcnid,
                                             txid_.txprid,
                                             spdlog_nclass_type{*request_obj_.get()}))
        } else {
            IFLOG(conn_->broker_->log_, info(LS_TXO"[{:08x}{:08x}{:08x}{:08x}]{{}}",
                                             txid_.txplid,
                                             txid_.txsvid,
                                             txid_.txcnid,
                                             txid_.txprid,
                                             TX_NO_OBJ))
        }
        if(result_obj_) {
            IFLOG(conn_->broker_->log_, info(LS_TXI"[{:08x}{:08x}{:08x}{:08x}]{}",
                                             txid_.txplid,
                                             txid_.txsvid,
                                             txid_.txcnid,
                                             txid_.txprid,
                                             spdlog_nclass_type{*result_obj_.get()}))
        } else {
            IFLOG(conn_->broker_->log_, info(LS_TXI"[{:08x}{:08x}{:08x}{:08x}]{{}}",
                                             txid_.txplid,
                                             txid_.txsvid,
                                             txid_.txcnid,
                                             txid_.txprid,
                                             TX_NO_OBJ))
        }
    }
    IFLOG(conn_->broker_->log_, info(LS_TRX"[{:08x}{:08x}{:08x}{:08x}][{}][TXRES:{}, TXRESCODE:{}, RESCLS:{}][RTT-NS:{}]",
                                     txid_.txplid,
                                     txid_.txsvid,
                                     txid_.txcnid,
                                     txid_.txprid,
                                     tx_res_str,
                                     tx_res_,
                                     result_code_,
                                     rescls_,
                                     ss.str().c_str()))
}

RetCode tx_impl::set_closed()
{
    if(status_ != TransactionStatus_FLYING) {
        return RetCode_BADSTTS;
    }
    const char *tx_res_str = (tx_res_ == TransactionResult_COMMITTED) ? TX_RES_COMMT : TX_RES_FAIL;
    trace_tx_closure(tx_res_str);
    set_status(TransactionStatus_CLOSED);
    if(ipubl_) {
        ilistener_->on_close(*ipubl_);
    } else {
        olistener_->on_close(*opubl_);
    }
    return RetCode_OK;
}

RetCode tx_impl::set_aborted()
{
    tx_res_ = TransactionResult_ABORTED;
    const char *tx_res_str = TX_RES_ABORTED;
    trace_tx_closure(tx_res_str);
    set_status(TransactionStatus_CLOSED);
    if(ipubl_) {
        ilistener_->on_close(*ipubl_);
    } else {
        olistener_->on_close(*opubl_);
    }
    return RetCode_OK;
}

RetCode tx_impl::set_status(TransactionStatus status)
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

}

namespace vlg {

incoming_transaction_impl::incoming_transaction_impl(incoming_transaction &publ,
                                                     std::shared_ptr<incoming_connection> &conn,
                                                     incoming_transaction_listener &listener) :
    tx_impl(publ, *conn, listener),
    conn_sh_(conn)
{}

incoming_transaction_impl::~incoming_transaction_impl()
{
    if(status_ ==  TransactionStatus_FLYING) {
        IFLOG(conn_->broker_->log_, critical(LS_DTR"[transaction is not in a safe state::{}]" LS_EXUNX, __func__, status_))
    }
}

void incoming_transaction_impl::set_request_obj_on_request(std::unique_ptr<nclass> &val)
{
    request_obj_ = std::move(val);
    req_nclassid_ = request_obj_->get_id();
}

// VLG_TRANSACTION SENDING METHS

RetCode incoming_transaction_impl::send_response()
{
    if(status_ != TransactionStatus_FLYING) {
        return RetCode_BADSTTS;
    }

    g_bbuf gbb;
    build_PKT_TXRESP(tx_res_,
                     result_code_,
                     &txid_,
                     rescls_,
                     res_clsenc_,
                     res_nclassid_,
                     &gbb);
    int totbytes = result_obj_ ? result_obj_->serialize(res_clsenc_, nullptr, &gbb) : 0;
    totbytes = htonl(totbytes);
    if(result_obj_) {
        gbb.put(&totbytes, (6*4), 4);
    }

    std::unique_ptr<conn_pkt> cpkt(new conn_pkt(nullptr, std::move(gbb)));
    conn_->pkt_sending_q_.put(&cpkt);
    sel_evt *evt = new sel_evt(VLG_SELECTOR_Evt_SendPacket, conn_sh_);
    return conn_->broker_->selector_.notify(evt);
}

}

namespace vlg {

outgoing_transaction_impl::outgoing_transaction_impl(outgoing_transaction &publ,
                                                     outgoing_transaction_listener &listener) :
    tx_impl(publ, listener)
{}

outgoing_transaction_impl::~outgoing_transaction_impl()
{
    if(status_ ==  TransactionStatus_FLYING) {
        IFLOG(conn_->broker_->log_, critical(LS_DTR"[transaction is not in a safe state::{}]" LS_EXUNX, __func__, status_))
    }
}

RetCode outgoing_transaction_impl::re_new()
{
    if(status_ == TransactionStatus_FLYING) {
        IFLOG(conn_->broker_->log_, error(LS_CLO "[transaction is flying, cannot renew]", __func__))
        return RetCode_BADSTTS;
    }
    static_cast<outgoing_connection_impl *>(conn_)->next_tx_id(txid_);
    set_status(TransactionStatus_INITIALIZED);
    return RetCode_OK;
}

void outgoing_transaction_impl::set_result_obj_on_response(std::unique_ptr<nclass> &val)
{
    result_obj_ = std::move(val);
    res_nclassid_ = result_obj_->get_id();
    rescls_ = true;
}

// VLG_TRANSACTION SENDING METHS

RetCode outgoing_transaction_impl::send()
{
    if(status_ != TransactionStatus_INITIALIZED) {
        return RetCode_BADSTTS;
    }

    rt_mark_time(&start_mark_tim_);
    set_flying();
    outgoing_transaction_impl *self = this;
    static_cast<outgoing_connection_impl *>(conn_)->outg_flytx_map_.put(&txid_, &self);
    IFLOG(conn_->broker_->log_, info(LS_OUT"[{:08x}{:08x}{:08x}{:08x}][TXTYPE:{}, TXACT:{}, CLSENC:{}, RSCLREQ:{}]",
                                     txid_.txplid,
                                     txid_.txsvid,
                                     txid_.txcnid,
                                     txid_.txprid,
                                     txtype_,
                                     txactn_,
                                     req_clsenc_,
                                     rsclrq_))

    g_bbuf gbb;
    build_PKT_TXRQST(txtype_,
                     txactn_,
                     &txid_,
                     rsclrq_,
                     req_clsenc_,
                     req_nclassid_,
                     conn_->connid_,
                     &gbb);
    int totbytes = request_obj_ ? request_obj_->serialize(req_clsenc_, current_obj_.get(), &gbb) : 0;
    totbytes = htonl(totbytes);
    if(request_obj_) {
        gbb.put(&totbytes, (6*4), 4);
    }

    std::unique_ptr<conn_pkt> cpkt(new conn_pkt(nullptr, std::move(gbb)));
    conn_->pkt_sending_q_.put(&cpkt);
    sel_evt *evt = new sel_evt(VLG_SELECTOR_Evt_SendPacket, conn_);
    return conn_->broker_->selector_.notify(evt);
}

}
