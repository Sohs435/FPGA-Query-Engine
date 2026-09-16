#include "fqe/query_engine.hpp"

#include <stdexcept>
#include <utility>

#include "fqe/parser.hpp"
#include "fqe/query_binder.hpp"
#include "fqe/tokenizer.hpp"

namespace fqe {

    void QueryEngine::add_table(std::string table_name, Table table) {

        if (table_name.empty()) {
            throw std::invalid_argument("Table name cannot be empty");
            }

        if (tables_.find(table_name) != tables_.end()) {
            throw std::invalid_argument("Table already exists: " + table_name);
        }

        tables_.emplace(std::move(table_name), std::move(table));
    }

    const Table& QueryEngine::find_table(
        const std::string& table_name) const {

        const auto table_iterator = tables_.find(table_name);

        if (table_iterator == tables_.end()) {
            throw std::invalid_argument("Unknown table: " + table_name);
        }

        return table_iterator->second;
    }

    QueryResult QueryEngine::execute(
        const std::string& query_text) const {

        Tokenizer tokenizer(query_text);

        Parser parser(tokenizer.tokenize());

        ParsedQuery parsed_query = parser.parse_query();

        const Table& table = find_table(parsed_query.table_name);

        BoundQuery bound_query = bind_query(parsed_query, table.schema());

        return execute_query(table, bound_query);
    }

}