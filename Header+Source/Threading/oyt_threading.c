#define OYTT_DONT_UNDEF_PLATFORM_MACROS
#include "oyt_threading.h"

/* Include OS specific headers */
#if OYTT_ON_WINDOWS
	#define WIN32_LEAN_AND_MEAN
	#define NOMINMAX
	#include <windows.h>
	#include <process.h>
#elif OYTT_ON_POSIX
	#define _POSIX_C_SOURCE 200809L
	#include <pthread.h>
	#include <sched.h>
	#include <time.h>
	#include <errno.h>
#endif

#include <string.h>

/* Portable static assert macro */
#if OYTT_C_VERSION >= 201112L
	#define STATIC_ASSERT(cond_, msg_) _Static_assert(cond_, #msg_);
#elif OYTT_CPP_VERSION >= 201103L
	#define STATIC_ASSERT(cond_, msg_) static_assert(cond_, #msg_);
#else
	#define STATIC_ASSERT(cond_, msg_) typedef char static_assert_##msg_[(cond_) ? 1 : -1];
#endif

/* Portable align-of macro */
#if OYTT_C_VERSION >= 202311L || OYTT_CPP_VERSION >= 201103L
	#define OYTT_ALIGNOF(t_) alignof(t_)
#elif OYTT_C_VERSION >= 201112L
	#define OYTT_ALIGNOF(t_) _Alignof(t_)
#elif OYTT_ON_GNUCC
	#define OYTT_ALIGNOF(t_) __alignof__(t_)
#elif OYTT_ON_MSVC
	#define OYTT_ALIGNOF(t_) __alignof(t_)
#endif

/* Helper to perform static assertion on size of type */
#define STATIC_ASSERT_SIZE(native_t_, lib_t_) \
STATIC_ASSERT( \
	sizeof(native_t_) <= sizeof(lib_t_), \
	lib_t_##_not_large_enough \
)

/* Helper to perform static assertion on alignment of type */
#ifdef OYTT_ALIGNOF
	#define STATIC_ASSERT_ALIGNMENT(native_t_, lib_t_) \
	STATIC_ASSERT( \
		OYTT_ALIGNOF(native_t_) <= OYTT_ALIGNOF(lib_t_), \
		lib_t_##_alignment_not_enough \
	)
#else
	#define STATIC_ASSERT_ALIGNMENT(native_t_, lib_t_)
#endif


/*----------------------------Thread Local Storage---------------------------*/

#if OYTT_ON_WINDOWS
	#define OYTT_TLS_NATIVE_TYPE DWORD
#elif OYTT_ON_POSIX
	#define OYTT_TLS_NATIVE_TYPE pthread_key_t
#endif

STATIC_ASSERT_SIZE(OYTT_TLS_NATIVE_TYPE, oyt_TlsIndex)
STATIC_ASSERT_ALIGNMENT(OYTT_TLS_NATIVE_TYPE, oyt_TlsIndex)

int oyt_TlsAlloc(oyt_TlsIndex *pIndex) {
	memset(pIndex, 0, sizeof(oyt_TlsIndex));
#if OYTT_ON_WINDOWS
	pIndex->idx = TlsAlloc();
	if (pIndex->idx != TLS_OUT_OF_INDEXES) {
		return OYT_THREADING_SUCCESS;
	}
#elif OYTT_ON_POSIX
{
	pthread_key_t key;
	if (pthread_key_create(&key, NULL) == 0) {
		memcpy(pIndex, &key, sizeof(key));
		return OYT_THREADING_SUCCESS;
	}
}
#endif
	return OYT_THREADING_ERROR;
}

void oyt_TlsFree(oyt_TlsIndex index) {
#if OYTT_ON_WINDOWS
	(void)TlsFree(index.idx);
#elif OYTT_ON_POSIX
	pthread_key_t key;
	memcpy(&key, &index, sizeof(key));
	(void)pthread_key_delete(key);
#endif
}

void *oyt_TlsGet(oyt_TlsIndex index) {
#if OYTT_ON_WINDOWS
	return TlsGetValue(index.idx);
#elif OYTT_ON_POSIX
	pthread_key_t key;
	memcpy(&key, &index, sizeof(key));
	return pthread_getspecific(key);
#endif
}

