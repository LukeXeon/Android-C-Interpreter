#include "pool.h"
#include <unordered_set>
#include <mutex>
using namespace std;
struct _pool_t
{
	virtual void * _malloc(size_t s) = 0;
	virtual void * _calloc(size_t s1, size_t s2) = 0;
	virtual void * _realloc(void *block, size_t s) = 0;
	virtual void _free(void *block) = 0;
	virtual ~_pool_t(){}
};

struct _normal_t :public _pool_t
{
	virtual void * _malloc(size_t s)override
	{
		return malloc(s);
	}
	virtual void * _calloc(size_t s1, size_t s2)override
	{
		return calloc(s1, s2);
	}
	virtual void * _realloc(void *block, size_t s)override
	{
		return _realloc(block, s);
	}
	virtual void _free(void *block)override
	{
		free(block);
	}

	virtual ~_normal_t()override
	{
	}
};

struct _manager_t :public _pool_t
{
	mutex _mutex;
	unordered_set<void*> _set;
	virtual void * _malloc(size_t s)override
	{
		void*block = malloc(s);
		if (block != NULL)
		{
			this->_mutex.lock();
			this->_set.emplace(block);
			this->_mutex.unlock();
		}
		return block;
	}

	virtual void * _calloc(size_t s1, size_t s2)override
	{
		void*block = calloc(s1, s2);
		if (block != NULL)
		{
			this->_mutex.lock();
			this->_set.emplace(block);
			this->_mutex.unlock();
		}
		return block;
	}

	virtual void * _realloc(void *block, size_t s)override
	{
		void*newBlock = realloc(block, s);
		if (newBlock != NULL)
		{
			this->_mutex.lock();
			this->_set.erase(block);
			this->_set.emplace(newBlock);
			this->_mutex.unlock();
		}
		return newBlock;
	}

	virtual void _free(void *block)override
	{
		this->_mutex.lock();
		this->_set.erase(block);
		this->_mutex.unlock();
		free(block);
	}

	virtual ~_manager_t()override
	{
		this->_mutex.lock();
		for (auto it = this->_set.begin(); it != this->_set.end(); it++)
		{
			free(*it);
		}
		this->_mutex.unlock();
	}
};

extern "C"
{
	void * PoolMalloc(PoolManager m, size_t s)
	{
		return m->_malloc(s);
	}

	void * PoolCalloc(PoolManager m, size_t s1, size_t s2)
	{
		return m->_calloc(s1, s2);
	}

	void * PoolRealloc(PoolManager m, void *block, size_t s)
	{
		return m->_realloc(block, s);
	}

	void PoolFree(PoolManager m, void *block)
	{
		m->_free(block);
	}

	PoolManager PoolCreate(pool_type mode)
	{
		switch (mode)
		{
		case normal_t:
			return new _normal_t();
		case manager_t:
			return new _manager_t();
		}
		return nullptr;
	}

	void PoolDestroy(PoolManager m)
	{
		delete m;
	}
}