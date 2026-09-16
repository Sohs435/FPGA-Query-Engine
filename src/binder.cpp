#include "fqe/binder.hpp"

#include <stdexcept>

namespace fqe {

    namespace {

        void validate_column_type (DataType type) {

            switch (type) {
                case DataType::Int32:
                case DataType::Int64:
                    return; 
            }

            throw std::invalid_argument ("Cannot bind predicate as type is not signed 32/64 bit");
        }

        void validate_comparison_operator (ComparisonOperator comparison) {

            switch (comparison) {

                case ComparisonOperator::Equal:
                case ComparisonOperator::NotEqual:
                case ComparisonOperator::LessThan:
                case ComparisonOperator::LessEqual:
                case ComparisonOperator::GreaterThan:
                case ComparisonOperator::GreaterEqual:
                    return;
            }

            throw std::invalid_argument("Cannot bind unsupported comparison operator");
        }
    }

    BoundComparisonPredicate bind_predicate (const Schema& schema, 
     const ComparisonPredicate& predicate) {
        
        std::size_t column_index = schema.index_of(predicate.column_name); 

        const Field& field = schema.field(column_index);

        validate_column_type(field.type);

        validate_comparison_operator(predicate.comparison);

        return BoundComparisonPredicate {column_index, field.type, predicate.comparison,
             predicate.value};

    }

    std::vector<BoundComparisonPredicate> bind_predicates (const Schema& schema,
     const std::vector<ComparisonPredicate>& predicates) {

        std::vector<BoundComparisonPredicate> bound_predicates;

        bound_predicates.reserve(predicates.size());

        for (const ComparisonPredicate& predicate : predicates){
            bound_predicates.push_back(bind_predicate(schema, predicate));
        }

        return bound_predicates;
    }


}