int oyt_TlsSet(oyt_TlsIndex index, void *value) {
#if OYTT_ON_WINDOWS
	if (TlsSetValue(index.idx, value)) {
		return OYT_THREADING_SUCCESS;
	}
#elif OYTT_ON_POSIX
	pthread_key_t key;
	memcpy(&key, &index, sizeof(key));
	if (pthread_setspecific(key, value) == 0) {
		return OYT_THREADING_SUCCESS;
	}
#endif
	return OYT_THREADING_ERROR;
}

/*---------------------------------Spinlocks---------------------------------*/

void oyt_Spinlock_init(oyt_Spinlock *pLock) {
	pLock->lock = 0;
}

int oyt_Spinlock_tryLock(oyt_Spinlock *pLock) {
#if OYTT_ON_MSVC
	if (!InterlockedExchange8(&pLock->lock, 1))
#elif OYTT_ON_GNUCC
	if (!__atomic_exchange_n(&pLock->lock, 1, __ATOMIC_ACQUIRE))
#endif
		return OYT_THREADING_SUCCESS;
	return OYT_THREADING_BUSY;
}

/* x86 PAUSE equivalent */
static void oytt_pauseCPU(void) {
#if OYTT_ON_MSVC
	YieldProcessor();
#elif defined(__i386__) || defined(__x86_64__)
	__builtin_ia32_pause();
#elif defined(__aarch64__) || (defined(__arm__) && defined(__ARM_ARCH) && __ARM_ARCH >= 7)
	__asm__ __volatile__("yield");
#endif
}

/* Relaxed atomic check on the spinlock */
static int oytt_Spinlock_isLocked(oyt_Spinlock *pLock) {
#if OYTT_ON_GNUCC
	return __atomic_load_n(&pLock->lock, __ATOMIC_RELAXED) != 0;
#else
	return pLock->lock != 0;
#endif
}

void oyt_Spinlock_lock(oyt_Spinlock *pLock) {
	#define OYTT_SPIN_LIMIT 128

	while (oyt_Spinlock_tryLock(pLock) == OYT_THREADING_BUSY) {
		int i;
		for (i = 0; i < OYTT_SPIN_LIMIT && oytt_Spinlock_isLocked(pLock); i++)
			oytt_pauseCPU();

		if (i == OYTT_SPIN_LIMIT)
			oyt_yield();
	}
}

void oyt_Spinlock_unlock(oyt_Spinlock *pLock) {
#if OYTT_ON_GNUCC
	__atomic_store_n(&pLock->lock, 0, __ATOMIC_RELEASE);
#elif OYTT_ON_MSVC
	InterlockedExchange8(&pLock->lock, 0);
#endif
}


/*----------------------------------Threads----------------------------------*/

#if OYTT_ON_WINDOWS
	#define OYTT_THREAD_NATIVE_TYPE oyt_Thread
#elif OYTT_ON_POSIX
	#define OYTT_THREAD_NATIVE_TYPE pthread_t
#endif

STATIC_ASSERT_SIZE(OYTT_THREAD_NATIVE_TYPE, oyt_Thread)
STATIC_ASSERT_ALIGNMENT(OYTT_THREAD_NATIVE_TYPE, oyt_Thread)

#if !defined(OYT_THREADING_NO_MALLOC)
	#include <stdlib.h>
#endif

struct oytt_ArgWrapper {
	oyt_ThreadFunction fn;
	void *arg;
#if defined(OYT_THREADING_NO_MALLOC)
	oyt_Spinlock lock;
#endif
};

#if OYTT_ON_WINDOWS
	static unsigned __stdcall oytt_threadEntryWrapper(void *arg) {
		struct oytt_ArgWrapper *pArgs = (struct oytt_ArgWrapper*)arg;
		struct oytt_ArgWrapper args = *pArgs;

	#if defined(OYT_THREADING_NO_MALLOC)
		oyt_Spinlock_unlock(&pArgs->lock);
	#else
		free(arg);
	#endif

		return (unsigned)args.fn(args.arg);
	}
