
#include <catch2/catch.hpp>
#include <lock_free/list.hpp>

#include <atomic>
#include <mutex>
#include <thread>
#include <vector>

TEST_CASE("lock_free::list thread safety", "[lock_free][list][thread]") {
  constexpr static std::size_t thread_per_op{16};
  constexpr std::size_t operation_iterations{10'000};
  std::atomic<bool> start_signal{false};
  lock_free::list<int> list{32};

  const auto worker_wrapper = [operation_iterations,
                               &start_signal](auto operation) {
    start_signal.wait(false);
    for (std::size_t ii{0}; ii != operation_iterations; ++ii)
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
  for (int ii{0}; ii != thread_per_op; ++ii) {
    threads.emplace_back(worker_wrapper, operation_emplace);
    threads.emplace_back(worker_wrapper, operation_pop);
    threads.emplace_back(worker_wrapper, operation_iterate);
  }

  start_signal = true;
  start_signal.notify_all();

  for (auto &thread : threads)
    thread.join();
}

TEST_CASE("lock_free::list benchmark", "[lock_free][list][container]") {
  constexpr static std::size_t chunk_size{1'024};

  BENCHMARK_ADVANCED("lock_free::list::emplace empty")
  (Catch::Benchmark::Chronometer meter) {
    lock_free::list<std::size_t> list{chunk_size};
    meter.measure([&list] { list.emplace(1); });
  };

  BENCHMARK_ADVANCED("lock_free::list::emplace size 1")
  (Catch::Benchmark::Chronometer meter) {
    lock_free::list<std::size_t> list{chunk_size};
    list.emplace(1);
    meter.measure([&list] { list.emplace(1); });
  };

  BENCHMARK_ADVANCED("lock_free::list::emplace size 100")
  (Catch::Benchmark::Chronometer meter) {
    lock_free::list<std::size_t> list{chunk_size};
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