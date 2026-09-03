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

    SelectionMask evaluate_predicate(const Table& table, const ComparisonPredicate& predicate);
}