#elif OYTT_ON_POSIX
	static void *oytt_threadEntryWrapper(void *arg) {
		int ret; void *out;
		struct oytt_ArgWrapper *pArgs = (struct oytt_ArgWrapper*)arg;
		struct oytt_ArgWrapper args = *pArgs;

	#if defined(OYT_THREADING_NO_MALLOC)
		oyt_Spinlock_unlock(&pArgs->lock);
	#else
		free(arg);
	#endif

		ret = args.fn(args.arg);
		memset(&out, 0, sizeof(void*));
		memcpy(&out, &ret, sizeof(int));
		return out;
	}
#endif

int oyt_Thread_create(
	oyt_Thread *pThread,
	oyt_ThreadFunction threadFunction,
	void *arg
) {
#if defined(OYT_THREADING_NO_MALLOC)
	struct oytt_ArgWrapper args, *pArgs = &args;
	oyt_Spinlock_init(&args.lock);
	oyt_Spinlock_lock(&args.lock);
#else
	struct oytt_ArgWrapper *pArgs = (struct oytt_ArgWrapper*)malloc(sizeof(struct oytt_ArgWrapper));
	if (pArgs == NULL) return OYT_THREADING_NO_MEMORY;
#endif
	memset(pThread, 0, sizeof(oyt_Thread));
	pArgs->fn = threadFunction;
	pArgs->arg = arg;
#if OYTT_ON_WINDOWS
	pThread->handle = (void*)_beginthreadex(
		NULL, 0,
		oytt_threadEntryWrapper, (void*)pArgs,
		0,
		&pThread->id
	);

	if (!pThread->handle) {
	#if !defined(OYT_THREADING_NO_MALLOC)
		free(pArgs);
	#endif
		return OYT_THREADING_ERROR;
	}
#elif OYTT_ON_POSIX
	{
	pthread_t thread;
	if (pthread_create(&thread, NULL, oytt_threadEntryWrapper, (void*)pArgs)) {
	#if !defined(OYT_THREADING_NO_MALLOC)
		free(pArgs);
	#endif
		return OYT_THREADING_ERROR;
	}
	memcpy(pThread, &thread, sizeof(pthread_t));
	}
#endif
#if defined(OYT_THREADING_NO_MALLOC)
	oyt_Spinlock_lock(&args.lock);
#endif
	return OYT_THREADING_SUCCESS;
}

int oyt_Thread_join(oyt_Thread thread, int *pReturn) {
#if OYTT_ON_WINDOWS
	if (thread.id == GetCurrentThreadId()) {
		SetLastError(ERROR_POSSIBLE_DEADLOCK);
		return OYT_THREADING_ERROR;
	}
	if (WaitForSingleObject(thread.handle, INFINITE) != WAIT_OBJECT_0) {
		CloseHandle(thread.handle);
		return OYT_THREADING_ERROR;
	}

	if (pReturn) {
		DWORD ret;
		if (!GetExitCodeThread(thread.handle, &ret)) {
			CloseHandle(thread.handle);
			return OYT_THREADING_ERROR;
		}

		*pReturn = (int)(unsigned)ret;
	}

	if (!CloseHandle(thread.handle)) {
		return OYT_THREADING_ERROR;
	}
#elif OYTT_ON_POSIX
	void *returnValue;
	pthread_t pthrd;
	memcpy(&pthrd, &thread, sizeof(pthread_t));

	if (pthread_join(pthrd, &returnValue)) {
		return OYT_THREADING_ERROR;
	}
	if (pReturn) {
		memcpy(pReturn, &returnValue, sizeof(int));
	}
#endif

	return OYT_THREADING_SUCCESS;
}

