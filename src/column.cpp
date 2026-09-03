#include "fqe/column.hpp"
#include "fqe/data_type.hpp"

#include <stdexcept>

namespace fqe { 

    namespace {
        
        // construct vector depending on type
        ColumnData make_column_data (DataType type){

            switch (type) {

                case DataType::Int32:
                    return std::vector<std::int32_t>{};

                case DataType::Int64:
                    return std::vector<std::int64_t>{};

            }

            throw std::invalid_argument("Column data can only be 32 bit or 64 bit signed");
        }


    }

    Column::Column(DataType type) : data_(make_column_data(type)){}

    // apply .type() to a column to return the data type of the elements in the vector
    // either signed 32 or 64 bit
    DataType Column::type() const noexcept {

        // conditional essentially boilds down to 
        // data type of data_ == std::vector<std::int32_t>;
        // true if data_ is a vector of 32 bit signed values
        if (std::holds_alternative <std::vector<std::int32_t>> (data_)){
            return DataType::Int32;
        }

        return DataType::Int64;
    }

    // essentially this will just allow us to return the size of the variant vector column
    // by calling a lambda that essentially performs 
    // const std::vector<std::DataType>& values = std::get<std::vector<std::DataType>> (data_);
    // std::visit figures out whether the type is 32 or 64 bit signed  
    std::size_t Column::size() const noexcept {
        return std::visit ([](const auto& values) {return values.size();}, data_);
    }

    // reserve capacity required to store all values for that column
    void Column::reserve(std::size_t capacity){
        std::visit([capacity] (auto& values){values.reserve(capacity);}, data_);
    }

    void Column::append(std::int32_t value){

        auto* values = std::get_if<std::vector<std::int32_t>> (&data_); // check if data
        // holds int32

        if (values == nullptr){ // nullptr -> doesnt hold int32 values so invalid argument
            throw std::invalid_argument("cannot append Int32 value to non Int32 column");
        }

        // values is a pointer to the vector so we use ->push_back / (*values).push_back()
        values->push_back(value);
    }

    void Column::append(std::int64_t value){

        auto* values = std::get_if<std::vector<std::int64_t>> (&data_);

        if (values == nullptr){
            throw std::invalid_argument("cannot append Int64 value to non Int64 column");
        }

        values->push_back(value);
    }

    // return data vector when .data() is called
    const ColumnData& Column::data() const noexcept {
        return data_; 
    }





}