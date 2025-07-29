#ifndef _TASKSYS_H
#define _TASKSYS_H
#include "itasksys.h"

/*
 * TaskSystemSerial: This class is the student's implementation of a
 * serial task execution engine.  See definition of ITaskSystem in
 * itasksys.h for documentation of the ITaskSystem interface.
 */
class TaskSystemSerial: public ITaskSystem {
    public:
        TaskSystemSerial(int num_threads);
        ~TaskSystemSerial();
        const char* name();
        void run(IRunnable* runnable, int num_total_tasks);
        TaskID runAsyncWithDeps(IRunnable* runnable, int num_total_tasks,
                                const std::vector<TaskID>& deps);
        void sync();
};

/*
 * TaskSystemParallelSpawn: This class is the student's implementation of a
 * parallel task execution engine that spawns threads in every run()
 * call.  See definition of ITaskSystem in itasksys.h for documentation
 * of the ITaskSystem interface.
 */
class TaskSystemParallelSpawn: public ITaskSystem {
    public:
        TaskSystemParallelSpawn(int num_threads);
        ~TaskSystemParallelSpawn();
        const char* name();

        std::atomic<int> pool_count;
        std::vector<std::thread> threads; //threads are declared but not defined => don't qualify as thread pool
        const int RATIO_THRESHOLD = 10;  
        /* if (tasks_in_bulk/num_threads >= ratio) 
        => static execution (heuristic approach , but it works)
        */
        void run(IRunnable* runnable, int num_total_tasks);
        TaskID runAsyncWithDeps(IRunnable* runnable, int num_total_tasks,
                                const std::vector<TaskID>& deps);
        void sync();
};

/*
 * TaskSystemParallelThreadPoolSpinning: This class is the student's
 * implementation of a parallel task execution engine that uses a
 * thread pool. See definition of ITaskSystem in itasksys.h for
 * documentation of the ITaskSystem interface.
 */

class TaskSystemParallelThreadPoolSpinning: public ITaskSystem {
    public:
        TaskSystemParallelThreadPoolSpinning(int num_threads);
        ~TaskSystemParallelThreadPoolSpinning();
        const char* name();
        void run(IRunnable* runnable, int num_total_tasks);
        TaskID runAsyncWithDeps(IRunnable* runnable, int num_total_tasks,
                                const std::vector<TaskID>& deps);
        void sync();
private:
    std::vector<std::thread> threads_;
    int num_total_tasks_;
    IRunnable *runnable_;
    std::queue<int> task_queue_;
    std::mutex lk_;
    bool stop_;
    std::atomic<int> task_done_;
};




/*
 * TaskSystemParallelThreadPoolSleeping: This class is the student's
 * optimized implementation of a parallel task execution engine that uses
 * a thread pool. See definition of ITaskSystem in
 * itasksys.h for documentation of the ITaskSystem interface.
 */

//this class is from tutorial.cpp , imma use it here for making sleeping threads
class ThreadState {
    public:
        std::condition_variable* condition_variable_;
        std::mutex* mutex_;
        std::atomic<bool>* start_;
        int counter_;
        int num_waiting_threads_;
        ThreadState(int num_waiting_threads) {
            condition_variable_ = new std::condition_variable();
            mutex_ = new std::mutex();
            counter_ = 0;
            num_waiting_threads_ = num_waiting_threads;
            start_ = new std::atomic<bool>(false);
        }
        ~ThreadState() {
            delete condition_variable_;
            delete mutex_;
            delete start_;
        }
};

class TaskSystemParallelThreadPoolSleeping: public ITaskSystem {
    public:
        TaskSystemParallelThreadPoolSleeping(int num_threads);
        ~TaskSystemParallelThreadPoolSleeping();
        const char* name();
        void run(IRunnable* runnable, int num_total_tasks);
        TaskID runAsyncWithDeps(IRunnable* runnable, int num_total_tasks,
                                const std::vector<TaskID>& deps);
        void sync();
        void signal_fn() ;
        void wait_fn() ;

    
        std::vector<std::thread> threads_;
        int num_total_tasks_;
        IRunnable *runnable_;
        std::queue<int> task_queue_;
        std::mutex lk_;
        bool stop_;
        std::atomic<int> task_done_;
        ThreadState* thread_state_; // for sleeping threads
};

#endif
