#pragma once

#include <string>
#include <unordered_map>

#include "query_executor.hpp"
#include "table.hpp"

namespace fqe {

    class QueryEngine {

        public:
            void add_table(std::string table_name, Table table);

            QueryResult execute(const std::string& query_text) const;

        private:
            const Table& find_table(
                const std::string& table_name) const;

            std::unordered_map<std::string, Table> tables_;
    };

}