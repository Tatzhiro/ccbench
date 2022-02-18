
#include <ctype.h> //isdigit,
#include <pthread.h>
#include <string.h>      //strlen,
#include <sys/syscall.h> //syscall(SYS_gettid),
#include <sys/types.h>   //syscall(SYS_gettid),
#include <unistd.h>      //syscall(SYS_gettid),
#include <x86intrin.h>

#include <iostream>
#include <string> //string
#include <thread>

#define GLOBAL_VALUE_DEFINE

#include "../include/atomic_wrapper.hh"
#include "../include/backoff.hh"
#include "../include/cpu.hh"
#include "../include/debug.hh"
#include "../include/fence.hh"
#include "../include/int64byte.hh"
#include "../include/masstree_wrapper.hh"
#include "../include/procedure.hh"
#include "../include/random.hh"
#include "../include/result.hh"
#include "../include/tsc.hh"
#include "../include/util.hh"
#include "../include/zipf.hh"
#include "include/common.hh"
#include "include/result.hh"
#include "include/transaction.hh"
#include "include/util.hh"

#define BAMBOO
#define NONTS
// #define PRINTF

#ifndef NONTS
long long int central_timestamp = 0; //*** added by tatsu
#endif

void Tuple::ownersAdd(int txn)
{
  owners.push_back(txn);
}

void worker(size_t thid, char &ready, const bool &start, const bool &quit)
{
  Result &myres = std::ref(SS2PLResult[thid]);
  Xoroshiro128Plus rnd;
  rnd.init();
  TxExecutor trans(thid, (Result *)&myres);
  FastZipf zipf(&rnd, FLAGS_zipf_skew, FLAGS_tuple_num);
  Backoff backoff(FLAGS_clocks_per_us);
  int op_counter;

#if MASSTREE_USE
  MasstreeWrapper<Tuple>::thread_init(int(thid));
#endif

#ifdef Linux
  setThreadAffinity(thid);
  // printf("Thread #%d: on CPU %d\n", *myid, sched_getcpu());
  // printf("sysconf(_SC_NPROCESSORS_CONF) %ld\n",
  // sysconf(_SC_NPROCESSORS_CONF));
#endif // Linux

  storeRelease(ready, 1);
  while (!loadAcquire(start))
    _mm_pause();
  while (!loadAcquire(quit))
  {
    makeProcedure(trans.pro_set_, rnd, zipf, FLAGS_tuple_num, FLAGS_max_ope, FLAGS_thread_num,
                  FLAGS_rratio, FLAGS_rmw, FLAGS_ycsb, false, thid, myres);
  RETRY:
    thread_stats[thid] = 0;
    commit_semaphore[thid] = 0;
#ifdef BAMBOO
    op_counter = 0;
#endif
#ifndef NONTS
    thread_timestamp[thid] = __atomic_add_fetch(&central_timestamp, 1, __ATOMIC_SEQ_CST);
#endif
#ifdef PRINTF
    printf("tx%d starts: timestamp = %d\n", (int)thid, thread_timestamp[thid]);
#endif
    if (loadAcquire(quit))
      break;
    if (thid == 0)
      leaderBackoffWork(backoff, SS2PLResult);

    trans.begin();
    for (auto itr = trans.pro_set_.begin(); itr != trans.pro_set_.end();
         ++itr)
    {
      if ((*itr).ope_ == Ope::READ)
      {
#ifdef PRINTF
        printf("tx%d read tup %d\n", (int)thid, (int)(*itr).key_);
#endif
#ifdef BAMBOO
        op_counter++;
#endif
        trans.read((*itr).key_);
      }
      else if ((*itr).ope_ == Ope::WRITE)
      {
#ifdef PRINTF
        printf("tx%d write tup %d\n", (int)thid, (int)(*itr).key_);
#endif
#ifdef BAMBOO
        op_counter++;
        if (op_counter > (FLAGS_max_ope / 2))
          trans.write((*itr).key_, false);
        else
#endif
          trans.write((*itr).key_, true);
      }
      else if ((*itr).ope_ == Ope::READ_MODIFY_WRITE)
      {
#ifdef BAMBOO
        op_counter++;
        if (op_counter > (FLAGS_max_ope / 2))
          trans.readWrite((*itr).key_, false);
        else
#endif
          trans.readWrite((*itr).key_, true);
      }
      else
      {
        ERR;
      }

      if (thread_stats[thid] == 1 || trans.status_ == TransactionStatus::aborted)
      {
        trans.status_ = TransactionStatus::aborted;
        trans.abort();
        goto RETRY;
      }
    }
#ifdef PRINTF
    int count = 0;
#endif
#ifdef BAMBOO
    while (commit_semaphore[thid] > 0 && thread_stats[thid] == 0)
    {
      usleep(1);
      #ifdef PRINTF
      count++;
      if (count % 1000 == 0)
      {
        printf("TX%d WAITING TOO LONG\n", (int)thid);
        trans.checkLists(0);
      }
      #endif
    }
#endif
    if (thread_stats[thid] == 1 || trans.status_ == TransactionStatus::aborted)
    {
      trans.status_ = TransactionStatus::aborted;
      trans.abort();
      goto RETRY;
    }
    trans.commit();
    /**
     * local_commit_counts is used at ../include/backoff.hh to calcurate about
     * backoff.
     */
    storeRelease(myres.local_commit_counts_,
                 loadAcquire(myres.local_commit_counts_) + 1);
  }

  return;
}

int main(int argc, char *argv[])
try
{
  gflags::SetUsageMessage("2PL benchmark.");
  gflags::ParseCommandLineFlags(&argc, &argv, true);
  chkArg();
  makeDB();
  printf("Bamboo\n");
  for (int i = 0; i < FLAGS_thread_num; i++)
  {
    thread_stats[i] = 0;
    thread_timestamp[i] = 0;
    commit_semaphore[i] = 0;
  }
  alignas(CACHE_LINE_SIZE) bool start = false;
  alignas(CACHE_LINE_SIZE) bool quit = false;
  initResult();
  std::vector<char> readys(FLAGS_thread_num);
  std::vector<std::thread> thv;
  for (size_t i = 0; i < FLAGS_thread_num; ++i)
    thv.emplace_back(worker, i, std::ref(readys[i]), std::ref(start),
                     std::ref(quit));
  waitForReady(readys);
  storeRelease(start, true);
  for (size_t i = 0; i < FLAGS_extime; ++i)
  {
    sleepMs(1000);
  }
  storeRelease(quit, true);
  for (auto &th : thv)
    th.join();

  for (unsigned int i = 0; i < FLAGS_thread_num; ++i)
  {
    SS2PLResult[0].addLocalAllResult(SS2PLResult[i]);
  }
  ShowOptParameters();
  SS2PLResult[0].displayAllResult(FLAGS_clocks_per_us, FLAGS_extime, FLAGS_thread_num);

  return 0;
}
catch (bad_alloc)
{
  printf("bad alloc\n");
  ERR;
}