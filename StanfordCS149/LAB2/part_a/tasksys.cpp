#include "tasksys.h"
#include "../common/CycleTimer.h"
#include <cassert>

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


const char* TaskSystemParallelThreadPoolSpinning::name() {
    return "Parallel + Thread Pool + Spin";
}

TaskSystemParallelThreadPoolSpinning::TaskSystemParallelThreadPoolSpinning(int num_threads): ITaskSystem(num_threads),
    runnable_(nullptr), stop_(false), task_done_(0) {
    //
    // TODO: CS149 student implementations may decide to perform setup
    // operations (such as thread pool construction) here.
    // Implementations are free to add new class member variables
    // (requiring changes to tasksys.h).
    //
    threads_.reserve(num_threads);
    for (int i = 0; i < num_threads; ++i) {
       threads_.emplace_back([&]() {
           while (!stop_) {
            int task_index = -1;
            {
                std::lock_guard<std::mutex> lk(lk_);
                if (!task_queue_.empty()) {
                    task_index = task_queue_.front();
                    task_queue_.pop();
                }
            }
            if (task_index != -1) {
                runnable_->runTask(task_index, num_total_tasks_);
                ++task_done_;
            }
       }});
    }
}

TaskSystemParallelThreadPoolSpinning::~TaskSystemParallelThreadPoolSpinning() {
    stop_ = true;
    for (auto& thread : threads_) {
        thread.join();
    }
}

void TaskSystemParallelThreadPoolSpinning::run(IRunnable* runnable, int num_total_tasks) {
    
    //
    // TODO: CS149 students will modify the implementation of this
    // method in Part A.  The implementation provided below runs all
    // tasks sequentially on the calling thread.
    //
    num_total_tasks_ = num_total_tasks;
    runnable_ = runnable;
    task_done_ = 0;
    {
        std::lock_guard<std::mutex> lk(lk_);
        assert(task_queue_.empty());
        for (int i = 0; i < num_total_tasks; ++i) {
            task_queue_.push(i);
        }
    }
    while (task_done_ != num_total_tasks);
    //why do we need this while loop? 
    // once this goes out of scope
    // the destructor will be called
    // and done will become true
    // and the threads will be joined while there is still some work to do
    // which means the solution won't pass correctness check
}

TaskID TaskSystemParallelThreadPoolSpinning::runAsyncWithDeps(IRunnable* runnable, int num_total_tasks,
                                                              const std::vector<TaskID>& deps) {
    return 0;
}

void TaskSystemParallelThreadPoolSpinning::sync() {
    return;
}

/*
 * ================================================================
 * Parallel Thread Pool Sleeping Task System Implementation
 * ================================================================
 */

// the implementation i decided on is: 
// to use the condition variable on checking the emptiness of the queue, 
// once it's not empty it'll notify all threads and the worker threads will be notified 
// and will lock pop then unlock and start processing which means no concurrency problems 
// since we are giving up the first queue element to the firs thread that wakes up 

const char* TaskSystemParallelThreadPoolSleeping::name() {
    return "Parallel + Thread Pool + Sleep";
}


void TaskSystemParallelThreadPoolSleeping::signal_fn() 
{
    thread_state_->mutex_->lock();
    while(!stop_) 
    {
        if(!task_queue_.empty()) {
            thread_state_->mutex_->unlock();
            thread_state_->condition_variable_->notify_all();
            thread_state_->mutex_->lock();
        }
    }
    thread_state_->mutex_->unlock();
}

void TaskSystemParallelThreadPoolSleeping::wait_fn()
{
    while(!stop_)
    {
        std::unique_lock<std::mutex> lk(*thread_state_->mutex_);
        thread_state_->condition_variable_->wait(lk);
        auto task_index = task_queue_.front();
        task_queue_.pop();
        lk.unlock();
        //runtask here
        runnable_->runTask(task_index, num_total_tasks_);
        std::cout << "Thread " << std::this_thread::get_id() 
                  << " completed task " << task_index << std::endl;
        ++task_done_;
    }
    
}

TaskSystemParallelThreadPoolSleeping::TaskSystemParallelThreadPoolSleeping(int num_threads): ITaskSystem(num_threads) {
    //
    // TODO: CS149 student implementations may decide to perform setup
    // operations (such as thread pool construction) here.
    // Implementations are free to add new class member variables
    // (requiring changes to tasksys.h).
    //
    thread_state_ = new ThreadState(num_threads - 1);
    threads_.reserve(num_threads);

    threads_[0]=std::thread(&TaskSystemParallelThreadPoolSleeping::signal_fn,this);

    for (int i = 1; i < num_threads; ++i) {
        threads_.emplace_back(std::thread(&TaskSystemParallelThreadPoolSleeping::wait_fn,this));
    }
}

TaskSystemParallelThreadPoolSleeping::~TaskSystemParallelThreadPoolSleeping() {
    //
    // TODO: CS149 student implementations may decide to perform cleanup
    // operations (such as thread pool shutdown construction) here.
    // Implementations are free to add new class member variables
    // (requiring changes to tasksys.h).
    //
    stop_ = true;
    for (auto& thread : threads_) {
        thread.join();
    }
    threads_.clear();
    delete thread_state_;
}

void TaskSystemParallelThreadPoolSleeping::run(IRunnable* runnable, int num_total_tasks) {


    //
    // TODO: CS149 students will modify the implementation of this
    // method in Parts A and B.  The implementation provided below runs all
    // tasks sequentially on the calling thread.
    //

    num_total_tasks_ = num_total_tasks;
    runnable_ = runnable;
    task_done_ = 0;
    {
        std::lock_guard<std::mutex> lk(lk_);
        assert(task_queue_.empty());
        for (int i = 0; i < num_total_tasks; ++i) {
            task_queue_.push(i);
        }
    }
    while (task_done_ != num_total_tasks);
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
