#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>

#include "predicate.hpp"
#include "table.hpp"

namespace fqe {

    std::size_t count_selected(const SelectionMask& mask);

    std::optional<std::int64_t> sum_selected(const Table& table, const std::string& column_name,
        const SelectionMask& mask);

    std::optional<std::int64_t> min_selected(const Table& table,const std::string& column_name,
        const SelectionMask& mask);

    std::optional<std::int64_t> max_selected(const Table& table, const std::string& column_name,
        const SelectionMask& mask);


}