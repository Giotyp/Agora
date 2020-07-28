#include <gtest/gtest.h>
// For some reason, gtest include order matters
#include "concurrentqueue.h"
#include <thread>

static constexpr size_t kNumWorker = 2;
static constexpr size_t kMaxTestNum = (1 << 24);

void run_master(moodycamel::ConcurrentQueue<size_t>** queues,
    moodycamel::ProducerToken** ptoks)
{
    for (size_t i = 0; i < kMaxTestNum; i++) {
        queues[i % kNumWorker]->enqueue(*ptoks[i % kNumWorker], i);
    }
}

void run_worker(size_t worker_id, moodycamel::ConcurrentQueue<size_t>* queue,
    moodycamel::ConsumerToken* ctok)
{
    size_t next_expected = worker_id;
    while (next_expected < kMaxTestNum) {
        size_t item;
        if (queue->try_dequeue(*ctok, item)) {
            ASSERT_EQ(item, next_expected);
            next_expected += kNumWorker;
        }
    }
}

TEST(TestConcurrentQueue, Correctness)
{
    auto** queues = new moodycamel::ConcurrentQueue<size_t>*[kNumWorker];
    auto** ptoks = new moodycamel::ProducerToken*[kNumWorker];
    auto** ctoks = new moodycamel::ConsumerToken*[kNumWorker];
    for (size_t i = 0; i < kNumWorker; i++) {
        queues[i] = new moodycamel::ConcurrentQueue<size_t>;
        ptoks[i] = new moodycamel::ProducerToken(*queues[i]);
        ctoks[i] = new moodycamel::ConsumerToken(*queues[i]);
    }
    auto* master = new std::thread(run_master, queues, ptoks);
    auto** workers = new std::thread*[kNumWorker];
    for (size_t i = 0; i < kNumWorker; i++) {
        workers[i] = new std::thread(run_worker, i, queues[i], ctoks[i]);
    }
    master->join();
    for (size_t i = 0; i < kNumWorker; i++) {
        workers[i]->join();
    }
    delete master;
    for (size_t i = 0; i < kNumWorker; i++) {
        delete workers[i];
        delete queues[i];
        delete ptoks[i];
        delete ctoks[i];
    }
    delete workers;
    delete queues;
    delete ptoks;
    delete ctoks;
}

int main(int argc, char** argv)
{
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}