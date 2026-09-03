#include "fqe/schema.hpp"

#include <stdexcept>
#include <utility>
#include "fqe/data_type.hpp"

namespace fqe {

    // constructor of schema
    // compiler auto generates destructor so no need to worry about that
    // class only owns resource managing standard-library objects so by rule of zero
    // probs shouldnt worry about destructor 
    Schema::Schema(std::vector <Field> fields) : fields_(std::move(fields)) {
        
        if (fields_.empty()){
            throw std::invalid_argument ("Schema empty - cannot occur");
        }

        index_by_name_.reserve(fields_.size()); // essentially reserve n entries for size n fields_
        // i.e map prepares internal bucket capacity for n entries to avoid rehashing 

        for (std::size_t i = 0; i < fields_.size(); i++){

            if (fields_[i].name.empty()){
                throw std::invalid_argument ("Field name empty - cannot occur");
            }

            bool inserted = index_by_name_.emplace(fields_[i].name, i).second; // if entry
            // alr exists in hash map return false since emplace will add a pair if its not 
            // alr present

            if (!inserted){
                throw std::invalid_argument ("Duplicate field name in schema: " 
                    + fields_[i].name);
            }

        }

    }

    // fields_ is a private vector so this is a safe way to find the number of fields in 
    // schema by calling some_schema.size()
    std::size_t Schema::size() const noexcept {
        return fields_.size();
    }

    // returns field at index - index by calling some_schema.field(index)
    const Field& Schema::field (std:: size_t index) const {

        if (index >= fields_.size()){
            throw std::out_of_range("Field index out of range");
        }

        return fields_[index];
    }
    // return index of field with some label name, assuming it exists within the schema
    std::size_t Schema::index_of (const std::string& name) const {

        auto iterator = index_by_name_.find(name);

        if (iterator == index_by_name_.end()){
            throw std::out_of_range ("Unknown Field: " + name);
        }

        return iterator->second; // return index of corresponding name
    }

    // return field with label name using .field(name)
    const Field& Schema::field(const std::string& name) const {
        return fields_[index_of(name)];
    }

    std::ostream& operator << (std::ostream& output, const Schema& schema){
        output << "Schema (" << schema.size() << " fields present)\n";

        for (std::size_t i = 0; i < schema.size(); i++){
            const Field& current_field = schema.field(i);

            output << '[' << i << "] " << current_field.name << ": "
             << to_string(current_field.type) << '\n';
        }

        return output;

    }


}