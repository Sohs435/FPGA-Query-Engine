#pragma once

#include <cstddef>
#include <cstdint>
#include <variant>
#include <vector>

#include "data_type.hpp"

namespace fqe {

    using ColumnData = std::variant< std::vector <std::int32_t>,  std::vector <std::int64_t>>;

    class Column {

        public:
            explicit Column(DataType type); // column constructor 

            DataType type() const noexcept; // .type() will return whether
            // DataType is int32 or 64

            std::size_t size() const noexcept; // .size will return the number
            // of element values in the column

            void reserve(std::size_t capacity); // .reserve will allow to reserve
            // certain amount of memory depending on the number of element values present 
            // in the column 

            void append (std::int32_t value); 

            void append(std::int64_t value); //append will allow for insertion of 
            // 32 or 64 bit values

            const ColumnData& data() const noexcept; //.data() will return all of the
            // element values present in the column 

        private:

            ColumnData data_; 
    };

}