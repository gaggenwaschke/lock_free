
#include <catch2/catch.hpp>
#include <lock_free/list.hpp>

#include <atomic>
#include <mutex>
#include <thread>
#include <vector>

namespace config {
constexpr std::size_t chunk_size{64};
constexpr std::size_t threads_per_op{16};
constexpr std::size_t operation_iterations{10'000};
} // namespace config

TEST_CASE("lock_free::list single threaded consistency", "[lock_free][list]") {
  lock_free::list<int> list{config::chunk_size};

  for (int ii{0}; ii != config::operation_iterations; ++ii)
    list.emplace(ii);

  for (int ii{config::operation_iterations - 1}; ii != -1; --ii)
    CHECK(*list.pop() == ii);
}

TEST_CASE("lock_free::list thread safety", "[lock_free][list][thread]") {

  std::atomic<bool> start_signal{false};
  lock_free::list<int> list{config::chunk_size};

  const auto worker_wrapper = [&start_signal](auto operation) {
    start_signal.wait(false);
    for (std::size_t ii{0}; ii != config::operation_iterations; ++ii)
      operation();
  };
  const auto operation_emplace = [&list] { list.emplace(1); };
  const auto operation_pop = [&list] { auto value = list.pop(); };
  const auto operation_iterate = [&list] {
    std::size_t cache{};
    for (const auto &value : list)
      cache = value;
  };

  std::vector<std::thread> threads{};
  for (int ii{0}; ii != config::threads_per_op; ++ii) {
    threads.emplace_back(worker_wrapper, operation_emplace);
    threads.emplace_back(worker_wrapper, operation_pop);
    threads.emplace_back(worker_wrapper, operation_iterate);
  }

  start_signal = true;
  start_signal.notify_all();

  for (auto &thread : threads)
    thread.join();
}

TEST_CASE("lock_free::list emplace thread consistency",
          "[lock_free][list][thread]") {
  std::atomic<bool> start_signal{false};
  lock_free::list<int> list{config::chunk_size};

  const auto worker_wrapper = [&start_signal](auto operation) {
    start_signal.wait(false);
    for (std::size_t ii{0}; ii != config::operation_iterations; ++ii)
      operation();
  };
  const auto operation_emplace = [&list] { list.emplace(1); };

  std::vector<std::thread> threads{};
  for (int ii{0}; ii != config::threads_per_op; ++ii) {
    threads.emplace_back(worker_wrapper, operation_emplace);
  }

  start_signal = true;
  start_signal.notify_all();

  for (auto &thread : threads)
    thread.join();

  std::size_t count{0};
  for (const auto &value : list)
    ++count;
  CHECK(count == (config::threads_per_op * config::operation_iterations));
}

TEST_CASE("lock_free::list pop thread consistency",
          "[lock_free][list][thread]") {
  std::atomic<bool> start_signal{false};
  std::atomic<std::size_t> error_count{0};
  lock_free::list<int> list{config::chunk_size};

  // Fill up list with exactly enough elements.
  for (std::size_t ii{0};
       ii != config::threads_per_op * config::operation_iterations; ++ii)
    list.emplace(ii);

  const auto worker_wrapper = [&start_signal](auto operation) {
    start_signal.wait(false);
    for (std::size_t ii{0}; ii != config::operation_iterations; ++ii)
      operation();
  };

  const auto operation_pop = [&error_count, &list] {
    if (list.pop() == list.end())
      // Report error if no more elements are in the list.
      // There should be exactly enough.
      ++error_count;
  };

  std::vector<std::thread> threads{};
  for (int ii{0}; ii != config::threads_per_op; ++ii) {
    threads.emplace_back(worker_wrapper, operation_pop);
  }

  start_signal = true;
  start_signal.notify_all();

  for (auto &thread : threads)
    thread.join();
  CHECK(error_count == 0);

  std::size_t count{0};
  for (const auto &value : list) {
    ++count;
  }
  CHECK(count == 0);
}

TEST_CASE("lock_free::list benchmark", "[lock_free][list][container]") {
  BENCHMARK_ADVANCED("lock_free::list::emplace empty")
  (Catch::Benchmark::Chronometer meter) {
    lock_free::list<std::size_t> list{config::chunk_size};
    meter.measure([&list] { list.emplace(1); });
  };

  BENCHMARK_ADVANCED("lock_free::list::emplace size 1")
  (Catch::Benchmark::Chronometer meter) {
    lock_free::list<std::size_t> list{config::chunk_size};
    list.emplace(1);
    meter.measure([&list] { list.emplace(1); });
  };

  BENCHMARK_ADVANCED("lock_free::list::emplace size 100")
  (Catch::Benchmark::Chronometer meter) {
    lock_free::list<std::size_t> list{config::chunk_size};
    for (std::size_t ii{0}; ii != 100; ++ii)
      list.emplace(ii);
    meter.measure([&list] { list.emplace(1); });
  };

  BENCHMARK_ADVANCED("std::vector::push without lock empty")
  (Catch::Benchmark::Chronometer meter) {
    std::vector<std::size_t> vector{};
    meter.measure([&vector] { vector.push_back(1); });
  };

  BENCHMARK_ADVANCED("std::vector::push without lock size 1")
  (Catch::Benchmark::Chronometer meter) {
    std::vector<std::size_t> vector{};
    vector.push_back(1);
    meter.measure([&vector] { vector.push_back(1); });
  };

  BENCHMARK_ADVANCED("std::vector::push without lock size 100")
  (Catch::Benchmark::Chronometer meter) {
    std::vector<std::size_t> vector{};
    for (std::size_t ii{0}; ii != 100; ++ii)
      vector.push_back(ii);
    meter.measure([&vector] { vector.push_back(1); });
  };

  BENCHMARK_ADVANCED("std::vector::push with lock empty")
  (Catch::Benchmark::Chronometer meter) {
    std::vector<std::size_t> vector{};
    std::mutex mutex{};
    meter.measure([&mutex, &vector] {
      std::unique_lock lock{mutex};
      vector.push_back(1);
    });
  };

  BENCHMARK_ADVANCED("std::vector::push with lock size 1")
  (Catch::Benchmark::Chronometer meter) {
    std::vector<std::size_t> vector{};
    vector.push_back(1);
    std::mutex mutex{};
    meter.measure([&mutex, &vector] {
      std::unique_lock lock{mutex};
      vector.push_back(1);
    });
  };

  BENCHMARK_ADVANCED("std::vector::push with lock size 100")
  (Catch::Benchmark::Chronometer meter) {
    std::vector<std::size_t> vector{};
    for (std::size_t ii{0}; ii != 100; ++ii)
      vector.push_back(ii);
    std::mutex mutex{};
    meter.measure([&mutex, &vector] {
      std::unique_lock lock{mutex};
      vector.push_back(1);
    });
  };
}