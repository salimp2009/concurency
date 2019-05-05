//#pragma once
#ifndef _SCOPED_LOCK_H
#define _SCOPE_LOCK_H

template<typename LOCK>
class ScopedLock
{
private:
	typedef LOCK lock_t;
	lock_t* m_plock;
public:
	explicit ScopedLock(lock_t& lock): m_plock{&lock} 
	{
		m_plock->Acquire();
	}

	~ScopedLock()
	{
		m_plock->Release();
	}
};
#endif // !_SCOPED_LOCK_H
