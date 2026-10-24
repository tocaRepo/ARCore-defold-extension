#ifndef DEFOLD_ARCORE_WORKER_H
#define DEFOLD_ARCORE_WORKER_H
#include <dmsdk/dlib/android.h>
#include <dmsdk/dlib/log.h>
#include <thread>
#include <mutex>
#include <condition_variable>

// Synchronous handoff, one uninterrupted attachment. Caller serialization also
// covers lifecycle jobs. The only thread that may use the JNIEnv is Run().
class ArWorker {
public:
    typedef void (*Job)(JNIEnv*, void*);
    ArWorker() : queued(false), completed(false), quitting(false), attached(false), job(0), argument(0) {}
    ~ArWorker() { Close(); }
    bool Invoke(Job next, void* data) {
        std::lock_guard<std::mutex> caller(caller_mutex);
        if (!thread.joinable()) { quitting = false; thread = std::thread(&ArWorker::Run, this); }
        return Execute(next, data);
    }
    void Close(Job cleanup = 0, void* data = 0) {
        std::lock_guard<std::mutex> caller(caller_mutex);
        if (!thread.joinable()) return;
        if (cleanup) Execute(cleanup, data);
        { std::lock_guard<std::mutex> lock(job_mutex); quitting = true; job_ready.notify_one(); }
        thread.join();
    }
private:
    ArWorker(const ArWorker&);
    ArWorker& operator=(const ArWorker&);
    bool Execute(Job next, void* data) {
        std::unique_lock<std::mutex> lock(job_mutex);
        job = next; argument = data; completed = false; queued = true;
        job_ready.notify_one();
        job_done.wait(lock, [this] { return completed; });
        return attached;
    }
    void Run() {
        dmAndroid::ThreadAttacher attachment;
        JNIEnv* env = attachment.GetEnv();
        if (env) dmLogInfo("ARCore worker attached to Java (persistent JNI lifetime)");
        std::unique_lock<std::mutex> lock(job_mutex);
        attached = env != 0;
        for (;;) {
            job_ready.wait(lock, [this] { return queued || quitting; });
            if (quitting) break;
            Job current = job; void* arg = argument; queued = false;
            lock.unlock();
            if (env) current(env, arg);
            lock.lock(); completed = true; job_done.notify_one();
        }
        // Close's cleanup runs first. ThreadAttacher detaches after lock release.
    }
    std::thread thread;
    std::mutex caller_mutex, job_mutex;
    std::condition_variable job_ready, job_done;
    bool queued, completed, quitting, attached;
    Job job;
    void* argument;
};
#endif
