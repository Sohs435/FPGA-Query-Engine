#include <algorithm>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <limits>
#include <numeric>
#include <optional>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

#include "fqe/column.hpp"
#include "fqe/parser.hpp"
#include "fqe/query_binder.hpp"
#include "fqe/query_engine.hpp"
#include "fqe/query_executor.hpp"
#include "fqe/schema.hpp"
#include "fqe/table.hpp"
#include "fqe/tokenizer.hpp"

namespace {

    using Clock = std::chrono::steady_clock;

    constexpr std::size_t minimum_iterations = 5;
    constexpr std::size_t maximum_iterations = 1000;
    constexpr std::size_t target_rows_per_benchmark = 5'000'000;

    volatile std::uint64_t benchmark_sink = 0;

    class ScopedTimer {

        public:
            // Starts timing when the timer object enters scope.
            explicit ScopedTimer(double& elapsed_milliseconds)
                : elapsed_milliseconds_(elapsed_milliseconds), start_(Clock::now()) {}

            // Stops timing when the timer object leaves scope and stores the elapsed milliseconds.
            ~ScopedTimer() {
                const auto end = Clock::now();
                elapsed_milliseconds_ = std::chrono::duration<double, std::milli>(end - start_).count();
            }

            // Prevents accidental timer copies that could write the same elapsed-time output twice.
            ScopedTimer(const ScopedTimer&) = delete;
            ScopedTimer& operator=(const ScopedTimer&) = delete;

        private:
            double& elapsed_milliseconds_;
            Clock::time_point start_;
    };

    struct WorkloadDefinition {
        std::string name;
        std::string sql;
        std::size_t logical_bytes_per_row;
        bool measure_end_to_end;
    };

    struct PreparedWorkload {
        std::string name;
        std::string sql;
        std::size_t logical_bytes_per_row;
        bool measure_end_to_end;
        fqe::BoundQuery bound_query;
    };

    struct BenchmarkStatistics {
        double minimum_milliseconds;
        double median_milliseconds;
        double average_milliseconds;
        double million_rows_per_second;
        double logical_gigabytes_per_second;
        double nanoseconds_per_row;
    };

    // Consumes a scalar result outside the timed region so the compiler cannot remove a direct CPU kernel.
    void consume_scalar_result(std::int64_t value) {
        benchmark_sink += static_cast<std::uint64_t>(value);
    }

    // Consumes the shape and boundary values of a query result so the query remains observable to the compiler.
    void consume_query_result(const fqe::QueryResult& result) {
        std::uint64_t value = static_cast<std::uint64_t>(result.row_count());
        value += static_cast<std::uint64_t>(result.column_count()) << 32;

        for (std::size_t column_index = 0; column_index < result.column_count(); column_index++) {
            const fqe::Column& column = result.column(column_index);

            std::visit(
                [&] (const auto& values) {
                    if (!values.empty()) {
                        value += static_cast<std::uint64_t>(values.front());
                        value += static_cast<std::uint64_t>(values.back());
                    }
                },
                column.data()
            );
        }

        benchmark_sink += value;
    }

    // Adds two input columns and reduces the generated values into one sum without using the query engine.
    std::int64_t direct_addition_sum(const std::vector<std::int32_t>& prices,
        const std::vector<std::int32_t>& quantities) {

        if (prices.size() != quantities.size()) {
            throw std::invalid_argument("Direct addition columns must have matching sizes");
        }

        std::int64_t total = 0;

        for (std::size_t row_index = 0; row_index < prices.size(); row_index++) {
            total += static_cast<std::int64_t>(prices[row_index]) + static_cast<std::int64_t>(quantities[row_index]);
        }

        return total;
    }

    // Multiplies two input columns and reduces the generated values into one sum without using the query engine.
    std::int64_t direct_multiplication_sum(const std::vector<std::int32_t>& prices,
        const std::vector<std::int32_t>& quantities) {

        if (prices.size() != quantities.size()) {
            throw std::invalid_argument("Direct multiplication columns must have matching sizes");
        }

        std::int64_t total = 0;

        for (std::size_t row_index = 0; row_index < prices.size(); row_index++) {
            total += static_cast<std::int64_t>(prices[row_index]) * static_cast<std::int64_t>(quantities[row_index]);
        }

        return total;
    }

