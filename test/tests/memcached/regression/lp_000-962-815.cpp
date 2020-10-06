#include "test/lib/common.hpp"
#include "test/lib/MemcachedCluster.hpp"

#include "libmemcachedutil-1.0/pool.h"

#include <atomic>
#include <sstream>

static atomic<bool> running = false;

static inline void set_running(bool is) {
  running.store(is, memory_order_release);
}
static inline bool is_running() {
  return running.load(memory_order_consume);
}

struct worker_ctx {
  memcached_pool_st *pool;
  vector<stringstream> errors;

  explicit worker_ctx(memcached_st *memc)
  : pool{memcached_pool_create(memc, 5, 10)}
  , errors{}
  {
  }

  ~worker_ctx() {
    memcached_pool_destroy(pool);
    for (const auto &err : errors) {
      cerr << err.str() << endl;
    }
  }

  stringstream &err() {
    return errors[errors.size()];
  }
};

static void *worker(void *arg) {
  auto *ctx = static_cast<worker_ctx *>(arg);

  while (is_running()) {
    memcached_return_t rc;
    timespec block{5, 0};
    auto *mc = memcached_pool_fetch(ctx->pool, &block, &rc);

    if (!mc || memcached_failed(rc)) {
      ctx->err() << "failed to fetch connection from pool: "
                 << memcached_strerror(nullptr, rc);
      this_thread::sleep_for(100ms);
    }

    auto rs = random_ascii_string(12);
    rc = memcached_set(mc, rs.c_str(), rs.length(), rs.c_str(), rs.length(), 0, 0);
    if (memcached_failed(rc)) {
      ctx->err() << "failed to memcached_set() "
                 << memcached_last_error_message(mc);
    }
    rc = memcached_pool_release(ctx->pool, mc);
    if (memcached_failed(rc)) {
      ctx->err() << "failed to release connection to pool: "
                 << memcached_strerror(nullptr, rc);
    }
  }

  return ctx;
}

TEST_CASE("memcached_regression_lp962815") {
  auto test = MemcachedCluster::mixed();
  auto memc = &test.memc;

  constexpr auto NUM_THREADS = 20;
  array<pthread_t, NUM_THREADS> tid;
  worker_ctx ctx{memc};

  set_running(true);

  for (auto &t : tid) {
    REQUIRE(0 == pthread_create(&t, nullptr, worker, &ctx));
  }

  this_thread::sleep_for(5s);
  set_running(false);

  for (auto t : tid) {
    void *ret = nullptr;
    REQUIRE(0 == pthread_join(t, &ret));
    REQUIRE(ret == &ctx);
  }
}
