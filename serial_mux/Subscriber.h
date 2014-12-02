#pragma once
#ifndef Subscriber_H_
#define Subscriber_H_

#include "Common.h"

namespace DustSerialMux {

   // the subscriber interface is used to track and manage
   // the notification filter used for subscriptions
   class ISubscriber 
   {
   public:
      // notification subscriptions
      virtual int  getSubscription() const = 0;
      virtual void setSubscription(SubscriptionParams params) = 0;
      virtual bool isSubscribed(int notifType) const = 0;
      // new filters are only applied after the subscribe reply is sent
      virtual void resetFilter() = 0;
      virtual void commitFilter() = 0;
   };
   
   class CSubscriber : public ISubscriber
   {
   public:
      CSubscriber()
         : m_params(), m_inTransaction(false), m_newParams() 
      { ; }
      virtual ~CSubscriber() { ; }
      
      // getSubscription, getUnreliable operate on the new params so that 
      // the recompute operation returns the latest value
      virtual int  getSubscription() const;
      virtual int  getUnreliable() const;
      // isSubscribed operates on the current filter
      virtual bool isSubscribed(int notifType) const;

      // setSubscription opens a new transaction
      virtual void setSubscription(SubscriptionParams params);

      // new filters are only applied after the subscribe reply is sent
      // the reset and commit end the current transaction
      virtual void resetFilter();
      virtual void commitFilter();
      
   private:
      SubscriptionParams m_params;
      bool m_inTransaction;
      SubscriptionParams m_newParams;
   };
   
} // namespace

#endif
