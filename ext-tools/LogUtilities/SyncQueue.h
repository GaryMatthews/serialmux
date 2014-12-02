/*
 * Copyright (c) 2010, Dust Networks, Inc.
 */

#pragma once

#ifndef SyncQueue_H_
#define SyncQueue_H_

#include <queue>

#include <boost/thread/mutex.hpp>
#include <boost/thread/condition_variable.hpp>


template <typename E>
class CSyncQueue
{
public:
   CSyncQueue() : m_queue(), m_lock(), m_cond() { ; }
   ~CSyncQueue() { 
      // TODO cancel outstanding wait operations
      // it's the caller's responsibility to empty the queue before
      // destruction if necessary
   }      

   void clear();
   
   void push(const E& el);

   bool empty();

   bool timedPop(E& el, int seconds);

private:
   std::queue<E> m_queue;
   boost::mutex        m_lock;
   boost::condition_variable m_cond;
};


// template definitions must be inline to prevent linker from complaining
// the compiler requires the full definition when the template is instantiated

template<typename E>
void CSyncQueue<E>::clear()
{
   boost::mutex::scoped_lock guard(m_lock);
   while (!m_queue.empty()) {
      m_queue.pop();
   }

   // notify everyone waiting
   m_cond.notify_all();
}

template<typename E>
void CSyncQueue<E>::push(const E& el)
{
   boost::mutex::scoped_lock guard(m_lock);
   bool wasEmpty = m_queue.empty();
   m_queue.push(el);
   if (wasEmpty) {
      m_cond.notify_one();
   }
}

template<typename E>
bool CSyncQueue<E>::empty()
{
   boost::mutex::scoped_lock guard(m_lock);
   return m_queue.empty();
}

template<typename E>
bool CSyncQueue<E>::timedPop(E& el, int seconds)
{
   boost::mutex::scoped_lock guard(m_lock);
   if (m_queue.empty())
   {
      m_cond.timed_wait(guard, boost::posix_time::seconds(seconds));
   }

   if (m_queue.empty())
   {
      return false;
   }
   el = m_queue.front();
   m_queue.pop();
   return true;
}


#endif /* ! SyncQueue_H_ */
