// defines supported database datatypes i.e int_32 and int_64

# pragma once

# include <string_view>

namespace fqe{
    enum class DataType{Int32, Int64};

    std::string_view to_string(DataType type);
    // convert type to readable text cuz we will eventually
    // need engine to print schemas and error messages 
}

// enum allows for better representation of types without
// relying on strings because a string could contain an 
// invalid value eg. std::string type = "integre";
// with enum only declared values are valid

// enum class provides distinct type, named values, protection
// from accidental integer conversion & scope thru DataType::

// i.e 
// DataType::Int32 will work
// Int32 will not

// this makes meaning clear and prevents naming conflict

