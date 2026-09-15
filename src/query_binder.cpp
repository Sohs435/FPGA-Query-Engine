#include "fqe/query_binder.hpp"

#include <algorithm>
#include <stdexcept>
#include <utility>

namespace fqe {
    namespace {
        bool contains_column_index (const std::vector <std::size_t>& indices,
             std::size_t target) {
            
            return std::find(indices.begin(), indices.end(), target) != indices.end();
        }

        bool uses_only_grouped_columns(const BoundScalarExpression& expression, 
            const std::vector<std::size_t>& group_by_indices) { 
            
            
            if (const auto* column = std::get_if<BoundColumnReference>(&expression.node)) {
                return contains_column_index(group_by_indices, (*column).column_index);
            }

            if (std::holds_alternative<BoundIntegerLiteral>(expression.node)) {
                return true;
            }

            const auto& arithmetic = std::get<BoundArithmeticExpression>(expression.node);

            return uses_only_grouped_columns(*arithmetic.left, group_by_indices) &&
                uses_only_grouped_columns(*arithmetic.right,group_by_indices);

        }
    }

    BoundQuery bind_query(const ParsedQuery& query, const Schema& schema) {
        if (query.select_items.empty()) {
            throw std::invalid_argument("Query must contain a minimum of one SELECT item ");
        }

        BoundQuery bound_query; 

        bound_query.table_name = query.table_name; 

        //bind GROUP BY column names to indices 
        for (const std::string& column_name : query.group_by_columns) {
            const std::size_t column_index = schema.index_of(column_name);
            
            //duplicate column present in lits of GROUP BY columns since index is repeated
            if (contains_column_index(bound_query.group_by_column_indices, column_index)){
                throw std::invalid_argument("Duplicate GROUP BY column: " + column_name);
            }
            //add index into GROUP BY index vector 
            bound_query.group_by_column_indices.push_back(column_index);
        }

        //Bind SELECT items

        for (const SelectItem& item : query.select_items) {
            BoundSelectItem bound_select_item;  
            bound_select_item.aggregate = item.aggregate; 
            bound_select_item.is_star = item.is_star; //COUNT (*) -> True 
            bound_select_item.alias = item.alias; 

            if (item.is_star) {
                if (!item.aggregate.has_value() || item.aggregate.value() != AggregateFunction::Count) {
                    throw std::invalid_argument ("Only count uses * (not to be confused with multiplication)");
                }
                
                //SUM(...) -> ... = expression COUNT(*) has no such thing so expression = nullptr 
                if (item.expression != nullptr) {
                    throw std::invalid_argument("Count(*) must not contain scalar expression");
                }
            }

            else {
                if (item.expression == nullptr) {
                    throw std::invalid_argument ("SELECT item missing an expression when it isn't COUNT");
                }

                bound_select_item.expression = bind_scalar_expression(schema, *item.expression); 
            }

            bound_query.select_items.push_back(std::move(bound_select_item));
        }

        if (query.where_expression != nullptr) {
            bound_query.where_expression = bind_predicate_expression(schema, *query.where_expression);
        }

        const bool contains_aggregate = std::any_of(bound_query.select_items.begin(), bound_query.select_items.end(),
            [](const BoundSelectItem& item) {return item.aggregate.has_value();}); 
        
        const bool is_grouped_query = contains_aggregate || !bound_query.group_by_column_indices.empty();

        if (is_grouped_query) {
            for (const BoundSelectItem& item : bound_query.select_items) {

                if (item.aggregate.has_value()) {
                    continue; 
                }

                if (item.expression == nullptr) {
                    throw std::invalid_argument("Non-aggregate SELECT item has no expression");
                }

                if (!uses_only_grouped_columns(*item.expression, bound_query.group_by_column_indices)) {
                    throw std::invalid_argument("Non-aggregate SELECT expression is a column that is not present in GROUP BY");
                }
            }
        }

        return bound_query; 
    }
}