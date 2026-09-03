#include "fqe/aggregate.hpp"

#include <stdexcept>
#include <variant>

namespace fqe {

    namespace {

        void validate_mask_size (const Column& column, const SelectionMask& mask) {

            if (column.size() != mask.size()){
                throw std::invalid_argument("mask and column size mismatch");
            }
        }
    }

    std::size_t count_selected (const SelectionMask& mask) {

        std::size_t count = 0;

        for (std::uint8_t selected : mask){

            if (selected) {
                count++;
            }
        }

        return count; 
    }

    std::optional<std::int64_t> sum_selected (const Table& table, 
     const std::string& column_name, const SelectionMask& mask) {

        const Column& column = table.column(column_name);

        validate_mask_size(column, mask);

        std::optional<std::int64_t> sum;

        std::visit ([&] (const auto& values){
            for (std::size_t i = 0; i < values.size(); i++){
                if (mask[i]) {

                    if (!sum.has_value()){
                        sum = 0; 
                    }

                    sum.value() += static_cast<std::int64_t>(values[i]);
                }
            }
        }, column.data());

        return sum; 
    }

    std::optional<std::int64_t> min_selected (const Table& table, const std::string& column_name, 
     const SelectionMask& mask) {

        const Column& column = table.column(column_name);

        validate_mask_size(column, mask);

        std::optional<std::int64_t> minimum;

        std::visit ([&] (const auto& values){

            for (size_t i = 0; i < values.size(); i++){

                if (mask[i]) {
                    std::int64_t value = static_cast<std::int64_t>(values[i]);

                    if (!minimum.has_value() || value < minimum.value()){
                        minimum = value; 
                    }
                }
            }
        }, column.data());

        return minimum; 
     }

    std::optional<std::int64_t> max_selected (const Table& table, const std::string& column_name, 
     const SelectionMask& mask) {

        const Column& column = table.column(column_name);

        validate_mask_size(column, mask);

        std::optional<std::int64_t> maximum;

        std::visit ([&] (const auto& values){

            for (size_t i = 0; i < values.size(); i++){

                if (mask[i]) {
                    std::int64_t value = static_cast<std::int64_t>(values[i]);

                    if (!maximum.has_value() || value > maximum.value()){
                        maximum = value; 
                    }
                }
            }
        }, column.data());

        return maximum;
     }


}