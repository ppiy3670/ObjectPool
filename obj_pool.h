#ifndef	OBJ_POOL_H
#define OBJ_POOL_H

#include <list>
#include <time.h>
#include <errno.h>
#include <thread>
#include <unistd.h>
#include <algorithm>
#include <chrono>

#include<string>
#include<vector>
#include <pthread.h>

typedef std::vector<std::string> StrArray;

class SpinLock {
public:
	SpinLock() { pthread_spin_init(&spinlock, 0); }
	~SpinLock() { pthread_spin_destroy(&spinlock); }

	int lock() { return pthread_spin_lock(&spinlock); }
	int trylock() { return pthread_spin_trylock(&spinlock); }
	int unlock() { return pthread_spin_unlock(&spinlock); }

private:
	pthread_spinlock_t spinlock;
};

using namespace std;

template<typename T>
class ObjPool {
public:
	// The internal function object, calculates object(s) that has expired
	class maxIdleObj {
	public:
		maxIdleObj(uint32_t idle): maxIdle(idle) {}
		bool operator ()(const T *item) {
			return (time(NULL) - item->lastUse) > maxIdle;
		}
	private: 
		uint32_t maxIdle;
	};
	
	// constructor: constructs a ObjPool class object.
	// NOTE the 2nd paramenter, noCreate, is used for pools that need not creating objects,
	// for example, a cached queue.
	ObjPool(StrArray *args = nullptr, bool noCreate = false): 
		_maxSize(128), _minSize(4), _noCreate(noCreate) {
		if (nullptr != args) _args = *args;
	}
	
	// Set the allowed value for an object's max time (in minutes) of idle,
	// this will also start a monitor thread, 
	// which will remove those expired objects from the pool.
	void setMaxIdle(uint32_t idle) {
		if (_noCreate) return;
		_maxIdle = (idle > 0) ? idle : 120;
		thread t(clearIdleItems, this);
		t.detach();
	}
	
	// Set the maximum and minimum size of the pool.
	void setSizeLimit(uint32_t maxSize, uint32_t minSize) {
		if (_noCreate) return;
		_maxSize = (maxSize > 0) ? maxSize : 256;
		_minSize = (minSize > 0) ? minSize : 4;

		for (int i = 0; i < _minSize; i++) release(new T(&_args));
	}

	~ObjPool() { clear(); }

	// Grab an object from the pool,
	// return a pointer that pointing to the object, 
	// or nullptr when any fault with error information in paramenter err.
	T *grab(string &err) {
		err = "";
		int n = (_noCreate)? m_Lock.trylock() : m_Lock.lock();
		if (0 == n) {
			if (_pool.size() > 0) {
				auto ret = _pool.front();
				_pool.pop_front();
				m_Lock.unlock();
				return ret;
			}
			m_Lock.unlock();
			//if need not creating new object£¬return 'nullptr' derectly when the pool is empty. 
			//Else, create a new object and return it. 
			return (_noCreate) ? nullptr : (new T(&_args));
		}
		err = (_noCreate && EBUSY == n) ? "" : "grab fail: spin lock fail";
		return nullptr;
	}
	
	// Release an object into the pool,
	// it will be put into the pool,
	// or be destroied when the pool is full. 
	void release(T *item) {
		bool pushed = false;
		item->lastUse = time(NULL);
		if (0 == m_Lock.lock()) {
			if (_pool.size() < _maxSize) {
				// push object at the pool's tail when it's used as cached queue (FIFO)
				// push object at the pool's head when it's used as object pool (LRU)
				(_noCreate) ? _pool.push_back(item) : _pool.push_front(item);
				pushed = true;
			}
			m_Lock.unlock();
			if (!pushed) delete item;
		}
	}

	static void clearIdleItems(ObjPool *pool){
		bool waitHalfIdle = false;
		auto it = pool->_pool.begin();

		while (true) {
			this_thread::sleep_for(chrono::minutes(waitHalfIdle ? pool->_maxIdle / 2 : pool->_maxIdle));

			string err("");
			if (0 == pool->m_Lock.trylock()) {
				if (pool->_pool.size() <= pool->_minSize) {
					for (it =  pool->_pool.begin(); it !=  pool->_pool.end(); it++) {
						(*it)->chkStatus();
					}
				} else while ((it = find_if(begin(pool->_pool), end(pool->_pool), maxIdleObj(pool->_maxIdle * 60))) != end(pool->_pool)) {
					delete *it;
					pool->_pool.erase(it);
				}
				pool->m_Lock.unlock();
				waitHalfIdle = false;
			} else waitHalfIdle = true;
		}
	}

public:
	uint32_t _maxIdle;
	uint32_t _maxSize;
	uint32_t _minSize;

	list<T *> _pool;
	SpinLock m_Lock;
	StrArray _args;

private:
	void clear() {
		while (!_pool.empty()) {
			delete _pool.front();
			_pool.pop_front();
		}
	}

private:
	bool _noCreate;
};

#endif