#pragma once

#include <cstddef>
#include <cstdint>

#include "data_type.hpp"
#include "predicate.hpp"
#include "schema.hpp"

namespace fqe {

    struct BoundComparisonPredicate{
        std::size_t column_index;
        DataType column_type;
        ComparisonOperator comparison;
        std::int64_t value;
        
    };

    // label, type with a private map that has indexes 1, 2, ... 
 
    BoundComparisonPredicate bind_predicate (const Schema& schema, 
        const ComparisonPredicate& predicate);

    std::vector <BoundComparisonPredicate> bind_predicates (const Schema& schema, 
        const std::vector<ComparisonPredicate>& predicates); 
}