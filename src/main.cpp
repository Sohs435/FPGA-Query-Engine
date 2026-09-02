#include <iostream>
#include "fqe/data_type.hpp"
#include "fqe/Schema.hpp"
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

    return 0;
}