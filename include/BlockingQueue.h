/*
 * BlockingQueue.h
 *
 *  Created on: Nov 9, 2016
 *      Author: msoderen
 */

#ifndef BLOCKINGQUEUE_H_
#define BLOCKINGQUEUE_H_

#include <iostream>
#include <vector>
#include <queue>
#include <condition_variable>
#include <assert.h>
#include "structures.h"

#define MAX_CAPACITY 20

/**
 * \class BlockingQueue
 *
 *
 * \brief 		_TCPSendQueue = new BlockingQueue<QueueElementBase*>(255);
 * 				_TCPSendQueue->put(new QueueElementBase() );
 *
 *This is a blocking thread safe FIFO queue
 *
 *
 * \author  Martin Soderen
 *
 * \version 0.1
 *
 * \date  2017/04/147 00:00:00
 *
 * Contact: martin.soderen@gmail.com
 *
 * Created on: Fri Apr 14 00:00:00 2017
 *
 * $Id: $
 *
 */
template<typename T>
class BlockingQueue {
public:
	/** \brief Constructor
	 * \param <T> template
	 */
	BlockingQueue() :
			mtx(), full_(), empty_(), capacity_(MAX_CAPACITY) {
	}
	/** \brief Constructor
	 * \param <T> template
	 * \param capacity how many elements you can fill
	 */
	BlockingQueue(std::size_t capacity) :
			mtx(), full_(), empty_(), capacity_(capacity) {
	}
	/** \brief put an element in the queue, blocks if full
	 * \param task element to put in queue
	 */
	void Put(const T& task);
	/** \brief take an element from the queue, blocks if empty
	 * \returns element
	 */
	T Take();
	/** \brief returns the front element(been longest in the queue), does not pop, blocks if empty
	 * \returns element
	 */
	T Front();
	/** \brief returns the back element(been shortest in the queue), does not pop, blocks if empty
	 * \returns element
	 */
	T Back();
	/** \brief returns the number of elements in the queue
	 * \returns size of queue
	 */
	size_t Size();
	/** \brief returns true if the queue is empty
	 * \returns bool if empty
	 */
	bool Empty();
	/** \brief sets the capacity of the queue
	 * \param capacity the new capacity of the queue
	 */
	void SetCapacity(const size_t capacity);
private:
	/** \brief deleted copy constructor
	 */
	BlockingQueue(const BlockingQueue& rhs);
	/** \brief deleted = operator
	 */
	BlockingQueue& operator=(const BlockingQueue& rhs);

private:
	///lock
	mutable std::mutex mtx;
	///condition variable
	std::condition_variable full_;
	///condition variable
	std::condition_variable empty_;
	///the underlying queue
	std::queue<T> queue_;
	///the capacity
	size_t capacity_;
};

#endif
