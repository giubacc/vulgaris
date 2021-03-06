/*
 * vulgaris
 * (C) 2018 - giuseppe.baccini@live.com
 *
 */

import Foundation

/**
 * SubscriptionEvent
 */
class SubscriptionEvent
{
    
}

/**
 * OutgoingSubscriptionListener
 */
protocol OutgoingSubscriptionListener
{
    func onStatusChange(outgoingSubscription: OutgoingSubscription, subscriptionStatus: SubscriptionStatus)
    func onStart(outgoingSubscription: OutgoingSubscription)
    func onStop(outgoingSubscription: OutgoingSubscription)
    func onIncomingEvent(outgoingSubscription: OutgoingSubscription, subscriptionEvent: SubscriptionEvent) -> RetCode
}

/**
 * OutgoingSubscription
 */
class OutgoingSubscription
{
}

/**
 * IncomingSubscriptionListener
 */
protocol IncomingSubscriptionListener
{
    func onStatusChange(incomingSubscription: IncomingSubscription, subscriptionStatus: SubscriptionStatus)
    func onStop(incomingSubscription: IncomingSubscription)
    func onAcceptEvent(incomingSubscription: IncomingSubscription, subscriptionEvent: SubscriptionEvent) -> RetCode
}

/**
 * IncomingSubscription
 */
class IncomingSubscription
{
    required init(_ incoConn: IncomingConnection, _ sh_inco_sbs: OpaquePointer){
        self.incoConn = incoConn
        self.own_inco_sbs_op = inco_subscription_get_own_ptr(sh_inco_sbs)
        self.inco_sbs_op = inco_subscription_get_ptr(own_inco_sbs_op)
        inco_subscription_set_on_releaseable_oc(inco_sbs_op, on_release, nil)
        inco_subscription_set_on_accept_distribution_oc(inco_sbs_op, on_accept_distribution, nil)
    }
    
    fileprivate func on_release(sbs: OpaquePointer!, ud: UnsafeMutableRawPointer!){
        incoConn.isbsRepo.removeValue(forKey: id)
        inco_subscription_release(own_inco_sbs_op)
    }
    
    fileprivate func on_accept_distribution(sbs: OpaquePointer!, evt: OpaquePointer!, ud: UnsafeMutableRawPointer!) -> RetCode{
        return RetCode_OK;
    }
    
    var id : UInt32 {
        get {
            return inco_subscription_get_id(inco_sbs_op)
        }
    }
    
    let incoConn : IncomingConnection
    let own_inco_sbs_op : OpaquePointer
    var inco_sbs_op : OpaquePointer
}
