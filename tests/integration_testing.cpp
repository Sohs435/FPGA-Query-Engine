#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <exception>
#include <iostream>
#include <stdexcept>
#include <string>
#include <utility>
#include <variant>
#include <vector>

#include "fqe/csv_loader.hpp"
#include "fqe/query_engine.hpp"
#include "fqe/schema.hpp"

namespace {

    void expect(bool condition, const std::string& test_name) {

        if (!condition) {
            throw std::runtime_error(
                test_name + ": FAILED");
        }

        std::cout << test_name << ": PASSED\n";
    }

    std::int64_t result_value(const fqe::QueryResult& result,
        std::size_t column_index, std::size_t row_index) {

        const fqe::Column& column =
            result.column(column_index);

        return std::visit(
            [row_index] (const auto& values) -> std::int64_t {
                return static_cast<std::int64_t>(
                    values.at(row_index));
            },

            column.data()
        );
    }

    std::size_t find_group_row(const fqe::QueryResult& result,
        std::int64_t instrument) {

        for (std::size_t row_index = 0;
             row_index < result.row_count(); row_index++) {

            if (result_value(result, 0, row_index) == instrument) {
                return row_index;
            }
        }

        return result.row_count();
    }

}

int main() {

    try {
        fqe::Schema trades({
            {"price", fqe::DataType::Int32},
            {"quantity", fqe::DataType::Int32},
            {"instrument", fqe::DataType::Int32},
            {"timestamp", fqe::DataType::Int64}
        });

        fqe::Table csv_table = fqe::load_csv(
            "data/trades_test.csv", trades);

        fqe::QueryEngine query_engine;

        query_engine.add_table(
            "trades", std::move(csv_table));

        std::cout << "Query engine integration tests\n";

        fqe::QueryResult projection_result = query_engine.execute(
            "SELECT price, quantity, "
            "price * quantity AS trade_value "
            "FROM trades "
            "WHERE instrument = 1;");

        expect(
            projection_result.row_count() == 7 &&
            projection_result.column_count() == 3,
            "Projection result shape");

        expect(
            result_value(projection_result, 0, 0) == 1200 &&
            result_value(projection_result, 1, 0) == 600 &&
            result_value(projection_result, 2, 0) == 720000 &&
            result_value(projection_result, 0, 6) == 1650 &&
            result_value(projection_result, 1, 6) == 590 &&
            result_value(projection_result, 2, 6) == 973500,
            "Projection result values");

        expect(
            projection_result.schema().field(2).name ==
                "trade_value",
            "Projection alias");

        fqe::QueryResult aggregate_result = query_engine.execute(
            "SELECT COUNT(*) AS trade_count, "
            "SUM(price) AS total_price, "
            "MIN(price) AS minimum_price, "
            "MAX(price) AS maximum_price "
            "FROM trades "
            "WHERE quantity > 500;");

        expect(
            aggregate_result.row_count() == 1 &&
            aggregate_result.column_count() == 4,
            "Aggregate result shape");

        expect(
            result_value(aggregate_result, 0, 0) == 20 &&
            result_value(aggregate_result, 1, 0) == 31450 &&
            result_value(aggregate_result, 2, 0) == 850 &&
            result_value(aggregate_result, 3, 0) == 2150,
            "Aggregate result values");

        expect(
            aggregate_result.schema().field(0).name ==
                "trade_count" &&
            aggregate_result.schema().field(1).name ==
                "total_price" &&
            aggregate_result.schema().field(2).name ==
                "minimum_price" &&
            aggregate_result.schema().field(3).name ==
                "maximum_price",
            "Aggregate aliases");

        fqe::QueryResult grouped_result = query_engine.execute(
            "SELECT instrument, "
            "COUNT(*) AS trade_count, "
            "SUM(price) AS total_price "
            "FROM trades "
            "WHERE quantity > 500 "
            "GROUP BY instrument;");

        expect(
            grouped_result.row_count() == 5 &&
            grouped_result.column_count() == 3,
            "GROUP BY result shape");

        const std::vector<std::int64_t> instruments = {
            1, 2, 3, 4, 5
        };

        const std::vector<std::int64_t> expected_counts = {
            6, 5, 3, 4, 2
        };

        const std::vector<std::int64_t> expected_sums = {
            9700, 7700, 4850, 6150, 3050
        };

        bool grouped_values_valid = true;

        for (std::size_t index = 0;
             index < instruments.size(); index++) {

            const std::size_t row_index = find_group_row(
                grouped_result, instruments[index]);

            if (row_index == grouped_result.row_count()) {
                grouped_values_valid = false;
                break;
            }

            if (result_value(grouped_result, 1, row_index) !=
                    expected_counts[index] ||
                result_value(grouped_result, 2, row_index) !=
                    expected_sums[index]) {

                grouped_values_valid = false;
                break;
            }
        }

        expect(
            grouped_values_valid,
            "GROUP BY result values");

        bool unknown_table_rejected = false;

        try {
            query_engine.execute(
                "SELECT COUNT(*) FROM missing_table;");
        }

        catch (const std::invalid_argument&) {
            unknown_table_rejected = true;
        }

        expect(
            unknown_table_rejected,
            "Unknown table rejection");

        bool unknown_column_rejected = false;

        try {
            query_engine.execute(
                "SELECT unknown_column FROM trades;");
        }

        catch (const std::out_of_range&) {
            unknown_column_rejected = true;
        }

        expect(
            unknown_column_rejected,
            "Unknown column rejection");

        bool invalid_grouping_rejected = false;

        try {
            query_engine.execute(
                "SELECT instrument, SUM(price) "
                "FROM trades;");
        }

        catch (const std::invalid_argument&) {
            invalid_grouping_rejected = true;
        }

        expect(
            invalid_grouping_rejected,
            "Invalid aggregate grouping rejection");

        std::cout << "All integration tests passed\n";

        return EXIT_SUCCESS;
    }

    catch (const std::exception& error) {
        std::cerr << "Integration test failure: "
            << error.what() << '\n';

        return EXIT_FAILURE;
    }
}