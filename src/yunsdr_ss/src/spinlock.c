#include <assert.h>
#include <stdbool.h>

#if defined(__WINDOWS_) || defined(_WIN32)
#   include <windows.h>
#else
#   include <pthread.h>
#endif

#if defined(__WINDOWS_) || defined(_WIN32)
static CRITICAL_SECTION m_criticalSection;
#else
static pthread_spinlock_t m_spinlock;
#endif

void spinlock_init()
{
#if defined(__WINDOWS_) || defined(_WIN32)
    InitializeCriticalSection(&m_criticalSection);
#else
    int rs = pthread_spin_init(&m_spinlock, PTHREAD_PROCESS_PRIVATE);
    assert(0 == rs);
#endif
}

void spinlock_deinit()
{
#if defined(__WINDOWS_) || defined(_WIN32)
    DeleteCriticalSection(&m_criticalSection);
#else
    int rs = pthread_spin_destroy(&m_spinlock);
    assert(0 == rs);
#endif
}

#if defined(__WINDOWS_) || defined(_WIN32)
CRITICAL_SECTION* innerMutex() { return &m_criticalSection; }
#else
pthread_spinlock_t* innerMutex() { return &m_spinlock; }
#endif

void lock()
{
#if defined(__WINDOWS_) || defined(_WIN32)
    EnterCriticalSection(&m_criticalSection);
#else
    int rs = pthread_spin_lock(&m_spinlock);
    assert(0 == rs);
#endif
}

bool trylock()
{
#if defined(__WINDOWS_) || defined(_WIN32)
    return TRUE == TryEnterCriticalSection(&m_criticalSection);
#else
    return 0 == pthread_spin_trylock(&m_spinlock);
#endif
}

void unlock()
{
#if defined(__WINDOWS_) || defined(_WIN32)
    LeaveCriticalSection(&m_criticalSection);
#else
    int rs = pthread_spin_unlock(&m_spinlock);
    assert(0 == rs);
#endif
}