    // Creates deterministic columns whose value patterns give known filter selectivities and group cardinalities.
    fqe::Table create_benchmark_table(std::size_t row_count) {
        if (row_count == 0) {
            throw std::invalid_argument("Benchmark row count must be greater than zero");
        }

        std::vector<std::int32_t> prices(row_count);
        std::vector<std::int32_t> quantities(row_count);
        std::vector<std::int32_t> instruments_10(row_count);
        std::vector<std::int32_t> instruments_1000(row_count);
        std::vector<std::int32_t> instruments_100000(row_count);
        std::vector<std::int64_t> timestamps(row_count);

        for (std::size_t row_index = 0; row_index < row_count; row_index++) {
            prices[row_index] = static_cast<std::int32_t>(500 + row_index % 2000);
            quantities[row_index] = static_cast<std::int32_t>(row_index % 1000);
            instruments_10[row_index] = static_cast<std::int32_t>(row_index % 10);
            instruments_1000[row_index] = static_cast<std::int32_t>(row_index % 1000);
            instruments_100000[row_index] = static_cast<std::int32_t>(row_index % 100000);
            timestamps[row_index] = std::int64_t{5'000'000'000} + static_cast<std::int64_t>(row_index);
        }

        fqe::Schema schema({
            {"price", fqe::DataType::Int32},
            {"quantity", fqe::DataType::Int32},
            {"instrument10", fqe::DataType::Int32},
            {"instrument1000", fqe::DataType::Int32},
            {"instrument100000", fqe::DataType::Int32},
            {"timestamp", fqe::DataType::Int64}
        });

        std::vector<fqe::Column> columns;
        columns.reserve(6);
        columns.emplace_back(std::move(prices));
        columns.emplace_back(std::move(quantities));
        columns.emplace_back(std::move(instruments_10));
        columns.emplace_back(std::move(instruments_1000));
        columns.emplace_back(std::move(instruments_100000));
        columns.emplace_back(std::move(timestamps));

        return fqe::Table(std::move(schema), std::move(columns));
    }

    // Defines the SQL workloads used to compare scans, filters, arithmetic, projection and grouping.
    std::vector<WorkloadDefinition> create_workload_definitions() {
        return {
            {"Column scan + COUNT", "SELECT COUNT(price) FROM trades;", 4, false},
            {"Filter 100% + COUNT", "SELECT COUNT(*) FROM trades WHERE quantity >= 0;", 4, false},
            {"Filter 50% + COUNT", "SELECT COUNT(*) FROM trades WHERE quantity >= 500;", 4, true},
            {"Filter 10% + COUNT", "SELECT COUNT(*) FROM trades WHERE quantity >= 900;", 4, false},
            {"Column scan + SUM", "SELECT SUM(price) FROM trades;", 4, false},
            {"Addition + SUM", "SELECT SUM(price + quantity) FROM trades;", 8, false},
            {"Multiplication + SUM", "SELECT SUM(price * quantity) FROM trades;", 8, false},
            {"Filter 50% + SUM", "SELECT SUM(price) FROM trades WHERE quantity >= 500;", 8, false},
            {"Filtered multiply + SUM", "SELECT SUM(price * quantity) FROM trades WHERE quantity >= 500;", 12, true},
            {"Projection 50%", "SELECT price, quantity FROM trades WHERE quantity >= 500;", 12, true},
            {"GROUP BY 10", "SELECT instrument10, COUNT(*), SUM(price) FROM trades GROUP BY instrument10;", 8, false},
            {"GROUP BY 1000", "SELECT instrument1000, COUNT(*), SUM(price) FROM trades GROUP BY instrument1000;", 8, true},
            {"GROUP BY 100000", "SELECT instrument100000, COUNT(*), SUM(price) FROM trades GROUP BY instrument100000;", 8, false}
        };
    }

    // Tokenizes, parses and binds one SQL string into the representation consumed by execute_query().
    fqe::BoundQuery prepare_query(const std::string& sql, const fqe::Schema& schema) {
        fqe::Tokenizer tokenizer(sql);
        fqe::Parser parser(tokenizer.tokenize());
        fqe::ParsedQuery parsed_query = parser.parse_query();

        return fqe::bind_query(parsed_query, schema);
    }

    // Prepares every SQL workload before timing so execution-only measurements exclude parsing and binding.
    std::vector<PreparedWorkload> prepare_workloads(const fqe::Schema& schema) {
        const std::vector<WorkloadDefinition> definitions = create_workload_definitions();
        std::vector<PreparedWorkload> workloads;
        workloads.reserve(definitions.size());

        for (const WorkloadDefinition& definition : definitions) {
            workloads.push_back(PreparedWorkload{
                definition.name,
                definition.sql,
                definition.logical_bytes_per_row,
                definition.measure_end_to_end,
                prepare_query(definition.sql, schema)
            });
        }

        return workloads;
    }

    // Chooses more repetitions for small datasets while guaranteeing at least five samples for large datasets.
    std::size_t calculate_iterations(std::size_t row_count) {
        const std::size_t calculated_iterations = target_rows_per_benchmark / row_count;
        return std::clamp(calculated_iterations, minimum_iterations, maximum_iterations);
    }

    // Uses fewer warm-ups for large datasets because a single large pass already exercises the complete operation.
    std::size_t calculate_warmup_iterations(std::size_t row_count) {
        return row_count >= 10'000'000 ? 1 : 3;
    }

    // Repeats any result-producing operation, consumes each result outside the timer and returns median-based throughput.
    template <typename Operation, typename Consumer>
    BenchmarkStatistics run_benchmark(std::size_t row_count, std::size_t logical_bytes_per_row,
        std::size_t iterations, std::size_t warmup_iterations, Operation&& operation, Consumer&& consumer) {

        using Result = std::decay_t<decltype(operation())>;

        for (std::size_t iteration = 0; iteration < warmup_iterations; iteration++) {
            Result result = operation();
            consumer(result);
        }

        std::vector<double> timings;
        timings.reserve(iterations);

        for (std::size_t iteration = 0; iteration < iterations; iteration++) {
            double elapsed_milliseconds = 0.0;
            std::optional<Result> result;

            {
                ScopedTimer timer(elapsed_milliseconds);
                result.emplace(operation());
            }

            consumer(result.value());
            timings.push_back(elapsed_milliseconds);
        }

        const double total_milliseconds = std::accumulate(timings.begin(), timings.end(), 0.0);
        const double average_milliseconds = total_milliseconds / static_cast<double>(iterations);

        std::sort(timings.begin(), timings.end());

        double median_milliseconds;

        if (iterations % 2 == 0) {
            median_milliseconds = (timings[iterations / 2 - 1] + timings[iterations / 2]) / 2.0;
        }

        else {
            median_milliseconds = timings[iterations / 2];
        }

        const double median_seconds = median_milliseconds / 1000.0;
        const double rows = static_cast<double>(row_count);
        const double rows_per_second = rows / median_seconds;
        const double logical_bytes = rows * static_cast<double>(logical_bytes_per_row);

        return BenchmarkStatistics{
            timings.front(),
            median_milliseconds,
            average_milliseconds,
            rows_per_second / 1.0e6,
            logical_bytes / median_seconds / 1.0e9,
            median_milliseconds * 1.0e6 / rows
        };
    }

    // Prints the column names shared by execution-only, direct CPU and end-to-end measurements.
    void print_table_header() {
        std::cout
            << std::left
            << std::setw(18) << "Mode"
            << std::setw(30) << "Workload"
            << std::right
            << std::setw(12) << "Min ms"
            << std::setw(12) << "Median ms"
            << std::setw(12) << "Average ms"
            << std::setw(12) << "M rows/s"
            << std::setw(14) << "Logical GB/s"
            << std::setw(12) << "ns/row"
            << '\n';
    }

    // Prints one benchmark result using the same fixed-width layout as the rest of the benchmark table.
    void print_statistics(const std::string& mode, const std::string& workload_name,
        const BenchmarkStatistics& statistics) {

        std::cout
            << std::left
            << std::setw(18) << mode
            << std::setw(30) << workload_name
            << std::right
            << std::fixed
            << std::setprecision(3)
            << std::setw(12) << statistics.minimum_milliseconds
            << std::setw(12) << statistics.median_milliseconds
            << std::setw(12) << statistics.average_milliseconds
            << std::setw(12) << statistics.million_rows_per_second
            << std::setw(14) << statistics.logical_gigabytes_per_second
            << std::setw(12) << statistics.nanoseconds_per_row
            << '\n';
    }

    // Reports how much slower multiplication is than addition when both process the same two input columns.
    void print_multiplication_comparison(const std::string& mode, std::size_t row_count,
        const BenchmarkStatistics& addition, const BenchmarkStatistics& multiplication) {

        const double delta_milliseconds = multiplication.median_milliseconds - addition.median_milliseconds;
        const double delta_nanoseconds_per_row = delta_milliseconds * 1.0e6 / static_cast<double>(row_count);

        std::cout << mode << " multiplication minus addition: "
            << std::fixed << std::setprecision(3) << delta_milliseconds << " ms | "
            << delta_nanoseconds_per_row << " ns/row\n";
    }

    // Builds one synthetic table and benchmarks every workload against that same immutable input data.
    void benchmark_dataset(std::size_t row_count) {
        const double input_mebibytes = static_cast<double>(row_count) * 28.0 / (1024.0 * 1024.0);
        const std::size_t iterations = calculate_iterations(row_count);
        const std::size_t warmup_iterations = calculate_warmup_iterations(row_count);

        std::cout << "\nRows: " << row_count
            << " | Approximate input: " << std::fixed << std::setprecision(2) << input_mebibytes << " MiB"
            << " | Iterations: " << iterations
            << " | Warm-ups: " << warmup_iterations << '\n';

        fqe::Table table = create_benchmark_table(row_count);
        std::vector<PreparedWorkload> workloads = prepare_workloads(table.schema());

        const std::size_t price_index = table.schema().index_of("price");
        const std::size_t quantity_index = table.schema().index_of("quantity");

        const auto& prices = std::get<std::vector<std::int32_t>>(table.column(price_index).data());
        const auto& quantities = std::get<std::vector<std::int32_t>>(table.column(quantity_index).data());

        print_table_header();

        std::optional<BenchmarkStatistics> engine_addition_statistics;
        std::optional<BenchmarkStatistics> engine_multiplication_statistics;

        for (const PreparedWorkload& workload : workloads) {
            const BenchmarkStatistics statistics = run_benchmark(
                row_count, workload.logical_bytes_per_row, iterations, warmup_iterations,
                [&] () { return fqe::execute_query(table, workload.bound_query); },
                [] (const fqe::QueryResult& result) { consume_query_result(result); }
            );

            print_statistics("Execution only", workload.name, statistics);

            if (workload.name == "Addition + SUM") {
                engine_addition_statistics = statistics;
            }

            if (workload.name == "Multiplication + SUM") {
                engine_multiplication_statistics = statistics;
            }
        }

        const BenchmarkStatistics direct_addition_statistics = run_benchmark(
            row_count, 8, iterations, warmup_iterations,
            [&] () { return direct_addition_sum(prices, quantities); },
            [] (std::int64_t result) { consume_scalar_result(result); }
        );

        print_statistics("Direct CPU", "Addition + SUM", direct_addition_statistics);

        const BenchmarkStatistics direct_multiplication_statistics = run_benchmark(
            row_count, 8, iterations, warmup_iterations,
            [&] () { return direct_multiplication_sum(prices, quantities); },
            [] (std::int64_t result) { consume_scalar_result(result); }
        );

        print_statistics("Direct CPU", "Multiplication + SUM", direct_multiplication_statistics);

        if (engine_addition_statistics.has_value() && engine_multiplication_statistics.has_value()) {
            print_multiplication_comparison("Query engine", row_count, engine_addition_statistics.value(),
                engine_multiplication_statistics.value());
        }

        print_multiplication_comparison("Direct CPU", row_count, direct_addition_statistics,
            direct_multiplication_statistics);

        fqe::QueryEngine query_engine;
        query_engine.add_table("trades", std::move(table));

        for (const PreparedWorkload& workload : workloads) {
            if (!workload.measure_end_to_end) {
                continue;
            }

            const BenchmarkStatistics statistics = run_benchmark(
                row_count, workload.logical_bytes_per_row, iterations, warmup_iterations,
                [&] () { return query_engine.execute(workload.sql); },
                [] (const fqe::QueryResult& result) { consume_query_result(result); }
            );

            print_statistics("End to end", workload.name, statistics);
        }
    }

    // Parses optional command-line row counts or returns the default progression from one thousand to ten million rows.
    std::vector<std::size_t> parse_row_counts(int argument_count, char* arguments[]) {
        if (argument_count == 1) {
            return {1'000, 10'000, 100'000, 1'000'000, 10'000'000};
        }

        std::vector<std::size_t> row_counts;
        row_counts.reserve(static_cast<std::size_t>(argument_count - 1));

        for (int argument_index = 1; argument_index < argument_count; argument_index++) {
            const std::string argument = arguments[argument_index];
            std::size_t consumed_characters = 0;
            const unsigned long long parsed_value = std::stoull(argument, &consumed_characters);

            if (consumed_characters != argument.size() || parsed_value == 0 ||
                parsed_value > std::numeric_limits<std::size_t>::max()) {
                throw std::invalid_argument("Invalid benchmark row count: " + argument);
            }

            row_counts.push_back(static_cast<std::size_t>(parsed_value));
        }

        return row_counts;
    }

}

// Runs every requested dataset size and reports an error if table generation or query execution fails.
int main(int argument_count, char* arguments[]) {
    try {
        const std::vector<std::size_t> row_counts = parse_row_counts(argument_count, arguments);

        std::cout << "CPU query-engine benchmark\n";

#ifdef NDEBUG
        std::cout << "Build mode: optimized release\n";
#else
        std::cout << "WARNING: NDEBUG is not defined\n";
#endif

        for (std::size_t row_count : row_counts) {
            benchmark_dataset(row_count);
        }

        std::cout << "\nBenchmark sink: " << static_cast<std::uint64_t>(benchmark_sink) << '\n';

        return EXIT_SUCCESS;
    }

    catch (const std::exception& error) {
        std::cerr << "Benchmark failure: " << error.what() << '\n';
        return EXIT_FAILURE;
    }
}