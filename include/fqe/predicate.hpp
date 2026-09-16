#pragma once

#include <cstdint>
#include <string> 

#include <cstdint>
#include <vector>

#include "table.hpp"

namespace fqe {

    enum class ComparisonOperator {
        Equal,
        NotEqual,
        LessThan,
        LessEqual, 
        GreaterThan,
        GreaterEqual
    };

    struct ComparisonPredicate {
        std::string column_name;
        ComparisonOperator comparison;
        std::int64_t value; 
    };

    using SelectionMask = std::vector<std::uint8_t>;

    struct BoundComparisonPredicate;

    SelectionMask evaluate_predicate(const Table& table, 
        const BoundComparisonPredicate& predicate);

    void combine_and(SelectionMask& current_mask, const SelectionMask& new_mask);

    void combine_or(SelectionMask& current_mask, const SelectionMask& new_mask);

    void invert_mask(SelectionMask& mask);



}