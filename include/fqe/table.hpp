#pragma once

#include <cstddef>
#include <string>
#include <vector> 

#include "column.hpp"
#include "schema.hpp"

namespace fqe {

    class Table {
        
        public:
            Table(Schema schema, std::vector<Column> columns); //table constructor 

            const Schema& schema() const noexcept; // return a given schema from table

            std::size_t row_count() const noexcept; // return number of rows

            std::size_t column_count() const noexcept; // return number of columns

            const Column& column(std::size_t index) const; //return column given column index - index

            const Column& column(const std::string& name) const; // return column given column label
            // name 

        private: 

        Schema schema_; 

        std::vector <Column> columns_; 

        std::size_t row_count_; 

    };
}