#include "fqe/predicate.hpp"
#include "fqe/binder.hpp"
#include <stdexcept>
#include <variant> 

namespace fqe { 

    namespace {
        
        bool compare (std::int64_t column_value, ComparisonOperator comparison, 
         std::int64_t predicate_value){

            switch (comparison) {

                case ComparisonOperator::Equal:
                    return column_value == predicate_value; 
                    
                case ComparisonOperator::NotEqual:
                    return column_value != predicate_value; 

                case ComparisonOperator::LessThan:
                    return column_value < predicate_value;

                case ComparisonOperator::LessEqual:
                    return column_value <= predicate_value; 
                    
                case ComparisonOperator::GreaterThan:
                    return column_value > predicate_value; 

                case ComparisonOperator::GreaterEqual:
                    return column_value >= predicate_value; 
            }

            throw std::invalid_argument ("Unsupported Comparison Operand");
        } 

    }

    SelectionMask evaluate_predicate (const Table& table, const BoundComparisonPredicate& predicate){

        const Column& column = table.column(predicate.column_index);

        if (column.type() != predicate.column_type){
            throw std::invalid_argument(
                "Bound predicate type does not match table column type"
            );
        }

        SelectionMask mask(column.size(), 0);

        std::visit ([&] (const auto& values) { 
            
                for (std::size_t i = 0; i < values.size(); i++){
                    mask[i] = compare(static_cast<std::int64_t>(values[i]), predicate.comparison,
                        predicate.value);
                }
            },

            column.data()
        );

        return mask;
    }

    void combine_and(SelectionMask& current_mask, const SelectionMask& new_mask) {

        if (current_mask.size() != new_mask.size()){
            throw std::invalid_argument("Cannot combine masks with different sizes");
        }

        for (std::size_t i = 0; i < current_mask.size(); i++){
            current_mask[i] = current_mask[i] && new_mask[i];
        }
    }

    void combine_or(SelectionMask& current_mask, const SelectionMask& new_mask) {

        if (current_mask.size() != new_mask.size()){
            throw std::invalid_argument("Cannot combine masks with different sizes");
        }

        for (std::size_t i = 0; i < current_mask.size(); i++){
            current_mask[i] = current_mask[i] || new_mask[i];
        }

    }

    void invert_mask(SelectionMask& current_mask){

        for (std::size_t i = 0; i < current_mask.size(); i++){
            current_mask[i] = !current_mask[i];
        }
    }
}