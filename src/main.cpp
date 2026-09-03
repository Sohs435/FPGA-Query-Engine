#include <iostream>
#include "fqe/data_type.hpp"
#include "fqe/schema.hpp"
#include "fqe/column.hpp"
#include "fqe/table.hpp"
#include "fqe/csv_loader.hpp"

int main() {

    // Test Int32
    std::cout
        << fqe::to_string(fqe::DataType::Int32)
        << '\n';

    // Test Int64
    std::cout
        << fqe::to_string(fqe::DataType::Int64)
        << '\n';
    
    fqe:: Schema trades({
        {"price", fqe::DataType::Int32}, {"quantity", fqe::DataType::Int32}, 
        {"instrument", fqe::DataType::Int32}, {"timestamp", fqe::DataType::Int64}
    });

    std::cout << trades << '\n';

    std::cout << "Quantity index: " << trades.index_of("quantity") << '\n';

    const fqe::Field& price_field = trades.field("price");

    std::cout << "price type: " << fqe::to_string(price_field.type) << '\n';

    fqe::Column prices (fqe::DataType::Int32);

    prices.reserve(3);

    prices.append(std::int32_t{1200});
    prices.append(std::int32_t{909090});
    prices.append(std::int32_t{1201}); 

    std::cout << "Type: " << fqe::to_string(prices.type()) << '\n';

    std::cout << "Number of values: " << prices.size() << '\n'; 

    // read only reference to vector inside prices
    const auto& values = std::get<std::vector<std::int32_t>>(prices.data());

    for (std::int32_t val : values){
        std::cout << val << '\n';
    }

    fqe::Table trades_table(
        trades, 
        {
            prices,
            fqe::Column(std::vector<std::int32_t>{600, 700, 800}),
            fqe::Column(std::vector<std::int32_t>{1, 2, 1}),
            fqe::Column(std::vector<std::int64_t>{100000001, 100000002, 100000003})
        }
    );

    std::cout << "Table rows: " << trades_table.row_count() << '\n';
    std::cout << "Table columns: " << trades_table.column_count() << '\n';

    const fqe::Column& quantity_column = trades_table.column("quantity");

    const auto& quantities = std::get<std::vector<std::int32_t>>(quantity_column.data());

    for (std:: int32_t quantity : quantities){
        std::cout << quantity << '\n'; 
    }

    fqe::Table csv_table = fqe::load_csv("data/trades_test.csv", trades);

    std::cout << "\nCSV table rows: "  << csv_table.row_count() << '\n';

    std::cout << "CSV table columns: " << csv_table.column_count() << '\n';

    const auto& csv_prices = std::get<std::vector<std::int32_t>>(
        csv_table.column("price").data());

    const auto& csv_quantities = std::get<std::vector<std::int32_t>>(
        csv_table.column("quantity").data());

    const auto& csv_instruments = std::get<std::vector<std::int32_t>>(
        csv_table.column("instrument").data());

    const auto& csv_timestamps = std::get<std::vector<std::int64_t>>(
        csv_table.column("timestamp").data());

    for (std::size_t i = 0; i < csv_table.row_count(); i++){

        std::cout << csv_prices[i] << ", " << csv_quantities[i] << ", " << csv_instruments[i] << ", " << csv_timestamps[i] << '\n';
    }

    return 0;
}