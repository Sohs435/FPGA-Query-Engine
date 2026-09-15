#pragma once

#include "query_binder.hpp"
#include "query_result.hpp"

namespace fqe {

    QueryResult execute_query(
        const Table& table,
        const BoundQuery& query);

}