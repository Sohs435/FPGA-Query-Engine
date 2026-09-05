#include <iostream>  
#include "fqe/data_type.hpp"  
#include "fqe/schema.hpp"  
#include "fqe/column.hpp"  
#include "fqe/table.hpp"  
#include "fqe/csv_loader.hpp"  
#include "fqe/predicate.hpp"  
#include "fqe/aggregate.hpp"  
#include "fqe/binder.hpp" 
#include "fqe/filter.hpp"
#include <limits>
#include <stdexcept>
#include "fqe/tokenizer.hpp"
  
int main() {  
  
    // Test Int32  
    std::cout  
        << fqe::to_string(fqe::DataType::Int32)  << '\n';  
  
    // Test Int64  
    std::cout  
        << fqe::to_string(fqe::DataType::Int64)  << '\n';  
      
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

    fqe::BoundComparisonPredicate bound_quantity_predicate = fqe::bind_predicate(
        csv_table.schema(), quantity_predicate);
      
    fqe::SelectionMask quantity_mask = fqe::evaluate_predicate(csv_table, 
        bound_quantity_predicate);  
  
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

    fqe::BoundComparisonPredicate bound_lower_price_predicate = fqe::bind_predicate(
        csv_table.schema(), lower_price_predicate);

    fqe::BoundComparisonPredicate bound_upper_price_predicate = fqe::bind_predicate(
        csv_table.schema(), upper_price_predicate);
  
    fqe::SelectionMask lower_price_mask = fqe::evaluate_predicate(csv_table,   
        bound_lower_price_predicate);  
  
    fqe::SelectionMask upper_price_mask = fqe::evaluate_predicate(csv_table,   
        bound_upper_price_predicate);  
  
    fqe::SelectionMask compound_mask = quantity_mask;  
  
    fqe::combine_and(compound_mask, lower_price_mask);  
  
    fqe::combine_and(compound_mask, upper_price_mask);  
  
    std::cout << "\nCompound Count: " << fqe::count_selected(compound_mask) << '\n';  
  
    fqe::ComparisonPredicate low_price_predicate{"price",  
        fqe::ComparisonOperator::LessThan, 1000};  

    fqe::BoundComparisonPredicate bound_low_price_predicate = fqe::bind_predicate(
        csv_table.schema(), low_price_predicate);
  
    fqe::SelectionMask low_price_mask = fqe::evaluate_predicate(csv_table,   
        bound_low_price_predicate);  
  
    fqe::SelectionMask or_mask = quantity_mask;  
  
    fqe::combine_or(or_mask, low_price_mask);  
  
    std::cout << "OR Count: " << fqe::count_selected(or_mask) << '\n';  
  
    fqe::SelectionMask not_mask = quantity_mask;  
  
    fqe::invert_mask(not_mask);  
  
    std::cout << "NOT Count: " << fqe::count_selected(not_mask) << '\n';  

    // Implements
    // SELECT SUM(price)
    // FROM trades
    // WHERE quantity > 500
    // AND ((instrument = 1 AND price = 1200) OR (instrument = 4 AND price = 1900));


    fqe::ComparisonPredicate instrument_one_predicate{"instrument",
    fqe::ComparisonOperator::Equal, 1};

    fqe::ComparisonPredicate price_1200_predicate{"price",
        fqe::ComparisonOperator::Equal, 1200};

    fqe::ComparisonPredicate instrument_four_predicate{"instrument",
        fqe::ComparisonOperator::Equal, 4};

    fqe::ComparisonPredicate price_1900_predicate{"price",
        fqe::ComparisonOperator::Equal, 1900};
    // doing checks without header but rather index of column 
    fqe::BoundComparisonPredicate bound_instrument_one =
        fqe::bind_predicate(csv_table.schema(), instrument_one_predicate);

    fqe::BoundComparisonPredicate bound_price_1200 =
        fqe::bind_predicate(csv_table.schema(), price_1200_predicate);

    fqe::BoundComparisonPredicate bound_instrument_four =
        fqe::bind_predicate(csv_table.schema(), instrument_four_predicate);

    fqe::BoundComparisonPredicate bound_price_1900 =
        fqe::bind_predicate(csv_table.schema(), price_1900_predicate);

    fqe::SelectionMask first_branch = fqe::evaluate_predicate(csv_table,
        bound_instrument_one);

    fqe::SelectionMask price_1200_mask = fqe::evaluate_predicate(csv_table,
        bound_price_1200);

    fqe::combine_and(first_branch, price_1200_mask);

    fqe::SelectionMask second_branch = fqe::evaluate_predicate(csv_table,
        bound_instrument_four);

    fqe::SelectionMask price_1900_mask = fqe::evaluate_predicate(csv_table,
        bound_price_1900);

    fqe::combine_and(second_branch, price_1900_mask);

    fqe::SelectionMask complex_mask = first_branch;

    fqe::combine_or(complex_mask, second_branch);

    fqe::combine_and(complex_mask, quantity_mask);

    std::cout << "\nComplex predicate count: " << fqe::count_selected(complex_mask) << '\n';

    auto complex_price_sum = fqe::sum_selected(csv_table, "price",
        complex_mask);

    if (complex_price_sum.has_value()){
        std::cout << "Complex predicate price sum: " << complex_price_sum.value() << '\n';
    }

    // Implements in SQL
    // SELECT COUNT(*), SUM(price) FROM trades WHERE quantity > 500 AND
    // ((instrument = 1 AND price = 1200) OR
    // (instrument = 4 AND price = 1900));
    // Can see that its very similar in expression to what's above
    // Obv no front end created to convert the top expression to the actual code ver
    fqe::PredicateExpressionPtr tree_filter_expression = fqe::make_and(
        fqe::make_comparison(fqe::ComparisonPredicate{
            "quantity",
            fqe::ComparisonOperator::GreaterThan,
            500
        }), // quantity > 500 AND 
        fqe::make_or(
            fqe::make_and(
                fqe::make_comparison(fqe::ComparisonPredicate{
                    "instrument",
                    fqe::ComparisonOperator::Equal,
                    1
                }), // instrument == 1 
                fqe::make_comparison(fqe::ComparisonPredicate{
                    "price",
                    fqe::ComparisonOperator::Equal,
                    1200
                }) // price == 1200 
            ), // instrument == 1 AND price == 1200
            fqe::make_and(
                fqe::make_comparison(fqe::ComparisonPredicate{
                    "instrument",
                    fqe::ComparisonOperator::Equal,
                    4
                }), // instrument == 4
                fqe::make_comparison(fqe::ComparisonPredicate{
                    "price",
                    fqe::ComparisonOperator::Equal,
                    1900
                }) // price == 1900
            )
        ) //instrument == 4 AND price == 1900
    ); // (instrument == 1 AND price == 1200) OR (instrument == 4 AND price == 1900)
    // quantity > 500 AND  (instrument == 1 AND price == 1200) OR (instrument == 4 AND price == 1900)


    fqe::BoundPredicateExpressionPtr tree_bound_filter_expression =
        fqe::bind_predicate_expression(
            csv_table.schema(), *tree_filter_expression);

    fqe::SelectionMask tree_selection_mask =
        fqe::evaluate_predicate_expression(
            csv_table, *tree_bound_filter_expression);

    auto tree_selected_price_sum = fqe::sum_selected(
        csv_table, "price", tree_selection_mask);

    std::cout << "\nTree predicate count: "
        << fqe::count_selected(tree_selection_mask) << '\n';

    if (tree_selected_price_sum.has_value()){
        std::cout << "Tree predicate price sum: "
            << tree_selected_price_sum.value() << '\n';
    }

    // Implements
    // SELECT COUNT(*)
    // FROM trades
    // WHERE NOT (quantity > 500);

    fqe::PredicateExpressionPtr tree_not_expression = fqe::make_not(
        fqe::make_comparison(fqe::ComparisonPredicate{
            "quantity",
            fqe::ComparisonOperator::GreaterThan,
            500
        })
    );

    fqe::BoundPredicateExpressionPtr tree_bound_not_expression =
        fqe::bind_predicate_expression(csv_table.schema(),
            *tree_not_expression);

    fqe::SelectionMask tree_not_selection_mask =
        fqe::evaluate_predicate_expression(csv_table,
            *tree_bound_not_expression);

    std::cout << "\nTree NOT count: "
        << fqe::count_selected(tree_not_selection_mask)
        << " (expected 10)\n";

    // Implements
    // SELECT COUNT(*)
    // FROM trades
    // WHERE price BETWEEN 1000 AND 2000;

    fqe::PredicateExpressionPtr tree_inclusive_between_expression =
        fqe::make_between("price", 1000, 2000, true, true);

    fqe::BoundPredicateExpressionPtr
        tree_bound_inclusive_between_expression =
            fqe::bind_predicate_expression(csv_table.schema(),
                *tree_inclusive_between_expression);

    fqe::SelectionMask tree_inclusive_between_mask =
        fqe::evaluate_predicate_expression(csv_table,
            *tree_bound_inclusive_between_expression);

    std::cout << "Inclusive BETWEEN count: "
        << fqe::count_selected(tree_inclusive_between_mask)
        << " (expected 21)\n";


    // Implements
    // SELECT COUNT(*)
    // FROM trades
    // WHERE price > 1000 AND price < 2000;

    fqe::PredicateExpressionPtr tree_exclusive_between_expression =
        fqe::make_between("price", 1000, 2000, false, false);

    fqe::BoundPredicateExpressionPtr
        tree_bound_exclusive_between_expression =
            fqe::bind_predicate_expression(csv_table.schema(),
                *tree_exclusive_between_expression);

    fqe::SelectionMask tree_exclusive_between_mask =
        fqe::evaluate_predicate_expression(csv_table,
            *tree_bound_exclusive_between_expression);

    std::cout << "Exclusive BETWEEN count: "
        << fqe::count_selected(tree_exclusive_between_mask)
        << " (expected 19)\n";


    // Implements
    // SELECT COUNT(*)
    // FROM trades
    // WHERE instrument IN (1, 4);

    fqe::PredicateExpressionPtr tree_in_expression = fqe::make_in(
        "instrument",
        std::vector<std::int64_t>{1, 4},
        false
    );

    fqe::BoundPredicateExpressionPtr tree_bound_in_expression =
        fqe::bind_predicate_expression(csv_table.schema(),
            *tree_in_expression);

    fqe::SelectionMask tree_in_selection_mask =
        fqe::evaluate_predicate_expression(csv_table,
            *tree_bound_in_expression);

    std::cout << "IN count: "
        << fqe::count_selected(tree_in_selection_mask)
        << " (expected 12)\n";


    // Implements
    // SELECT COUNT(*)
    // FROM trades
    // WHERE instrument NOT IN (1, 4);

    fqe::PredicateExpressionPtr tree_not_in_expression = fqe::make_in(
        "instrument",
        std::vector<std::int64_t>{1, 4},
        true
    );

    fqe::BoundPredicateExpressionPtr tree_bound_not_in_expression =
        fqe::bind_predicate_expression(csv_table.schema(),
            *tree_not_in_expression);

    fqe::SelectionMask tree_not_in_selection_mask =
        fqe::evaluate_predicate_expression(csv_table,
            *tree_bound_not_in_expression);

    std::cout << "NOT IN count: "
        << fqe::count_selected(tree_not_in_selection_mask)
        << " (expected 18)\n";


    // Implements
    // SELECT COUNT(*)
    // FROM trades
    // WHERE price > quantity;

    fqe::PredicateExpressionPtr tree_column_comparison_expression =
        fqe::make_comparison(
            fqe::make_column_reference("price"),
            fqe::ComparisonOperator::GreaterThan,
            fqe::make_column_reference("quantity")
        );

    fqe::BoundPredicateExpressionPtr
        tree_bound_column_comparison_expression =
            fqe::bind_predicate_expression(csv_table.schema(),
                *tree_column_comparison_expression);

    fqe::SelectionMask tree_column_comparison_mask =
        fqe::evaluate_predicate_expression(csv_table,
            *tree_bound_column_comparison_expression);

    std::cout << "Column comparison count: "
        << fqe::count_selected(tree_column_comparison_mask)
        << " (expected 29)\n";


    // Implements
    // SELECT COUNT(*)
    // FROM trades
    // WHERE price * quantity > 1000000;

    fqe::PredicateExpressionPtr tree_arithmetic_expression =
        fqe::make_comparison(
            fqe::make_arithmetic(
                fqe::ArithmeticOperator::Multiply,
                fqe::make_column_reference("price"),
                fqe::make_column_reference("quantity")
            ),
            fqe::ComparisonOperator::GreaterThan,
            fqe::make_integer_literal(1000000)
        );

    fqe::BoundPredicateExpressionPtr
        tree_bound_arithmetic_expression =
            fqe::bind_predicate_expression(csv_table.schema(),
                *tree_arithmetic_expression);

    fqe::SelectionMask tree_arithmetic_selection_mask =
        fqe::evaluate_predicate_expression(csv_table,
            *tree_bound_arithmetic_expression);

    std::cout << "Arithmetic comparison count: "
        << fqe::count_selected(tree_arithmetic_selection_mask)
        << " (expected 11)\n";


    // Implements
    // SELECT COUNT(*)
    // FROM trades
    // WHERE TRUE;

    fqe::PredicateExpressionPtr tree_true_expression =
        fqe::make_boolean(true);

    fqe::BoundPredicateExpressionPtr tree_bound_true_expression =
        fqe::bind_predicate_expression(csv_table.schema(),
            *tree_true_expression);

    fqe::SelectionMask tree_true_selection_mask =
        fqe::evaluate_predicate_expression(csv_table,
            *tree_bound_true_expression);

    std::cout << "TRUE count: "
        << fqe::count_selected(tree_true_selection_mask)
        << " (expected 30)\n";


    // Implements
    // SELECT COUNT(*)
    // FROM trades
    // WHERE FALSE;

    fqe::PredicateExpressionPtr tree_false_expression =
        fqe::make_boolean(false);

    fqe::BoundPredicateExpressionPtr tree_bound_false_expression =
        fqe::bind_predicate_expression(csv_table.schema(),
            *tree_false_expression);

    fqe::SelectionMask tree_false_selection_mask =
        fqe::evaluate_predicate_expression(csv_table,
            *tree_bound_false_expression);

    std::cout << "FALSE count: "
        << fqe::count_selected(tree_false_selection_mask)
        << " (expected 0)\n";


    // Tests whether an unknown column is rejected during binding.

    try {

        fqe::PredicateExpressionPtr tree_invalid_column_expression =
            fqe::make_comparison(fqe::ComparisonPredicate{
                "unknown_column",
                fqe::ComparisonOperator::GreaterThan,
                500
            });

        fqe::BoundPredicateExpressionPtr tree_invalid_column_bound =
            fqe::bind_predicate_expression(csv_table.schema(),
                *tree_invalid_column_expression);

        std::cout << "Invalid column test: FAIL\n";
    }

    catch (const std::out_of_range& error){
        std::cout << "Invalid column test: PASS "
            << error.what() << '\n';
    }


    // Tests whether division by zero is rejected during execution.

    try {

        fqe::PredicateExpressionPtr tree_division_zero_expression =
            fqe::make_comparison(
                fqe::make_arithmetic(
                    fqe::ArithmeticOperator::Divide,
                    fqe::make_column_reference("price"),
                    fqe::make_integer_literal(0)
                ),
                fqe::ComparisonOperator::GreaterThan,
                fqe::make_integer_literal(1)
            );

        fqe::BoundPredicateExpressionPtr
            tree_bound_division_zero_expression =
                fqe::bind_predicate_expression(csv_table.schema(),
                    *tree_division_zero_expression);

        fqe::SelectionMask tree_division_zero_mask =
            fqe::evaluate_predicate_expression(csv_table,
                *tree_bound_division_zero_expression);

        std::cout << "Division by zero test: FAIL\n";
    }

    catch (const std::domain_error& error){
        std::cout << "Division by zero test: PASSED - "
            << error.what() << '\n';
    }


    // Tests whether signed Int64 overflow is rejected.

    try {

        fqe::PredicateExpressionPtr tree_overflow_expression =
            fqe::make_comparison(
                fqe::make_arithmetic(
                    fqe::ArithmeticOperator::Add,
                    fqe::make_integer_literal(
                        std::numeric_limits<std::int64_t>::max()),
                    fqe::make_integer_literal(1)
                ),
                fqe::ComparisonOperator::GreaterThan,
                fqe::make_integer_literal(0)
            );

        fqe::BoundPredicateExpressionPtr tree_bound_overflow_expression =
            fqe::bind_predicate_expression(csv_table.schema(),
                *tree_overflow_expression);

        fqe::SelectionMask tree_overflow_selection_mask =
            fqe::evaluate_predicate_expression(csv_table,
                *tree_bound_overflow_expression);

        std::cout << "Arithmetic overflow test: FAILED\n";
    }

    catch (const std::overflow_error& error){
        std::cout << "Arithmetic overflow test: PASSED - " << error.what() << '\n';
    }


    // Tests filtering an empty table.

    fqe::Table empty_filter_table(
        trades,
        {
            fqe::Column(std::vector<std::int32_t>{}),
            fqe::Column(std::vector<std::int32_t>{}),
            fqe::Column(std::vector<std::int32_t>{}),
            fqe::Column(std::vector<std::int64_t>{})
        }
    );

    fqe::SelectionMask empty_table_selection_mask =
        fqe::evaluate_predicate_expression(empty_filter_table, *tree_bound_true_expression);

    std::cout << "Empty table count: "<< fqe::count_selected(empty_table_selection_mask)
     << " (expected 0)\n";

     //TOKENIZER TESTING/DEBUGGING

    // Tokenizes:
    // SELECT SUM(price * quantity), COUNT(*)
    // FROM trades
    // WHERE quantity >= 500
    // AND NOT (price < 1000 OR instrument IN (2, 3))
    // GROUP BY instrument;

    std::string tokenizer_query =
        "SeLECT SUM(price * quantity), COUNT(*) "
        "FRoM trades "
        "WHERE quantity >= 500 "
        "ANd NOT (price < 1000 OR instrument IN (2, 3)) "
        "GROUP BY instrument;";

    fqe::Tokenizer query_tokenizer(tokenizer_query);

    std::vector<fqe::Token> tokenizer_tokens = query_tokenizer.tokenize();

    std::cout << "\nTokenizer output:\n";
    
    //check all token type values in query and if they match and in order
    for (const fqe::Token& token : tokenizer_tokens) {

        std::cout << token.position << " | " << fqe::to_string(token.type) << " | '"
            << token.text << "'";

        if (token.integer_value.has_value()) {
            std::cout << " | value = " << token.integer_value.value();
        }

        std::cout << '\n';
    }
  
    return 0;  
}