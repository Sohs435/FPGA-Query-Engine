#pragma once

#include <cstddef>
#include <optional>
#include <string>
#include <vector>

#include "filter.hpp"
#include "parser.hpp"
#include "schema.hpp"

namespace fqe {

    struct BoundSelectItem {

        // Empty for an ordinary projected scalar expression
        std::optional<AggregateFunction> aggregate;

        // Bound scalar tree containing column indices rather than column names
        // This remains nullptr for COUNT(*)
        BoundScalarExpressionPtr expression;

        bool is_star = false;

        std::optional<std::string> alias;
    };

    struct BoundQuery {

        std::vector<BoundSelectItem> select_items;

        // Retained for identification and later catalog integration
        std::string table_name;

        // nullptr means that the query doesnt contain WHERE
        BoundPredicateExpressionPtr where_expression;

        // GROUP BY column names converted into schema indices
        std::vector<std::size_t> group_by_column_indices;
    };

    BoundQuery bind_query(const ParsedQuery& query, const Schema& schema);

}