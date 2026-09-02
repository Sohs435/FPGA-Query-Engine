#include "fqe/data_type.hpp"

namespace fqe {

    std::string_view to_string(DataType type) noexcept {

        switch (type) {

            case DataType::Int32:
                return "Int32";

            case DataType::Int64:
                return "Int64";
        }

        return "Unknown";
    }

}