int oyt_Thread_detach(oyt_Thread thread) {
#if OYTT_ON_WINDOWS
	if (!CloseHandle(thread.handle)) {
#elif OYTT_ON_POSIX
	pthread_t pthrd; memcpy(&pthrd, &thread, sizeof(pthread_t));

	if (pthread_detach(pthrd)) {
#endif
		return OYT_THREADING_ERROR;
	}

	return OYT_THREADING_SUCCESS;
}

void oyt_Thread_exit(int returnCode) {
#if OYTT_ON_WINDOWS
	_endthreadex((unsigned)returnCode);
#elif OYTT_ON_POSIX
	void *ret; memset(&ret, 0, sizeof(void*));
	memcpy(&ret, &returnCode, sizeof(int));
	pthread_exit(ret);
#endif
}

int oyt_Thread_equal(oyt_Thread a, oyt_Thread b) {
#if OYTT_ON_WINDOWS
	return a.id == b.id;
#elif OYTT_ON_POSIX
	pthread_t t1, t2;
	memcpy(&t1, &a, sizeof(pthread_t));
	memcpy(&t2, &b, sizeof(pthread_t));
	return pthread_equal(t1, t2);
#endif
}

oyt_Thread oyt_Thread_getCurrent(void) {
	oyt_Thread out;
#if OYTT_ON_WINDOWS
	out.handle = GetCurrentThread();
	out.id = GetCurrentThreadId();
#elif OYTT_ON_POSIX
	const pthread_t thrd = pthread_self();
	memcpy(&out, &thrd, sizeof(pthread_t));
#endif
	return out;
}

void oyt_sleep(unsigned long milliseconds) {
#if OYTT_ON_WINDOWS

	Sleep(milliseconds);
#elif OYTT_ON_POSIX
	struct timespec requested, remaining;
	requested.tv_sec = milliseconds / 1000;
	requested.tv_nsec = (milliseconds % 1000) * 1000000;

	while (nanosleep(&requested, &remaining) == -1) {
		if (errno != EINTR) break;
		requested = remaining;
	}
#endif
}

void oyt_yield(void) {
#if OYTT_ON_WINDOWS
	(void)SwitchToThread();
#elif OYTT_ON_POSIX
	sched_yield();
#endif
}


/*----------------------------------Mutexes----------------------------------*/

#if OYTT_ON_WINDOWS
	/* Internal windows mutex struct */
	typedef struct {
		union {
			SRWLOCK normal;
			CRITICAL_SECTION recursive;
		} mtx;
		oyt_MutexType type;
	} oytt_MutexInternal;

	#define OYTT_MUTEX_NATIVE_TYPE oytt_MutexInternal
#elif OYTT_ON_POSIX
	#define OYTT_MUTEX_NATIVE_TYPE pthread_mutex_t
#endif

STATIC_ASSERT_SIZE(OYTT_MUTEX_NATIVE_TYPE, oyt_Mutex)
STATIC_ASSERT_ALIGNMENT(OYTT_MUTEX_NATIVE_TYPE, oyt_Mutex)


int oyt_Mutex_create(oyt_Mutex *pMutex, oyt_MutexType type) {
	memset(pMutex, 0, sizeof(oyt_Mutex));

	{
#if OYTT_ON_WINDOWS
	oytt_MutexInternal *pMutexInternal = (oytt_MutexInternal*)pMutex;
	pMutexInternal->type = type;

	switch (type) {
	case OYT_MUTEX_NORMAL:
		InitializeSRWLock(&pMutexInternal->mtx.normal);
		return OYT_THREADING_SUCCESS;

	case OYT_MUTEX_RECURSIVE:
		InitializeCriticalSection(&pMutexInternal->mtx.recursive);
		return OYT_THREADING_SUCCESS;

	default:
		return OYT_THREADING_ERROR;
	}
#elif OYTT_ON_POSIX
	int res;
	pthread_mutexattr_t attr;
	if (pthread_mutexattr_init(&attr) != 0)
		return OYT_THREADING_ERROR;

	switch (type) {
	case OYT_MUTEX_NORMAL:
		pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_NORMAL);
		break;

	case OYT_MUTEX_RECURSIVE:
		pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);
		break;

	default:
		(void)pthread_mutexattr_destroy(&attr);
		return OYT_THREADING_ERROR;
	}

	res = pthread_mutex_init((pthread_mutex_t*)pMutex, &attr);
	(void)pthread_mutexattr_destroy(&attr);

	return res == 0 ? OYT_THREADING_SUCCESS : OYT_THREADING_ERROR;
#endif
	}
}

void oyt_Mutex_destroy(oyt_Mutex *pMutex) {
#if OYTT_ON_WINDOWS
	oytt_MutexInternal *pMutexInternal = (oytt_MutexInternal*)pMutex;
	if (pMutexInternal->type == OYT_MUTEX_RECURSIVE)
		DeleteCriticalSection(&pMutexInternal->mtx.recursive);
#elif OYTT_ON_POSIX
	pthread_mutex_destroy((pthread_mutex_t*)pMutex);
#endif
}

