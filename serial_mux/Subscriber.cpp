
#include "Subscriber.h"
#include "Common.h"


namespace DustSerialMux {

   // return the full subscription filter
   int CSubscriber::getSubscription() const {
      return m_newParams.filter;
   }

   // return the full subscription filter
   int CSubscriber::getUnreliable() const {
      return m_newParams.unreliable;
   }

   // update the subscription filter
   void CSubscriber::setSubscription(SubscriptionParams params) {
      m_inTransaction = true;
      m_newParams = params;
   }

   // check whether we should pass through the notification
   bool CSubscriber::isSubscribed(int notifType) const {
      return subscribeFilterMatch(m_params, notifType);
   }

   void CSubscriber::resetFilter() 
   {
      m_newParams = m_params;  // reset the newFilter to the previous value
      m_inTransaction = false;
   }
      
   void CSubscriber::commitFilter() 
   {
      m_params = m_newParams;  // set the filter to the new value
      m_inTransaction = false;
   }   

} // namespace
