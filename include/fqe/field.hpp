#pragma once

#include <string> 

#include "data_type.hpp"

namespace fqe {
    struct Field {
        std::string name; 
        DataType type; 
    
    };
}

// Ex. 
// Field price_field{"price", Datatype::Int64};
// we use struct since its just a small collection 
// of related vals without much complexity in behaviour


