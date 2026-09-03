#include <iostream>
#include "fqe/data_type.hpp"
#include "fqe/schema.hpp"
#include "fqe/column.hpp"

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

    return 0;
}