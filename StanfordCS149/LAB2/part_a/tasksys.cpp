#include "tasksys.h"
#include "../common/CycleTimer.h"
#include <iostream>
IRunnable::~IRunnable() {}  
ITaskSystem::ITaskSystem(int num_threads) {}
ITaskSystem::~ITaskSystem() {}
/*
 * ================================================================
 * Serial task system implementation
 * ================================================================
 */
    
const char* TaskSystemSerial::name() {
    return "Serial";
}

TaskSystemSerial::TaskSystemSerial(int num_threads): ITaskSystem(num_threads) {
}

TaskSystemSerial::~TaskSystemSerial() {}

void TaskSystemSerial::run(IRunnable* runnable, int num_total_tasks) {
    for (int i = 0; i < num_total_tasks; i++) {
        runnable->runTask(i, num_total_tasks);
    }
}

TaskID TaskSystemSerial::runAsyncWithDeps(IRunnable* runnable, int num_total_tasks,
                                          const std::vector<TaskID>& deps) {
    return 0;
}

void TaskSystemSerial::sync() {
    return;
}

/*
 * ================================================================
 * Parallel Task System Implementation
 * ================================================================
 */

const char* TaskSystemParallelSpawn::name() {
    return "Parallel + Always Spawn";
}

void runThreadDynamic(IRunnable* runnable, int num_total_tasks, std::atomic<int>* curr_task_id)
{
    while (true) {
        int task_id = curr_task_id->fetch_add(1, std::memory_order_relaxed);
        if (task_id >= num_total_tasks) break;
        runnable->runTask(task_id, num_total_tasks);
    }
}

void runThreadStatic(IRunnable* runnable, int first_task_id, int curr_thread_tasks, int num_total_tasks)
{
    const int last_task_id = first_task_id + curr_thread_tasks;
    for(int task_id = first_task_id; task_id < last_task_id; ++task_id) {
        runnable->runTask(task_id, num_total_tasks);
    }
}

TaskSystemParallelSpawn::TaskSystemParallelSpawn(int num_threads): ITaskSystem(num_threads) {
    this->pool_count = num_threads;
}

TaskSystemParallelSpawn::~TaskSystemParallelSpawn() 
{
}
// this function spawns already defined number of threads when it gets a runnable , 
// but has 2 ways of assigning work to threads :
// static <= (if dynamic will incurr a lots of scheduling overhead)
// dynamic <= (when there is a risk of load imbalance, which is not the case here but i'll implement it anyway )
void TaskSystemParallelSpawn::run(IRunnable* runnable, int num_total_tasks) {

    constexpr int RATIO_THRESHOLD = 2;
    if ((num_total_tasks / this->pool_count) < RATIO_THRESHOLD) { // Dynamic assignment
        std::atomic<int> curr_task_id{0};

        for (int i = 0; i < this->pool_count; i++) {
            this->threads.push_back(std::thread(runThreadDynamic, runnable,num_total_tasks, &curr_task_id));
        }
    }
    else { // Static assignment
        const int tasks_per_thread = num_total_tasks / this->pool_count;
        const int remaining_tasks = num_total_tasks % this->pool_count;

        int first_task = 0;
        for (int i = 0; i < this->pool_count; i++) {
            const int curr_thread_tasks = tasks_per_thread + (i < remaining_tasks ? 1 : 0);
            this->threads.push_back(std::thread(runThreadStatic, runnable, first_task, curr_thread_tasks, num_total_tasks));
            first_task += curr_thread_tasks;
        }

    }
    for (auto& t : this->threads) {
        t.join();
    }
    this->threads.clear();
}




TaskID TaskSystemParallelSpawn::runAsyncWithDeps(IRunnable* runnable, int num_total_tasks,
                                                 const std::vector<TaskID>& deps) {
    // You do not need to implement this method.
    return 0;
}

void TaskSystemParallelSpawn::sync() {
    // You do not need to implement this method.
    return;
}







/*
 * ================================================================
 * Parallel Thread Pool Spinning Task System Implementation
 * ================================================================
 */


