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
	virtual long long _size() = 0;
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

	virtual long long _size()
	{
		return 0;
	}

	virtual ~_normal_t()override
	{
	}
};

struct _manager_t :public _pool_t
{
	unordered_set<void*> _set;
	mutex _mutex;
	virtual void * _malloc(size_t s)override
	{
		void*block = malloc(s);
		if (block != NULL)
		{
			_mutex.lock();
			this->_set.emplace(block);
			_mutex.unlock();
		}
		return block;
	}

	virtual void * _calloc(size_t s1, size_t s2)override
	{
		void*block = calloc(s1, s2);
		if (block != NULL)
		{
			_mutex.lock();
			this->_set.emplace(block);
			_mutex.unlock();
		}
		return block;
	}

	virtual void * _realloc(void *block, size_t s)override
	{
		void*newBlock = realloc(block, s);
		if (newBlock != NULL)
		{
			_mutex.lock();
			this->_set.erase(block);
			this->_set.emplace(newBlock);
			_mutex.unlock();
		}
		return newBlock;
	}

	virtual void _free(void *block)override
	{
		_mutex.lock();
		this->_set.erase(block);
		_mutex.unlock();
		free(block);
	}

	virtual long long _size()
	{
		long long result = 0;
		_mutex.lock();
		for (auto it = this->_set.begin(); it != this->_set.end(); it++)
		{
			result += malloc_usable_size(*it);
		}
		_mutex.unlock();
		return result;
	}

	virtual ~_manager_t()override
	{
		_mutex.lock();
		for (auto it = this->_set.begin(); it != this->_set.end(); it++)
		{
			free(*it);
		}
		_mutex.unlock();
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

	long long PoolSize(PoolManager m)
	{
		return m->_size();
	}
}