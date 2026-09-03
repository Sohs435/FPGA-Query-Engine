#include <iostream> 
#include "fqe/data_type.hpp" 
#include "fqe/schema.hpp" 
#include "fqe/column.hpp" 
#include "fqe/table.hpp" 
#include "fqe/csv_loader.hpp" 
#include "fqe/predicate.hpp" 
#include "fqe/aggregate.hpp" 
 
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
 
    fqe::ComparisonPredicate quantity_predicate{"quantity",  
        fqe::ComparisonOperator::GreaterThan,500}; 
     
    fqe::SelectionMask quantity_mask = fqe::evaluate_predicate(csv_table, quantity_predicate); 
 
    std::cout << "\nResults for quantity > 500\n"; 
 
    std::cout << "Count: " << fqe::count_selected(quantity_mask) << '\n'; 
 
    auto price_sum = fqe::sum_selected(csv_table, "price", quantity_mask); 
 
    auto minimum_price = fqe::min_selected(csv_table, "price", quantity_mask); 
 
    auto maximum_price = fqe::max_selected(csv_table, "price", quantity_mask); 
 
    if (price_sum.has_value()){ 
        std::cout << "Price sum: " << price_sum.value() << '\n'; 
    } 
 
    if (minimum_price.has_value()){ 
        std::cout << "Minimum price: " << minimum_price.value() << '\n'; 
    } 
 
    if (maximum_price.has_value()){ 
        std::cout << "Maximum price: " << maximum_price.value() << '\n'; 
    } 
 
    fqe::ComparisonPredicate lower_price_predicate{"price", 
        fqe::ComparisonOperator::GreaterEqual, 1000}; 
 
    fqe::ComparisonPredicate upper_price_predicate{"price", 
        fqe::ComparisonOperator::LessEqual, 2000}; 
 
    fqe::SelectionMask lower_price_mask = fqe::evaluate_predicate(csv_table,  
        lower_price_predicate); 
 
    fqe::SelectionMask upper_price_mask = fqe::evaluate_predicate(csv_table,  
        upper_price_predicate); 
 
    fqe::SelectionMask compound_mask = quantity_mask; 
 
    fqe::combine_and(compound_mask, lower_price_mask); 
 
    fqe::combine_and(compound_mask, upper_price_mask); 
 
    std::cout << "\nCompound Count: " << fqe::count_selected(compound_mask) << '\n'; 
 
    fqe::ComparisonPredicate low_price_predicate{"price", 
        fqe::ComparisonOperator::LessThan, 1000}; 
 
    fqe::SelectionMask low_price_mask = fqe::evaluate_predicate(csv_table,  
        low_price_predicate); 
 
    fqe::SelectionMask or_mask = quantity_mask; 
 
    fqe::combine_or(or_mask, low_price_mask); 
 
    std::cout << "OR Count: " << fqe::count_selected(or_mask) << '\n'; 
 
    fqe::SelectionMask not_mask = quantity_mask; 
 
    fqe::invert_mask(not_mask); 
 
    std::cout << "NOT Count: " << fqe::count_selected(not_mask) << '\n'; 
 
    return 0; 
}