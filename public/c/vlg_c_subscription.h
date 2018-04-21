/*
 * vulgaris
 * (C) 2018 - giuseppe.baccini@gmail.com
 *
 */

#ifndef VLG_C_SUBSCRIPTION_H_
#define VLG_C_SUBSCRIPTION_H_
#include "vlg.h"

/************************************************************************
SUBSCRIPTION EVENT
************************************************************************/

unsigned int subscription_event_get_id(subscription_event *sev);
SubscriptionEventType subscription_event_get_event_type(subscription_event *sev);
ProtocolCode subscription_event_get_proto_code(subscription_event *sev);
unsigned int subscription_event_get_timestamp_0(subscription_event *sev);
unsigned int subscription_event_get_timestamp_1(subscription_event *sev);
Action  subscription_event_get_action(subscription_event *sev);
own_nclass *subscription_event_get_object(subscription_event *sev);

/************************************************************************
INCOMING SUBSCRIPTION HANDLERS
************************************************************************/

typedef void(*inco_subscription_status_change)(incoming_subscription *isbs, SubscriptionStatus status, void *usr_data);
typedef RetCode(*inco_subscription_accept_distribution)(incoming_subscription *isbs, const subscription_event *sbs_evt, void *usr_data);
typedef void(*inco_subscription_on_stop)(incoming_subscription *isbs, void *usr_data);

/************************************************************************
INCOMING SUBSCRIPTION
************************************************************************/

void inco_subscription_release(shr_incoming_subscription *sbs);
incoming_subscription *inco_subscription_get_ptr(shr_incoming_subscription *sbs);
incoming_connection *inco_subscription_get_connection(incoming_subscription *sbs);
unsigned int inco_subscription_get_id(incoming_subscription *sbs);
unsigned int inco_subscription_get_subscription_nclass_id(incoming_subscription *sbs);
SubscriptionType inco_subscription_get_subscription_type(incoming_subscription *sbs);
SubscriptionMode inco_subscription_get_subscription_mode(incoming_subscription *sbs);
SubscriptionFlowType inco_subscription_get_subscription_flow_type(incoming_subscription *sbs);
SubscriptionDownloadType inco_subscription_get_subscription_download_type(incoming_subscription *sbs);
Encode inco_subscription_get_subscription_nclass_encode(incoming_subscription *sbs);
unsigned int inco_subscription_get_open_timestamp_0(incoming_subscription *sbs);
unsigned int inco_subscription_get_open_timestamp_1(incoming_subscription *sbs);
int inco_subscription_is_initial_query_ended(incoming_subscription *sbs);
void inco_subscription_set_subscription_nclass_id(incoming_subscription *sbs, unsigned int nclass_id);
void inco_subscription_set_subscription_type(incoming_subscription *sbs, SubscriptionType sbs_type);
void inco_subscription_set_subscription_mode(incoming_subscription *sbs, SubscriptionMode sbs_mode);
void inco_subscription_set_subscription_flow_type(incoming_subscription *sbs, SubscriptionFlowType sbs_flow_type);
void inco_subscription_set_subscription_download_type(incoming_subscription *sbs, SubscriptionDownloadType sbs_dwnl_type);
void inco_subscription_set_nclass_encode(incoming_subscription *sbs, Encode nclass_encode);
void inco_subscription_set_open_timestamp_0(incoming_subscription *sbs, unsigned int ts0);
void inco_subscription_set_open_timestamp_1(incoming_subscription *sbs, unsigned int ts1);
RetCode inco_subscription_await_for_status_reached(incoming_subscription *sbs, SubscriptionStatus test, SubscriptionStatus *current, time_t sec, long nsec);
void inco_subscription_set_status_change_handler(incoming_subscription *sbs, inco_subscription_status_change handler, void *usr_data);
void inco_subscription_set_accept_distribution_handler(incoming_subscription *sbs, inco_subscription_accept_distribution handler, void *usr_data);
void inco_subscription_set_on_stop_handler(incoming_subscription *sbs, inco_subscription_on_stop handler, void *usr_data);
RetCode inco_subscription_stop(incoming_subscription *sbs);
RetCode inco_subscription_await_for_stop_result(incoming_subscription *sbs, SubscriptionResponse *stop_result, ProtocolCode *stop_protocode, time_t sec, long nsec);

/************************************************************************
INCOMING SUBSCRIPTION HANDLERS
************************************************************************/

