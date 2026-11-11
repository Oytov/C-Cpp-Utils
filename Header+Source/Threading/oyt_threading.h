/**
 * @file oyt_threading.h
 *
 * OYT_THREADING_NO_MALLOC can be defined at the beginning of oyt_threading.c or in the compilation command.
 * It results in no dynamic memory being used by the library itself.
 * By default dynamic memory is used during thread creation.
 *
 * @warning This library breaks strict aliasing to avoid including OS headers in your project.
 *          I haven't encountered issues but compile with -fno-strict-aliasing to be safe.
 */

#ifndef oyt_threading_h
#define oyt_threading_h

/* C standard */
#if defined(__STDC_VERSION__)
	#define OYTT_C_VERSION __STDC_VERSION__
#else
	#define OYTT_C_VERSION 0
#endif

/* C++ standard */
#if defined(_MSVC_LANG)
	#define OYTT_CPP_VERSION _MSVC_LANG
#elif defined(__cplusplus)
	#define OYTT_CPP_VERSION __cplusplus
#else
	#define OYTT_CPP_VERSION 0
#endif

/* Which compiler */
#define OYTT_ON_GNUCC 0
#define OYTT_ON_MSVC 0
#if defined(__GNUC__) || defined(__clang__)
	#undef OYTT_ON_GNUCC
	#define OYTT_ON_GNUCC 1
#elif defined(_MSC_VER)
	#undef  OYTT_ON_MSVC
	#define OYTT_ON_MSVC 1
#endif

/* Which operating system */
#define OYTT_ON_WINDOWS 0
#define OYTT_ON_POSIX 0
#if defined(_WIN32) || defined(_WIN64)
	#undef  OYTT_ON_WINDOWS
	#define OYTT_ON_WINDOWS 1
#elif defined(__unix__) || defined(__unix) || defined(__APPLE__)
	#undef  OYTT_ON_POSIX
	#define OYTT_ON_POSIX 1
#endif

enum {
	OYT_THREADING_SUCCESS,
	OYT_THREADING_BUSY,
	OYT_THREADING_NO_MEMORY,
	OYT_THREADING_TIMEOUT,
	OYT_THREADING_ERROR
};

