#pragma once

#include <string>

#include "schema.hpp"
#include "table.hpp"

namespace fqe {

    Table load_csv (const std::string& file_path, Schema schema); //return table from CSV given 
    // file path and schema

    
}