#pragma once
// Types related to the std library

#include <mutex>
//#include <shared_mutex>

#define DISABLE_COPY(T) \
	T(const T &) = delete; \
	T &operator=(const T &) = delete;

#define ASSERT_FORCED(cond, msg) \
	if (!(cond)) { throw std::runtime_error(msg); }

typedef std::unique_lock<std::mutex> SimpleLock;
//typedef std::unique_lock<std::shared_mutex> WriteLock;
//typedef std::shared_lock<std::shared_mutex> ReadLock;

typedef uint32_t peer_t; // same as in ENetPeer

void sleep_ms(long delay);

// Auto-unlock wrapper for larger operations
template<typename T>
class PtrLock {
public:
	// Mutex be locked before!
	PtrLock(std::mutex &m, T *ptr) :
		m_mutex(m), m_ptr(ptr)
	{
		if (!ptr)
			m.unlock();
	}

	~PtrLock()
	{
		if (m_ptr)
			m_mutex.unlock();
	}

	DISABLE_COPY(PtrLock)

	void release()
	{
		if (m_ptr)
			m_mutex.unlock();
		m_ptr = nullptr;
	}

	inline T *ptr() const { return m_ptr; }
	// Synthetic sugar
	inline T *operator->() const { return m_ptr; }
	// Validation check
	inline bool operator!() const { return !m_ptr; }

private:
	std::mutex &m_mutex;
	T *m_ptr;
};