int oyt_Mutex_tryLock(oyt_Mutex *pMutex) {
#if OYTT_ON_WINDOWS
	oytt_MutexInternal *pMutexInternal = (oytt_MutexInternal*)pMutex;

	switch (pMutexInternal->type) {
	case OYT_MUTEX_NORMAL:
		if (TryAcquireSRWLockExclusive(&pMutexInternal->mtx.normal))
			return OYT_THREADING_SUCCESS;
		return OYT_THREADING_BUSY;

	case OYT_MUTEX_RECURSIVE:
		if (TryEnterCriticalSection(&pMutexInternal->mtx.recursive))
			return OYT_THREADING_SUCCESS;
		return OYT_THREADING_BUSY;

	default:
		return OYT_THREADING_ERROR;
	}
#elif OYTT_ON_POSIX
	const int res = pthread_mutex_trylock((pthread_mutex_t*)pMutex);
	if (res == 0)
		return OYT_THREADING_SUCCESS;
	else if (res == EBUSY)
		return OYT_THREADING_BUSY;
	else
		return OYT_THREADING_ERROR;
#endif
}

int oyt_Mutex_lock(oyt_Mutex *pMutex) {
#if OYTT_ON_WINDOWS
	oytt_MutexInternal *pMutexInternal = (oytt_MutexInternal*)pMutex;

	switch (pMutexInternal->type) {
	case OYT_MUTEX_NORMAL:
		AcquireSRWLockExclusive(&pMutexInternal->mtx.normal);
		return OYT_THREADING_SUCCESS;

	case OYT_MUTEX_RECURSIVE:
		EnterCriticalSection(&pMutexInternal->mtx.recursive);
		return OYT_THREADING_SUCCESS;

	default:
		return OYT_THREADING_ERROR;
	}
#elif OYTT_ON_POSIX
	if (pthread_mutex_lock((pthread_mutex_t*)pMutex) == 0)
		return OYT_THREADING_SUCCESS;
	else
		return OYT_THREADING_ERROR;
#endif
}

int oyt_Mutex_unlock(oyt_Mutex *pMutex) {
#if OYTT_ON_WINDOWS
	oytt_MutexInternal *pMutexInternal = (oytt_MutexInternal*)pMutex;

	switch (pMutexInternal->type) {
	case OYT_MUTEX_NORMAL:
		ReleaseSRWLockExclusive(&pMutexInternal->mtx.normal);
		return OYT_THREADING_SUCCESS;

	case OYT_MUTEX_RECURSIVE:
		LeaveCriticalSection(&pMutexInternal->mtx.recursive);
		return OYT_THREADING_SUCCESS;

	default:
		return OYT_THREADING_ERROR;
	}
#elif OYTT_ON_POSIX
	if (pthread_mutex_unlock((pthread_mutex_t*)pMutex) == 0)
		return OYT_THREADING_SUCCESS;
	else
		return OYT_THREADING_ERROR;
#endif
}


/*----------------------------Condition-Variables----------------------------*/

#if OYTT_ON_WINDOWS
	#define OYTT_CONDITION_NATIVE_TYPE CONDITION_VARIABLE
#elif OYTT_ON_POSIX
	#define OYTT_CONDITION_NATIVE_TYPE pthread_cond_t
#endif

STATIC_ASSERT_SIZE(OYTT_CONDITION_NATIVE_TYPE, oyt_Condition)
STATIC_ASSERT_ALIGNMENT(OYTT_CONDITION_NATIVE_TYPE, oyt_Condition)


int oyt_Condition_create(oyt_Condition *pCondition) {
	memset(pCondition, 0, sizeof(oyt_Condition));
#if OYTT_ON_WINDOWS
	InitializeConditionVariable((PCONDITION_VARIABLE)pCondition);
	return OYT_THREADING_SUCCESS;
#elif OYTT_ON_POSIX
	if (pthread_cond_init((pthread_cond_t*)pCondition, NULL) == 0)
		return OYT_THREADING_SUCCESS;
	return OYT_THREADING_ERROR;
#endif
}

void oyt_Condition_destroy(oyt_Condition *pCondition) {
#if OYTT_ON_WINDOWS
	(void)pCondition;
#elif OYTT_ON_POSIX
	(void)pthread_cond_destroy((pthread_cond_t*)pCondition);
#endif
}