#ifdef __cplusplus
extern "C" {
#endif


/*----------------------------Thread Local Storage---------------------------*/

/* Thread-local duration specifier */
#if OYTT_C_VERSION >= 202311L || OYTT_CPP_VERSION >= 201103L
	#define oyt_threadlocal thread_local
#elif OYTT_C_VERSION >= 201112L
	#define oyt_threadlocal _Thread_local
#elif OYTT_ON_GNUCC
	#define oyt_threadlocal __thread
#elif OYTT_ON_MSVC
	#define oyt_threadlocal __declspec(thread)
#else
	#error "Thread-local storage duration specifier not supported"
#endif

#if !(OYTT_ON_WINDOWS || OYTT_ON_POSIX)
	#error "Thread Local Storage API not supported"
#endif

/**
 * @struct Thread Local Storage index.
 */
typedef struct {
#if OYTT_ON_WINDOWS
	unsigned long idx;
#elif OYTT_ON_POSIX
	long storage_;
#endif
} oyt_TlsIndex;

/**
 * @brief Allocates a TLS index.
 *
 * @param[in,out]	pIndex	Memory for a TLS index
 *
 * @return SUCCESS, ERROR.
 */
int oyt_TlsAlloc(oyt_TlsIndex *pIndex);

/**
 * @brief Frees a TLS index.
 *
 * @param[in]	index	TLS index
 */
void oyt_TlsFree(oyt_TlsIndex index);

/**
 * @brief Gets the value stored in the TLS slot associated with the index.
 *
 * @param[in]	index	TLS index
 *
 * @return Stored value.
 */
void *oyt_TlsGet(oyt_TlsIndex index);

/**
 * @brief Stores the value in the TLS slot associated with the index.
 *
 * @param[in]	index	TLS index
 * @param[in]	value	Value to be stored
 *
 * @return SUCCESS, ERROR.
 */
int oyt_TlsSet(oyt_TlsIndex index, void *value);

/*---------------------------------Spinlocks---------------------------------*/

#if !(OYTT_ON_GNUCC || OYTT_ON_MSVC)
	#error "Spinlocks not supported"
#endif

/**
 * @struct Spinlock type
 */
typedef struct {
	volatile char lock;
} oyt_Spinlock;

#define OYT_SPINLOCK_INIT { 0 }

/**
 * @brief Initialize a spinlock.
 *
 * @param[in,out]	pLock	Pointer to a spinlock
 */
void oyt_Spinlock_init(oyt_Spinlock *pLock);

/**
 * @brief Attempts locking.
 *
 * @param[in,out]	pLock	Pointer to a spinlock
 *
 * @return SUCCESS, BUSY.
 */
int oyt_Spinlock_tryLock(oyt_Spinlock *pLock);

/**
 * @brief Blocks the calling thread until the spinlock is locked.
 *
 * @param[in,out]	pLock	Pointer to a spinlock
 */
void oyt_Spinlock_lock(oyt_Spinlock *pLock);

/**
 * @brief Unlocks a spinlock.
 *
 * @param[in,out]	pLock	Pointer to a spinlock
 *
 * @warning Implementation doesn't enforce ownership, any thread can unlock a spinlock.
 */
void oyt_Spinlock_unlock(oyt_Spinlock *pLock);


/*----------------------------------Threads----------------------------------*/

#if !(OYTT_ON_WINDOWS || OYTT_ON_POSIX)
	#error "Threads not supported"
#endif

/**
 * @typedef Thread function type
 */
typedef int (*oyt_ThreadFunction)(void*);

/**
 * @struct Thread type.
 */
typedef struct {
#if OYTT_ON_WINDOWS
	void *handle;
	unsigned id;
#elif OYTT_ON_POSIX
	void *storage_[1];
#endif
} oyt_Thread;

/**
 * @brief Creates a new thread.
 *
 * @param[in,out]	pThread			Pointer to a oyt_Thread
 * @param[in]		threadFunction	Thread entry point/function
 * @param[in]		arg				Argument(s)
 *
 * @return SUCCESS, NO_MEMORY, ERROR.
 */
int oyt_Thread_create(
	oyt_Thread *pThread,
	oyt_ThreadFunction threadFunction,
	void *arg
);

/**
 * @brief Joins a non-detached thread.
 *
 * @param[in]	thread		A valid oyt_Thread
 * @param[out]	pReturn		[Optional] Storage for the thread's return value
 *
 * @return SUCCESS, ERROR.
 */
int oyt_Thread_join(oyt_Thread thread, int *pReturn);

/**
 * @brief Detaches a thread.
 *
 * @param[in]	thread	A valid oyt_Thread
 *
 * @return SUCCESS, ERROR.
 */
int oyt_Thread_detach(oyt_Thread thread);

/**
 * @brief Exits the calling thread.
 *
 * @param[in]	returnCode	Return code
 *
 * @note Can only be called by threads created by this library.
 */
void oyt_Thread_exit(int returnCode);

/**
 * @brief Compares two oyt_Threads.
 *
 * @param[in]	a	A valid oyt_Thread
 * @param[in]	b	A valid oyt_Thread
 *
 * @return Non zero if the threads are the same.
 */
int oyt_Thread_equal(oyt_Thread a, oyt_Thread b);

/**
 * @brief Returns a oyt_Thread for the calling/current thread.
 *
 * @return oyt_Thread.
 *
 * @warning Do not use the returned oyt_Thread in oyt_Thread_join() or oyt_Thread_detach()
 */
oyt_Thread oyt_Thread_getCurrent(void);

/**
 * @brief Sleeps for at least the specified amount.
 *
 * @param[in]	milliseconds	Time in milliseconds
 */
void oyt_sleep(unsigned long milliseconds);

/**
 * @brief Hints to the scheduler to let other threads run.
 */
void oyt_yield(void);


/*----------------------------------Mutexes----------------------------------*/

#if !(OYTT_ON_WINDOWS || OYTT_ON_POSIX)
	#error "Mutexes not supported"
#endif

/**
 * @struct Mutex type.
 */
typedef struct {void* storage_[
#if OYTT_ON_WINDOWS
	7
#elif OYTT_ON_POSIX
	8
#endif
];} oyt_Mutex;

/**
 * @enum Type of mutex.
 */
typedef enum {
	OYT_MUTEX_NORMAL,		/*!< Deadlocks on relock or when trying to relock */
	OYT_MUTEX_RECURSIVE		/*!< The same number of unlocks - as locks - have to be performed */
} oyt_MutexType;

/**
 * @brief Creates a mutex.
 *
 * @param[in,out]	pMutex	Memory for a mutex
 * @param[in]		type	Type of the mutex
 *
 * @return SUCCESS, ERROR.
 */
int oyt_Mutex_create(oyt_Mutex *pMutex, oyt_MutexType type);

/**
 * @brief Destroys a mutex.
 *
 * @param[in,out]	pMutex	Pointer to a mutex
 */
void oyt_Mutex_destroy(oyt_Mutex *pMutex);

/**
 * @brief Attempts to lock a mutex without blocking.
 *
 * @param[in,out]	pMutex	Pointer to a mutex
 *
 * @return SUCCESS, BUSY, ERROR.
 */
int oyt_Mutex_tryLock(oyt_Mutex *pMutex);

/**
 * @brief Blocks on a mutex.
 *
 * @param[in,out]	pMutex	Pointer to a mutex
 *
 * @return SUCCESS, ERROR.
 */
int oyt_Mutex_lock(oyt_Mutex *pMutex);

/**
 * @brief Unlocks a mutex.
 *
 * @param[in,out]	pMutex	Pointer to a mutex
 *
 * @return SUCCESS, ERROR.
 *
 * @note Unlocking a mutex owned by another thread/not owned is undefined behavior.
 */
int oyt_Mutex_unlock(oyt_Mutex *pMutex);


/*----------------------------Condition-Variables----------------------------*/

#if !(OYTT_ON_WINDOWS || OYTT_ON_POSIX)
	#error "Condition variables not supported"
#endif

/**
 * @struct Condition variable type.
 */
typedef struct {
#if OYTT_ON_WINDOWS
	void* storage_[1];
#elif OYTT_ON_POSIX && (defined(__LP64__) || defined(_LP64))
	void* storage_[6];
#elif OYTT_ON_POSIX
	union {
		double alignment_;
		void* ptrs_[12];
	} storage_;
#endif
} oyt_Condition;

/**
 * @brief Creates a condition variable.
 *
 * @param[in,out]	pCondition	Memory for a condition variable
 *
 * @return SUCCESS, ERROR.
 */
int oyt_Condition_create(oyt_Condition *pCondition);

/**
 * @brief Destroys a condition variable.
 *
 * @param[in,out]	pCondition	Pointer to a condition variable
 */
void oyt_Condition_destroy(oyt_Condition *pCondition);

/**
 * @brief Wakes a singular thread waiting on the condition variable.
 *
 * @param[in,out]	pCondition	Pointer to a condition variable
 *
 * @return SUCCESS, ERROR.
 */
int oyt_Condition_wakeOne(oyt_Condition *pCondition);

/**
 * @brief Wakes all threads waiting on the condition variable.
 *
 * @param[in,out]	pCondition	Pointer to a condition variable
 *
 * @return SUCCESS, ERROR.
 */
int oyt_Condition_wakeAll(oyt_Condition *pCondition);

/**
 * @brief Waits on a condition variable, is allowed to spuriously wake up.
 *
 * @param[in,out]	pCondition	Pointer to a condition variable
 * @param[in,out]	pMutex		Pointer to a mutex
 *
 * @return SUCCESS, ERROR.
 *
 * @warning If using a recursive mutex, it must only have a lock count of 1.
 */
int oyt_Condition_wait(oyt_Condition *pCondition, oyt_Mutex *pMutex);

/**
 * @brief Waits on a condition variable with timeout, is allowed to spuriously wake up.
 *
 * @param[in,out]	pCondition		Pointer to a condition variable
 * @param[in,out]	pMutex			Pointer to a mutex
 * @param[in]		milliseconds	Timeout in milliseconds
 *
 * @return SUCCESS, TIMEOUT, ERROR.
 *
 * @warning If using a recursive mutex, it must only have a lock count of 1.
 */
int oyt_Condition_timedWait(
	oyt_Condition *pCondition,
	oyt_Mutex *pMutex,
	unsigned long milliseconds
);


#ifdef __cplusplus
}
#endif

#ifndef OYTT_DONT_UNDEF_PLATFORM_MACROS
	#undef OYTT_ON_POSIX
	#undef OYTT_ON_WINDOWS
	#undef OYTT_ON_MSVC
	#undef OYTT_ON_GNUCC
	#undef OYTT_CPP_VERSION
	#undef OYTT_C_VERSION
#endif

#endif /* oyt_threading_h */
