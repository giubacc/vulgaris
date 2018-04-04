/*
 *
 * (C) 2017 - giuseppe.baccini@gmail.com
 *
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 *
 */

#include "pr_impl.h"
#include "conn_impl.h"
#include "tx_impl.h"

#define TX_RES_COMMT    "COMMITTED"
#define TX_RES_FAIL     "FAILED"
#define TX_RES_ABORTED  "ABORTED"
#define TX_NO_OBJ       "NO-OBJ"

namespace vlg {
const std_shared_ptr_obj_mng<transaction> tx_std_shp_omng;
}

namespace vlg {

transaction_impl::transaction_impl(transaction &publ) :
    conn_(nullptr),
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
    publ_(publ)
{}

transaction_impl::~transaction_impl()
{
    if(status_ ==  TransactionStatus_FLYING) {
        IFLOG(cri(TH_ID, LS_DTR"[transaction is not in a safe state::%d]" LS_EXUNX, __func__, status_))
    }
}

RetCode transaction_impl::re_new()
{
    IFLOG(trc(TH_ID, LS_OPN, __func__))
    if(status_ == TransactionStatus_FLYING) {
        IFLOG(err(TH_ID, LS_CLO "[transaction is flying, cannot renew]", __func__))
        return vlg::RetCode_BADSTTS;
    }
    conn_->impl_->next_tx_id(txid_);
    set_status(TransactionStatus_INITIALIZED);
    IFLOG(trc(TH_ID, LS_OPN, __func__))
    return vlg::RetCode_OK;
}

void transaction_impl::set_connection_sh(std::shared_ptr<connection> &val)
{
    conn_sh_ = val;
    conn_ = val.get();
}

void transaction_impl::set_request_obj(const nclass &val)
{
    request_obj_ = val.clone();
    req_nclassid_ = request_obj_->get_id();
}

void transaction_impl::set_current_obj(const nclass &val)
{
    current_obj_ = val.clone();
    req_nclassid_ = current_obj_->get_id();
}

void transaction_impl::set_result_obj(const nclass &val)
{
    result_obj_ = val.clone();
    res_nclassid_ = result_obj_->get_id();
    rescls_ = true;
}

void transaction_impl::set_request_obj_on_request(nclass &val)
{
    request_obj_.reset(&val);
    req_nclassid_ = request_obj_->get_id();
}

void transaction_impl::set_result_obj_on_response(nclass &val)
{
    result_obj_.reset(&val);
    res_nclassid_ = result_obj_->get_id();
    rescls_ = true;
}

// VLG_TRANSACTION SENDING METHS

RetCode transaction_impl::send()
{
    IFLOG(trc(TH_ID, LS_OPN, __func__))
    RetCode rcode = vlg::RetCode_OK;
    if(status_ != TransactionStatus_INITIALIZED) {
        IFLOG(err(TH_ID, LS_CLO, __func__))
        return vlg::RetCode_BADSTTS;
    }

    vlg::rt_mark_time(&start_mark_tim_);
    set_flying();
    transaction_impl *self = this;
    if((rcode = conn_->impl_->outg_flytx_map_.put(&txid_, &self))) {
        set_status(TransactionStatus_ERROR);
        IFLOG(err(TH_ID, LS_TRL"[error putting tx into flying map - res:%d]", rcode))
        return rcode;
    }
    IFLOG(inf(TH_ID,
              LS_OUT"[%08x%08x%08x%08x][TXTYPE:%d, TXACT:%d, CLSENC:%d, RSCLREQ:%d]",
              txid_.txplid,
              txid_.txsvid,
              txid_.txcnid,
              txid_.txprid,
              txtype_,
              txactn_,
              req_clsenc_,
              rsclrq_))
    g_bbuf *gbb = new g_bbuf();
    build_PKT_TXRQST(txtype_,
                     txactn_,
                     &txid_,
                     rsclrq_,
                     req_clsenc_,
                     req_nclassid_,
                     conn_->get_id(),
                     gbb);
    int totbytes = request_obj_ ? request_obj_->serialize(req_clsenc_, current_obj_.get(), gbb) : 0;
    totbytes = htonl(totbytes);
    if(request_obj_) {
        gbb->put(&totbytes, (6*4), 4);
    }
    gbb->flip();
    RET_ON_KO(conn_->impl_->pkt_sending_q_.put(&gbb))
    selector_event *evt = new selector_event(VLG_SELECTOR_Evt_SendPacket, conn_);
    if((rcode = conn_->impl_->peer_->selector_.evt_enqueue_and_notify(evt))) {
        set_status(TransactionStatus_ERROR);
    }
    IFLOG(trc(TH_ID, LS_CLO "[res:%d]", __func__, rcode))
    return rcode;
}

RetCode transaction_impl::send_response()
{
    IFLOG(trc(TH_ID, LS_OPN, __func__))
    RetCode rcode = vlg::RetCode_OK;
    if(status_ != TransactionStatus_FLYING) {
        IFLOG(err(TH_ID, LS_CLO, __func__))
        return vlg::RetCode_BADSTTS;
    }
    g_bbuf *gbb = new g_bbuf();
    build_PKT_TXRESP(tx_res_,
                     result_code_,
                     &txid_,
                     rescls_,
                     res_clsenc_,
                     res_nclassid_,
                     gbb);
    int totbytes = result_obj_ ? result_obj_->serialize(res_clsenc_, nullptr, gbb) : 0;
    totbytes = htonl(totbytes);
    if(result_obj_) {
        gbb->put(&totbytes, (6*4), 4);
    }
    gbb->flip();
    RET_ON_KO(conn_->impl_->pkt_sending_q_.put(&gbb))
    selector_event *evt = new selector_event(VLG_SELECTOR_Evt_SendPacket, conn_sh_);
    rcode = conn_->impl_->peer_->selector_.evt_enqueue_and_notify(evt);
    if(rcode) {
        set_status(TransactionStatus_ERROR);
    }
    IFLOG(trc(TH_ID, LS_CLO "[res:%d]", __func__, rcode))
    return rcode;
}

// VLG_TRANSACTION STATUS

RetCode transaction_impl::set_flying()
{
    IFLOG(trc(TH_ID, LS_OPN, __func__))
    if(status_ != TransactionStatus_INITIALIZED) {
        IFLOG(err(TH_ID, LS_CLO, __func__))
        return vlg::RetCode_BADSTTS;
    }
    IFLOG(dbg(TH_ID, LS_TXO"[%08x%08x%08x%08x][FLYING]",
              txid_.txplid,
              txid_.txsvid,
              txid_.txcnid,
              txid_.txprid))
    IFLOG(trc(TH_ID, LS_OPN, __func__))
    set_status(TransactionStatus_FLYING);
    return vlg::RetCode_OK;
}

inline void transaction_impl::trace_tx_closure(const char *tx_res_str)
{
    char tim_buf[32];
    rt_time_t fin_mark_tim, dt_mark_tim;
    vlg::rt_mark_time(&fin_mark_tim);
    dt_mark_tim = vlg::rt_diff_time(start_mark_tim_, fin_mark_tim);
    snprintf(tim_buf, 32, "%14llu", dt_mark_tim);
    if(conn_->get_connection_type() == ConnectionType_INGOING) {
        if(request_obj_) {
            IFLOG(inf_nclass(TH_ID,
                             request_obj_.get(),
                             true,
                             LS_TXI"[%08x%08x%08x%08x]",
                             txid_.txplid,
                             txid_.txsvid,
                             txid_.txcnid,
                             txid_.txprid))
        } else {
            IFLOG(inf(TH_ID,
                      LS_TXI"[%08x%08x%08x%08x]{%s}",
                      txid_.txplid,
                      txid_.txsvid,
                      txid_.txcnid,
                      txid_.txprid,
                      TX_NO_OBJ))
        }
        if(result_obj_) {
            IFLOG(inf_nclass(TH_ID,
                             result_obj_.get(),
                             true,
                             LS_TXO"[%08x%08x%08x%08x]",
                             txid_.txplid,
                             txid_.txsvid,
                             txid_.txcnid,
                             txid_.txprid))
        } else {
            IFLOG(inf(TH_ID,
                      LS_TXO"[%08x%08x%08x%08x]{%s}",
                      txid_.txplid,
                      txid_.txsvid,
                      txid_.txcnid,
                      txid_.txprid,
                      TX_NO_OBJ))
        }
    } else {
        if(request_obj_) {
            IFLOG(inf_nclass(TH_ID,
                             request_obj_.get(),
                             true,
                             LS_TXO"[%08x%08x%08x%08x]",
                             txid_.txplid,
                             txid_.txsvid,
                             txid_.txcnid,
                             txid_.txprid))
        } else {
            IFLOG(inf(TH_ID,
                      LS_TXO"[%08x%08x%08x%08x]{%s}",
                      txid_.txplid,
                      txid_.txsvid,
                      txid_.txcnid,
                      txid_.txprid,
                      TX_NO_OBJ))
        }
        if(result_obj_) {
            IFLOG(inf_nclass(TH_ID,
                             result_obj_.get(),
                             true,
                             LS_TXI"[%08x%08x%08x%08x]",
                             txid_.txplid,
                             txid_.txsvid,
                             txid_.txcnid,
                             txid_.txprid))
        } else {
            IFLOG(inf(TH_ID, LS_TXI"[%08x%08x%08x%08x]{%s}",
                      txid_.txplid,
                      txid_.txsvid,
                      txid_.txcnid,
                      txid_.txprid,
                      TX_NO_OBJ))
        }
    }
    IFLOG(inf(TH_ID,
              LS_TRX"[%08x%08x%08x%08x][%s][TXRES:%d, TXRESCODE:%d, RESCLS:%d][RTT-NS:%s]",
              txid_.txplid,
              txid_.txsvid,
              txid_.txcnid,
              txid_.txprid,
              tx_res_str,
              tx_res_,
              result_code_,
              rescls_,
              tim_buf))
}

RetCode transaction_impl::set_closed()
{
    IFLOG(trc(TH_ID, LS_OPN, __func__))
    if(status_ != TransactionStatus_FLYING) {
        IFLOG(err(TH_ID, LS_CLO, __func__))
        return vlg::RetCode_BADSTTS;
    }
    const char *tx_res_str = (tx_res_ == TransactionResult_COMMITTED) ? TX_RES_COMMT : TX_RES_FAIL;
    trace_tx_closure(tx_res_str);
    set_status(TransactionStatus_CLOSED);
    publ_.on_close();
    IFLOG(trc(TH_ID, LS_OPN, __func__))
    return vlg::RetCode_OK;
}

RetCode transaction_impl::set_aborted()
{
    IFLOG(trc(TH_ID, LS_OPN, __func__))
    tx_res_ = TransactionResult_ABORTED;
    const char *tx_res_str = TX_RES_ABORTED;
    trace_tx_closure(tx_res_str);
    set_status(TransactionStatus_CLOSED);
    publ_.on_close();
    IFLOG(trc(TH_ID, LS_OPN, __func__))
    return vlg::RetCode_OK;
}

RetCode transaction_impl::set_status(TransactionStatus status)
{
    IFLOG(trc(TH_ID, LS_OPN, __func__))
    scoped_mx smx(mon_);
    status_ = status;
    publ_.on_status_change(status_);
    mon_.notify_all();
    IFLOG(trc(TH_ID, LS_OPN, __func__))
    return vlg::RetCode_OK;
}

RetCode transaction_impl::await_for_status_reached_or_outdated(TransactionStatus test,
                                                               TransactionStatus &current,
                                                               time_t sec,
                                                               long nsec)
{
    IFLOG(trc(TH_ID, LS_OPN, __func__))
    scoped_mx smx(mon_);
    if(status_ < TransactionStatus_INITIALIZED) {
        IFLOG(err(TH_ID, LS_CLO, __func__))
        return vlg::RetCode_BADSTTS;
    }
    RetCode rcode = vlg::RetCode_OK;
    while(status_ < test) {
        int pthres;
        if((pthres = mon_.wait(sec, nsec))) {
            if(pthres == ETIMEDOUT) {
                rcode = vlg::RetCode_TIMEOUT;
                break;
            }
        }
    }
    current = status_;
    IFLOG(log(rcode ? vlg::TL_WRN : vlg::TL_DBG, TH_ID, LS_CLO "test:%d [reached or outdated] current:%d",
              __func__,
              test,
              status_))
    return rcode;
}

RetCode transaction_impl::await_for_closure(time_t sec, long nsec)
{
    IFLOG(trc(TH_ID, LS_OPN, __func__))
    scoped_mx smx(mon_);
    if(status_ < TransactionStatus_INITIALIZED) {
        IFLOG(err(TH_ID, LS_CLO, __func__))
        return vlg::RetCode_BADSTTS;
    }
    RetCode rcode = vlg::RetCode_OK;
    while(status_ < TransactionStatus_CLOSED) {
        int pthres;
        if((pthres = mon_.wait(sec, nsec))) {
            if(pthres == ETIMEDOUT) {
                rcode = vlg::RetCode_TIMEOUT;
                break;
            }
        }
    }
    IFLOG(log(rcode ? vlg::TL_WRN : vlg::TL_DBG, TH_ID, LS_CLO "[res:%d][closed %s]",
              __func__, rcode, rcode ? "not reached" : "reached"))
    return rcode;
}

}