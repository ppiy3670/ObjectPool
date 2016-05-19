#include "obj_pool.h"
#include <iostream>
#include <mutex>

using namespace std;

typedef struct _poolable_obj {
	int objValue;
	
	//requred, for periodic removing those objects which has expired
    time_t lastUse;
	
	//requred, for periodic status checking
    bool chkStatus() { return true; }
	
	//requred, for constructing new poolable object
    _poolable_obj(StrArray *args = nullptr): objValue(0) {}

} PoolableObj, *PPoolableObj;

static mutex _mutex;

void threadFunc(int thread_id, ObjPool<PoolableObj> *pool) {
	for (int i = 0; i < 5; i++) {
		string err("");
		auto pObj = pool->grab(err);
		if ("" != err) {
			cout << "grab object fail: " << err << endl;
		} else {
			_mutex.lock();
			cout << "thread(" << thread_id << "): object(" << (unsigned long long)pObj << ") value = " << ++pObj->objValue << endl;
			_mutex.unlock();
			pool->release(pObj);
		}
		
		this_thread::sleep_for(chrono::milliseconds(200));
	}
}

int main() {
	ObjPool<PoolableObj> myObjPool;
	
	myObjPool.setMaxIdle(1);
	myObjPool.setSizeLimit(5, 3);

	thread t1(threadFunc, 1, &myObjPool);
	thread t2(threadFunc, 2, &myObjPool);
	thread t3(threadFunc, 3, &myObjPool);
	thread t4(threadFunc, 4, &myObjPool);
	
	t1.join();
	t2.join();
	t3.join();
	t4.join();
	return 0;
}