void TaskSystemParallelThreadPoolSpinning::spin()
{
    while (true) {
        IRunnable* task;
        int i, num_tasks;

        lock.lock();
        if (task_queue.empty()) {
            // ✅ If done is true AND queue is empty, it's time to exit
            if (done) {
                lock.unlock();
                break;
            }
            lock.unlock();
            //std::cout<<"queue empty , discovered by thread  "<<std::this_thread::get_id()<<"\n";
            //yield
            std::this_thread::yield(); // Yield to allow other threads to run
            continue; // Check the queue again
        } else {
            auto pair = task_queue.front();
            task_queue.pop();
            lock.unlock();
            task = pair.first;
            auto x = pair.second;
            i = x.first;
            num_tasks = x.second;

            // if(i%32==0) 
            // {
            //     std::cout<<"Thread "<<std::this_thread::get_id()<<" running task "<<i<<" of "<<num_tasks<<"\n";
            // }

            task->runTask(i, num_tasks);
            tasks_done++;
        }
    }
}

const char* TaskSystemParallelThreadPoolSpinning::name() {
    return "Parallel + Thread Pool + Spin";
}

// Constructor
TaskSystemParallelThreadPoolSpinning::TaskSystemParallelThreadPoolSpinning(int num_threads): ITaskSystem(num_threads)
{
    TaskSystemParallelThreadPoolSpinning::threads.reserve(num_threads);
    done = false;
    tasks_done = 0;
    for(int i = 0; i < num_threads; i++) 
    {
        threads.push_back(std::thread(&TaskSystemParallelThreadPoolSpinning::spin, this));
    }
}

TaskSystemParallelThreadPoolSpinning::~TaskSystemParallelThreadPoolSpinning() 
{
    // Wait for all threads to finish
    done = true; // Signal threads to exit
    for (auto& t : threads) {
        if (t.joinable()) {
            t.join();
        }
    }
    threads.clear();
}

// Run a task with specified number of total tasks
void TaskSystemParallelThreadPoolSpinning::run(IRunnable* runnable, int num_total_tasks) 
{
    //we will lock the whole scope
    lock.lock();
    tasks_done = 0; // Reset tasks_done
    done = false; // Reset done flag
    for(int i = 0; i < num_total_tasks; ++i)
    {
        task_queue.push(std::make_pair(runnable, std::make_pair(i, num_total_tasks)));
    }
    lock.unlock();
    //the joining of the threads is done in the destructor
    while (tasks_done.load(std::memory_order_relaxed) < num_total_tasks)
    {
        std::this_thread::yield(); // so the main thread doesn’t hog CPU
    }
    done = true; // ✅ Tell threads to exit cleanly
}


// Asynchronous task submission (not used in this version)
TaskID TaskSystemParallelThreadPoolSpinning::runAsyncWithDeps(IRunnable* runnable, int num_total_tasks,
                                                              const std::vector<TaskID>& deps) {
    return 0; // Not implemented
}

void TaskSystemParallelThreadPoolSpinning::sync() {
    // You do not need to implement this method.
    return;
}

/*
 * ================================================================
 * Parallel Thread Pool Sleeping Task System Implementation
 * ================================================================
 */

const char* TaskSystemParallelThreadPoolSleeping::name() {
    return "Parallel + Thread Pool + Sleep";
}

TaskSystemParallelThreadPoolSleeping::TaskSystemParallelThreadPoolSleeping(int num_threads): ITaskSystem(num_threads) {
    //
    // TODO: CS149 student implementations may decide to perform setup
    // operations (such as thread pool construction) here.
    // Implementations are free to add new class member variables
    // (requiring changes to tasksys.h).
    //
}

TaskSystemParallelThreadPoolSleeping::~TaskSystemParallelThreadPoolSleeping() {
    //
    // TODO: CS149 student implementations may decide to perform cleanup
    // operations (such as thread pool shutdown construction) here.
    // Implementations are free to add new class member variables
    // (requiring changes to tasksys.h).
    //
}

void TaskSystemParallelThreadPoolSleeping::run(IRunnable* runnable, int num_total_tasks) {


    //
    // TODO: CS149 students will modify the implementation of this
    // method in Parts A and B.  The implementation provided below runs all
    // tasks sequentially on the calling thread.
    //

    for (int i = 0; i < num_total_tasks; i++) {
        runnable->runTask(i, num_total_tasks);
    }
}

TaskID TaskSystemParallelThreadPoolSleeping::runAsyncWithDeps(IRunnable* runnable, int num_total_tasks,
                                                    const std::vector<TaskID>& deps) {


    //
    // TODO: CS149 students will implement this method in Part B.
    //

    return 0;
}

void TaskSystemParallelThreadPoolSleeping::sync() {

    //
    // TODO: CS149 students will modify the implementation of this method in Part B.
    //

    return;
}
