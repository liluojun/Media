//
// Created by admin on 2018/3/30.
//

#include "../include/Looper.h"

#include <jni.h>
#include <pthread.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <semaphore.h>
#include "../../common/include/native_log.h"

struct LooperMessage;
typedef struct LooperMessage LooperMessage;



/**
 * 线程执行句柄
 * @param p
 * @return
 */
void* Looper::trampoline(void* p) {
    ((Looper*)p)->loop();
    return NULL;
}


Looper::Looper() {
    head = NULL;

    sem_init(&headdataavailable, 0, 0);
    sem_init(&headwriteprotect, 0, 1);
    pthread_attr_t attr;
    pthread_attr_init(&attr);

    pthread_create(&worker, &attr, trampoline, this);
    running = true;
}

Looper::~Looper() {
    if (running) {
        LOGD("Looper deleted while still running. Some messages will not be processed");
        quit();
    }
}

/**
 * 入队消息
 * @param what
 * @param flush
 */
void Looper::postMessage(int what, bool flush) {
    postMessage(what, 0, 0, NULL, flush);
}

/**
 * 入队消息
 * @param what
 * @param obj
 * @param flush
 */
void Looper::postMessage(int what, void *obj, bool flush) {
    postMessage(what, 0, 0, obj, flush);
}

/**
 * 入队消息
 * @param what
 * @param arg1
 * @param arg2
 * @param flush
 */
void Looper::postMessage(int what, int arg1, int arg2, bool flush) {
    postMessage(what, arg1, arg2, NULL, flush);
}

/**
 *
 * @param what
 * @param arg1
 * @param arg2
 * @param obj
 * @param flush
 */
void Looper::postMessage(int what, int arg1, int arg2, void *obj, bool flush) {
    LooperMessage *msg = new LooperMessage();
    msg->what = what;
    msg->obj = obj;
    msg->arg1 = arg1;
    msg->arg2 = arg2;
    msg->next = NULL;
    msg->quit = false;
    addMessage(msg, flush);
}

/**
 * 添加消息
 * @param msg
 * @param flush
 */
void Looper::addMessage(LooperMessage *msg, bool flush) {
    sem_wait(&headwriteprotect);
    LooperMessage *h = head;

    if (flush) {
        while(h) {
            LooperMessage *next = h->next;
            delete h;
            h = next;
        }
        h = NULL;
    }
    if (h) {
        while (h->next) {
            h = h->next;
        }
        h->next = msg;
    } else {
        head = msg;
    }
    //LOGD("post msg %d", msg->what);
    sem_post(&headwriteprotect);
    sem_post(&headdataavailable);
}

/**
 * 循环体
 */
void Looper::loop() {
    while(true) {
        // wait for available message
        sem_wait(&headdataavailable);

        // get next available message
        sem_wait(&headwriteprotect);
        LooperMessage *msg = head;
        if (msg == NULL) {
            sem_post(&headwriteprotect);
            continue;
        }
        head = msg->next;
        sem_post(&headwriteprotect);

        if (msg->quit) {
            delete msg;
            return;
        }
        handleMessage(msg);
        delete msg;
    }
}

/**
 * 退出Looper循环
 */
void Looper::quit() {
    LooperMessage *msg = new LooperMessage();
    msg->what = 0;
    msg->obj = NULL;
    msg->next = NULL;
    msg->quit = true;
    addMessage(msg, false);
    void *retval;
    pthread_join(worker, &retval);
    sem_destroy(&headdataavailable);
    sem_destroy(&headwriteprotect);
    running = false;
}

/**
 * 处理消息
 * @param what
 * @param data
 */
void Looper::handleMessage(LooperMessage *msg) {
    LOGD("dropping msg %d %p", msg->what, msg->obj);
}
/*// Cross-platform Looper using std::thread and std::mutex
// Supports safe message posting and FIFO processing

#include <thread>
#include <mutex>
#include <condition_variable>
#include <queue>
#include <atomic>
#include <functional>
#include <memory>

struct LooperMessage {
    int what;
    int arg1;
    int arg2;
    void* obj;

    LooperMessage(int w, int a1 = 0, int a2 = 0, void* o = nullptr)
        : what(w), arg1(a1), arg2(a2), obj(o) {}
};

class Looper {
public:
    using MessageHandler = std::function<void(std::shared_ptr<LooperMessage>)>;

    explicit Looper(MessageHandler handler)
        : handler(handler), running(true), worker(&Looper::loop, this) {}

    ~Looper() {
        quit();
    }

    void postMessage(int what, int arg1 = 0, int arg2 = 0, void* obj = nullptr, bool flush = false) {
        std::lock_guard<std::mutex> lock(mutex);
        if (!running) return;

        if (flush) {
            while (!queue.empty()) queue.pop();
        }

        queue.push(std::make_shared<LooperMessage>(what, arg1, arg2, obj));
        cond.notify_one();
    }

    void quit() {
        {
            std::lock_guard<std::mutex> lock(mutex);
            running = false;
            cond.notify_all();
        }
        if (worker.joinable()) {
            worker.join();
        }
    }

private:
    void loop() {
        while (true) {
            std::shared_ptr<LooperMessage> msg;
            {
                std::unique_lock<std::mutex> lock(mutex);
                cond.wait(lock, [&] { return !queue.empty() || !running; });
                if (!running && queue.empty()) break;
                msg = queue.front();
                queue.pop();
            }
            if (handler && msg) {
                handler(msg);
            }
        }
    }

    std::mutex mutex;
    std::condition_variable cond;
    std::queue<std::shared_ptr<LooperMessage>> queue;
    std::thread worker;
    std::atomic<bool> running;
    MessageHandler handler;
};

// Usage Example:
// Looper looper([](std::shared_ptr<LooperMessage> msg) {
//     std::cout << "Message: " << msg->what << std::endl;
// });
// looper.postMessage(1);
// looper.quit();
*/