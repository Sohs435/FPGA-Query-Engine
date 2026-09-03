#include "fqe/table.hpp"

#include <stdexcept>
#include <utility>

namespace fqe {

    Table::Table (Schema schema, std::vector<Column> columns) : schema_(std::move(schema)), 
        columns_(std::move(columns)), row_count_(0) {

            if (columns_.size() != schema_.size()){ 
                throw std:: invalid_argument ("Number of Columns not matching schema");
            }

            row_count_ = columns_[0].size();

            for (std::size_t i = 0; i < columns_.size(); i++){

                if (columns_[i].type() != schema_.field(i).type){
                    throw std::invalid_argument ("Column type does not match type from schema for field: "
                     + schema_.field(i).name);
                }

                if (columns_[i].size() != row_count_){
                    throw std::invalid_argument("Columns have different number of rows");
                }
            }
        }

        const Schema& Table::schema() const noexcept {
            return schema_;
        }

        std::size_t Table::row_count() const noexcept { 
            return row_count_; 
        }

        std::size_t Table::column_count() const noexcept{
            return columns_.size();
        }

        const Column& Table::column(std::size_t index) const {

            if (index >= columns_.size()){
                throw std::out_of_range("Column index out of range");
            }

            return columns_[index];

        }

        const Column& Table::column(const std::string& name) const {
            return column(schema_.index_of(name));
        }
}