typedef void(*outg_subscription_status_change)(outgoing_subscription *osbs, SubscriptionStatus status, void *usr_data);
typedef void(*outg_subscription_notify_event)(outgoing_subscription *osbs, const subscription_event *sbs_evt, void *usr_data);
typedef void(*outg_subscription_on_start)(outgoing_subscription *osbs, void *usr_data);
typedef void(*outg_subscription_on_stop)(outgoing_subscription *osbs, void *usr_data);

/************************************************************************
OUTGOING SUBSCRIPTION
************************************************************************/

own_outgoing_subscription *outg_subscription_create(void);
outgoing_subscription *outg_subscription_get_ptr(own_outgoing_subscription *sbs);
void outg_subscription_destroy(own_outgoing_subscription *sbs);
RetCode outg_subscription_bind(outgoing_subscription *sbs, outgoing_connection *conn);
outgoing_connection *outg_subscription_get_connection(outgoing_subscription *sbs);
unsigned int outg_subscription_get_id(outgoing_subscription *sbs);
unsigned int outg_subscription_get_subscription_nclass_id(outgoing_subscription *sbs);
SubscriptionType outg_subscription_get_subscription_type(outgoing_subscription *sbs);
SubscriptionMode outg_subscription_get_subscription_mode(outgoing_subscription *sbs);
SubscriptionFlowType outg_subscription_get_subscription_flow_type(outgoing_subscription *sbs);
SubscriptionDownloadType outg_subscription_get_subscription_download_type(outgoing_subscription *sbs);
Encode outg_subscription_get_subscription_nclass_encode(outgoing_subscription *sbs);
unsigned int outg_subscription_get_open_timestamp_0(outgoing_subscription *sbs);
unsigned int outg_subscription_get_open_timestamp_1(outgoing_subscription *sbs);
void outg_subscription_set_subscription_nclass_id(outgoing_subscription *sbs, unsigned int nclass_id);
void outg_subscription_set_subscription_type(outgoing_subscription *sbs, SubscriptionType sbs_type);
void outg_subscription_set_subscription_mode(outgoing_subscription *sbs, SubscriptionMode sbs_mode);
void outg_subscription_set_subscription_flow_type(outgoing_subscription *sbs, SubscriptionFlowType sbs_flow_type);
void outg_subscription_set_subscription_download_type(outgoing_subscription *sbs, SubscriptionDownloadType sbs_dwnl_type);
void outg_subscription_set_nclass_encode(outgoing_subscription *sbs, Encode nclass_encode);
void outg_subscription_set_open_timestamp_0(outgoing_subscription *sbs, unsigned int ts0);
void outg_subscription_set_open_timestamp_1(outgoing_subscription *sbs, unsigned int ts1);
RetCode outg_subscription_await_for_status_reached(outgoing_subscription *sbs, SubscriptionStatus test, SubscriptionStatus *current, time_t sec, long nsec);
void outg_subscription_set_status_change_handler(outgoing_subscription *sbs, outg_subscription_status_change handler, void *usr_data);
void outg_subscription_set_event_notify_handler(outgoing_subscription *sbs, outg_subscription_notify_event handler, void *usr_data);
void outg_subscription_set_on_start_handler(outgoing_subscription *sbs, outg_subscription_on_start handler, void *usr_data);
void outg_subscription_set_on_stop_handler(outgoing_subscription *sbs, outg_subscription_on_stop handler, void *usr_data);
RetCode outg_subscription_start(outgoing_subscription *sbs);

RetCode outg_subscription_start_full(outgoing_subscription *sbs,
                                     SubscriptionType sbs_type,
                                     SubscriptionMode sbs_mode,
                                     SubscriptionFlowType sbs_flow_type,
                                     SubscriptionDownloadType sbs_dwnl_type,
                                     Encode nclass_encode,
                                     unsigned int nclass_id,
                                     unsigned int open_timestamp_0,
                                     unsigned int open_timestamp_1);

RetCode outg_subscription_await_for_start_result(outgoing_subscription *sbs, SubscriptionResponse *start_result, ProtocolCode *start_protocode, time_t sec, long nsec);
RetCode outg_subscription_stop(outgoing_subscription *sbs);
RetCode outg_subscription_await_for_stop_result(outgoing_subscription *sbs, SubscriptionResponse *stop_result, ProtocolCode *stop_protocode, time_t sec, long nsec);

#endif
