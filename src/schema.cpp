#include "fqe/schema.hpp"

#include <stdexcept>
#include <utility>
#include "fqe/data_type.hpp"

namespace fqe {

    Schema::Schema(std::vector <Field> fields) : fields_(std::move(fields)) {
        
        if (fields_.empty()){
            throw std::invalid_argument ("Schema empty - cannot occur");
        }

        index_by_name_.reserve(fields_.size());

        for (std::size_t i = 0; i < fields_.size(); i++){

            if (fields_[i].name.empty()){
                throw std::invalid_argument ("Field name empty - cannot occur");
            }

            bool inserted = index_by_name_.emplace(fields_[i].name, i).second;

            if (!inserted){
                throw std::invalid_argument ("Duplicate field name in schema: " 
                    + fields_[i].name);
            }

        }

    }

    std::size_t Schema::size() const noexcept {
        return fields_.size();
    }

    const Field& Schema::field (std:: size_t index) const {

        if (index >= fields_.size()){
            throw std::out_of_range("Field index out of range");
        }

        return fields_[index];
    }

    std::size_t Schema::index_of (const std::string& name) const {

        auto iterator = index_by_name_.find(name);

        if (iterator == index_by_name_.end()){
            throw std::out_of_range ("Unknown Field: " + name);
        }

        return iterator->second; // return index of corresponding name
    }

    const Field& Schema::field(const std::string& name) const {
        return fields_[index_of(name)];
    }

    std::ostream& operator << (std::ostream& output, const Schema& schema){
        output << "Schema(" << schema.size() << "fields present)\n";

        for (std::size_t i = 0; i < schema.size(); i++){
            const Field& current_field = schema.field(i);

            output << '[' << i << "] " << current_field.name << ": "
             << to_string(current_field.type) << '\n';
        }

        return output;

    }


}