int oyt_Condition_wakeOne(oyt_Condition *pCondition) {
#if OYTT_ON_WINDOWS
	WakeConditionVariable((PCONDITION_VARIABLE)pCondition);
	return OYT_THREADING_SUCCESS;
#elif OYTT_ON_POSIX
	if (pthread_cond_signal((pthread_cond_t*)pCondition) == 0)
		return OYT_THREADING_SUCCESS;
	return OYT_THREADING_ERROR;
#endif
}

int oyt_Condition_wakeAll(oyt_Condition *pCondition) {
#if OYTT_ON_WINDOWS
	WakeAllConditionVariable((PCONDITION_VARIABLE)pCondition);
	return OYT_THREADING_SUCCESS;
#elif OYTT_ON_POSIX
	if (pthread_cond_broadcast((pthread_cond_t*)pCondition) == 0)
		return OYT_THREADING_SUCCESS;
	return OYT_THREADING_ERROR;
#endif
}

#if OYTT_ON_WINDOWS
static BOOL oytt_waitOnConditionVariable(
	PCONDITION_VARIABLE pCondition,
	oytt_MutexInternal *pMutexInternal,
	DWORD dwMs
) {
	switch (pMutexInternal->type) {
	case OYT_MUTEX_NORMAL:
		return SleepConditionVariableSRW(
			(PCONDITION_VARIABLE)pCondition,
			&pMutexInternal->mtx.normal,
			dwMs, 0
		);
	case OYT_MUTEX_RECURSIVE:
		return SleepConditionVariableCS(
			(PCONDITION_VARIABLE)pCondition,
			&pMutexInternal->mtx.recursive,
			dwMs
		);
	}
	return FALSE;
}
#endif

int oyt_Condition_wait(oyt_Condition *pCondition, oyt_Mutex *pMutex) {
#if OYTT_ON_WINDOWS
	oytt_MutexInternal *pMutexInternal = (oytt_MutexInternal*)pMutex;

	switch (pMutexInternal->type) {
	case OYT_MUTEX_NORMAL:
	case OYT_MUTEX_RECURSIVE:
		if (oytt_waitOnConditionVariable((PCONDITION_VARIABLE)pCondition, pMutexInternal, INFINITE))
			return OYT_THREADING_SUCCESS;
	}
	return OYT_THREADING_ERROR;
#elif OYTT_ON_POSIX
	if (pthread_cond_wait((pthread_cond_t*)pCondition, (pthread_mutex_t*)pMutex) == 0)
		return OYT_THREADING_SUCCESS;
	return OYT_THREADING_ERROR;
#endif
}

int oyt_Condition_timedWait(
	oyt_Condition *pCondition,
	oyt_Mutex *pMutex,
	unsigned long milliseconds
) {
#if OYTT_ON_WINDOWS
	oytt_MutexInternal *pMutexInternal = (oytt_MutexInternal*)pMutex;

	switch (pMutexInternal->type) {
	case OYT_MUTEX_NORMAL:
	case OYT_MUTEX_RECURSIVE:
		SetLastError(ERROR_SUCCESS);
		if (oytt_waitOnConditionVariable((PCONDITION_VARIABLE)pCondition, pMutexInternal, (DWORD)milliseconds))
			return OYT_THREADING_SUCCESS;
		else if (GetLastError() == ERROR_TIMEOUT)
			return OYT_THREADING_TIMEOUT;
	}
	return OYT_THREADING_ERROR;
#elif OYTT_ON_POSIX
	int res;
	struct timespec abstime;
	clock_gettime(CLOCK_REALTIME, &abstime);
	abstime.tv_sec  += milliseconds / 1000;
	abstime.tv_nsec += (long)(milliseconds % 1000) * 1000000L;
	if (abstime.tv_nsec >= 1000000000L) {
		abstime.tv_sec++;
		abstime.tv_nsec -= 1000000000L;
	}

	res = pthread_cond_timedwait((pthread_cond_t*)pCondition, (pthread_mutex_t*)pMutex, &abstime);
	if (res == 0)
		return OYT_THREADING_SUCCESS;
	else if (res == ETIMEDOUT)
		return OYT_THREADING_TIMEOUT;
	return OYT_THREADING_ERROR;
#endif
}
