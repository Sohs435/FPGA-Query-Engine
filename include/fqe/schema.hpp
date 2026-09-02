# pragma once

#include <cstddef>
#include <ostream>
#include <string>
#include <unordered_map>
#include <vector>

#include "field.hpp"

namespace fqe {

    class Schema {
        
        public: 
            explicit Schema(std::vector<Field> fields); // constructor of schema
            // explicit prevents accidental implicit conversion from a vector into a schema

            //i.e. if we have a vector fields
            // and we define a function process_schema (const Schema& schema);
            // process_schema(fields)); will not work
            // process_schema(Schema(fields)); will work

            std::size_t size() const noexcept; // calling .size() on a schema will 
            // return the number of fields in the schema
            // const noexcept means size() will NEVER modify schema obj and it 
            // will not throw an exception

            const Field& field(std::size_t index) const; // lookup field by index
            // .field(i) returns column description at index i

            // const Field& means the function returns a reference to the original Field
            //stored in fields and not a copy
            // additionally we cannot modify the returned field due to const present

            const Field& field(const std::string& name) const;
            // looking up a field by name .field(name) and returns a reference
            // to the original field and not a copy

            std::size_t index_of (const std::string& name) const; 
            // returns the value that the key name points to 

        private: 
            std::vector<Field> fields_; // ordered vector of Fields
            // each element contains {std::string label, DataType:: type} o

            //schema field 0 -> table column 0 
            //schema field 1 -> table column 1 and so on

            std::unordered_map<std::string, std::size_t> index_by_name_; // name -> index map
            // ex: "price" : 0 
            // key : value pair 

    };

    std::ostream& operator<<(std::ostream& output, const Schema& schema); //allows printing
    // of schema (std::cout << schema;)
}