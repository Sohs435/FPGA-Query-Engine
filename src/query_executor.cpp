#include "fqe/query_executor.hpp"

#include <algorithm>
#include <cstdint>
#include <functional>
#include <limits>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <utility>
#include <variant>
#include <vector>


namespace fqe {
    namespace {
        // SELECT ... 
        // FROM ...
        // WHERE ... -> predicate expression -> output a mask wrt which rows actually satisfy predicate expression
        //checks whether row passed WHERE condition using selection mask 
        bool row_is_selected(const SelectionMask& mask, std::size_t row_index) {
            return mask[row_index] != 0;
        }

        //number of rows that passed WHERE -> count number of 1s in mask using a lambda that returns whether mask unit is 
        //is non zero and iterates thru entire mask 
        std::size_t count_selected_rows(const SelectionMask& mask) {
            return static_cast<std::size_t>(std::count_if(mask.begin(), mask.end(),
                [](std::uint8_t selected) {return selected != 0;}));
        }

        //check for overflows 
        //used when accumulating SUM terms
        std::int64_t checked_add(std::int64_t left_value,
            std::int64_t right_value) {

            const std::int64_t maximum =
                std::numeric_limits<std::int64_t>::max(); //2**63 - 1

            const std::int64_t minimum =
                std::numeric_limits<std::int64_t>::min(); //-2**63

            const bool overflows =
                (right_value > 0 && left_value > maximum - right_value) || //addition check overflow 
                (right_value < 0 && left_value < minimum - right_value); //subtraction check underflow

            if (overflows) {
                throw std::overflow_error(
                    "Integer overflow/underflow during aggregate addition");
            }

            return left_value + right_value;//return sum as long as it doesnt overflow (subtraction is dealt with 
            // by converting positive numbers to negative in parser)
        }

        //convert evaluated values into correct column storage
        //Like for SELECT price from TRADES or SELECT price * quantity from TRADES
        Column create_result_column(DataType result_type,
            std::vector<std::int64_t> values) {

            if (result_type == DataType::Int64) {
                return Column(std::move(values));
            }

            if (result_type == DataType::Int32) {
                std::vector<std::int32_t> narrowed_values;

                narrowed_values.reserve(values.size());

                const std::int64_t minimum =
                    std::numeric_limits<std::int32_t>::min();

                const std::int64_t maximum =
                    std::numeric_limits<std::int32_t>::max();

                for (std::int64_t value : values) {

                    if (value < minimum || value > maximum) {
                        throw std::overflow_error(
                            "Result value does not fit in Int32");
                    }

                    narrowed_values.push_back(
                        static_cast<std::int32_t>(value));
                }

                return Column(std::move(narrowed_values));
            }

            throw std::invalid_argument("Unsupported result data type");
        }

        //MAX, MIN, SUM, COUNT -> SELECT aggregate
        std::string aggregate_name(AggregateFunction aggregate) {

            switch (aggregate) {
                case AggregateFunction::Count:
                    return "count";

                case AggregateFunction::Sum:
                    return "sum";

                case AggregateFunction::Min:
                    return "min";

                case AggregateFunction::Max:
                    return "max";
            }

            throw std::invalid_argument("Unsupported aggregate function");
        }
        
        //checks whether chosen output name has already been used -> cannot have duplicate field names
        bool output_name_exists(const std::vector<Field>& fields,
            const std::string& name) {

            return std::any_of(fields.begin(), fields.end(),
                [&] (const Field& field) {return field.name == name;});
        }

        //evert result column has a unique name
        //if we have SELECT price, price FROM trades
        //result column names would be price, price_2
        std::string make_unique_output_name(std::string base_name,
            const std::vector<Field>& fields) {

            if (!output_name_exists(fields, base_name)) {
                return base_name;
            }

            std::size_t suffix = 2;

            while (true) {
                std::string candidate =
                    base_name + "_" + std::to_string(suffix);

                if (!output_name_exists(fields, candidate)) {
                    return candidate;
                }

                suffix++;
            }
        }
        //choose result column name by priority explicit alias -> aggregate bane -> og column name -> expression
        // price AS p -> p
        // SUM(price) -> sum
        // price -> price
        //price * quantity -> expression
        std::string default_output_name(const Table& table,
            const BoundSelectItem& item) {

            if (item.alias.has_value()) {
                return item.alias.value();
            }

            if (item.aggregate.has_value()) {
                return aggregate_name(item.aggregate.value());
            }

            if (item.expression == nullptr) {
                throw std::invalid_argument(
                    "SELECT item has no scalar expression");
            }

            const auto* column = std::get_if<BoundColumnReference>(
                &item.expression->node);

            if (column != nullptr) {
                return table.schema().field(column->column_index).name;
            }

            return "expression";
        }
        //return data type of aggregate result
        //SUM -> int64
        //COUNT -> int64
        //MIN, MAX -> input expression type
        DataType aggregate_result_type(const BoundSelectItem& item) {

            if (!item.aggregate.has_value()) {
                throw std::invalid_argument(
                    "SELECT item is not an aggregate");
            }

            switch (item.aggregate.value()) {
                case AggregateFunction::Count:
                case AggregateFunction::Sum:
                    return DataType::Int64;

                case AggregateFunction::Min:
                case AggregateFunction::Max:

                    if (item.expression == nullptr) {
                        throw std::invalid_argument(
                            "Aggregate requires an expression");
                    }

                    return item.expression->result_type;
            }

            throw std::invalid_argument("Unsupported aggregate function");
        }
        //execute 1 aggregate select item over SELECT
        // SELECT COUNT(price * quantity) FROM trades -> execute count 
        std::int64_t execute_aggregate(const Table& table,
            const BoundSelectItem& item, const SelectionMask& mask) {

            if (!item.aggregate.has_value()) {
                throw std::invalid_argument(
                    "SELECT item is not an aggregate");
            }

            const AggregateFunction aggregate = item.aggregate.value();

            if (aggregate == AggregateFunction::Count) {
                std::int64_t count = 0;

                for (std::size_t row_index = 0;
                     row_index < table.row_count(); row_index++) {

                    if (!row_is_selected(mask, row_index)) {
                        continue;
                    }

                    // COUNT(expression) evaluates the expression.
                    // COUNT(*) does not have an expression.
                    if (!item.is_star) {

                        if (item.expression == nullptr) {
                            throw std::invalid_argument(
                                "COUNT expression is missing");
                        }

                        static_cast<void>(evaluate_scalar_expression(
                            table, *item.expression, row_index));
                    }

                    if (count ==
                        std::numeric_limits<std::int64_t>::max()) {

                        throw std::overflow_error(
                            "COUNT result exceeds Int64");
                    }

                    count++;
                }

                return count;
            }

            if (item.expression == nullptr) {
                throw std::invalid_argument(
                    "Aggregate expression is missing");
            }

            if (aggregate == AggregateFunction::Sum) {
                std::int64_t total = 0;

                for (std::size_t row_index = 0;
                     row_index < table.row_count(); row_index++) {

                    if (!row_is_selected(mask, row_index)) {
                        continue;
                    }

                    const std::int64_t value =
                        evaluate_scalar_expression(
                            table, *item.expression, row_index);

                    total = checked_add(total, value);
                }

                return total;
            }

            bool value_found = false;
            std::int64_t result = 0;

            for (std::size_t row_index = 0;
                 row_index < table.row_count(); row_index++) {

                if (!row_is_selected(mask, row_index)) {
                    continue;
                }

                const std::int64_t value =
                    evaluate_scalar_expression(
                        table, *item.expression, row_index);

                if (!value_found) {
                    result = value;
                    value_found = true;
                    continue;
                }

                if (aggregate == AggregateFunction::Min) {
                    result = std::min(result, value);
                }

                else if (aggregate == AggregateFunction::Max) {
                    result = std::max(result, value);
                }
            }

            if (!value_found) {
                throw std::domain_error(
                    "MIN or MAX cannot operate on an empty selection");
            }

            return result;
        }

        QueryResult execute_projection_query(const Table& table,
            const BoundQuery& query, const SelectionMask& mask) {

            std::vector<Field> result_fields;
            std::vector<Column> result_columns;

            result_fields.reserve(query.select_items.size());
            result_columns.reserve(query.select_items.size());

            const std::size_t selected_rows =
                count_selected_rows(mask);

            for (const BoundSelectItem& item : query.select_items) {

                if (item.aggregate.has_value()) {
                    throw std::invalid_argument(
                        "Aggregate found in projection query");
                }

                if (item.expression == nullptr) {
                    throw std::invalid_argument(
                        "Projection expression is missing");
                }

                std::vector<std::int64_t> values;

                values.reserve(selected_rows);

                for (std::size_t row_index = 0;
                     row_index < table.row_count(); row_index++) {

                    if (!row_is_selected(mask, row_index)) {
                        continue;
                    }

                    values.push_back(evaluate_scalar_expression(
                        table, *item.expression, row_index));
                }

                const DataType result_type =
                    item.expression->result_type;

                std::string output_name = make_unique_output_name(
                    default_output_name(table, item), result_fields);

                result_fields.push_back(
                    Field{std::move(output_name), result_type});

                result_columns.push_back(create_result_column(
                    result_type, std::move(values)));
            }

            Schema result_schema(std::move(result_fields));

            return QueryResult(
                std::move(result_schema),
                std::move(result_columns));
        }
        //executes a query containing atleast 1 aggregate
        QueryResult execute_aggregate_query(const Table& table,
            const BoundQuery& query, const SelectionMask& mask) {

            std::vector<Field> result_fields;
            std::vector<Column> result_columns;

            result_fields.reserve(query.select_items.size());
            result_columns.reserve(query.select_items.size());

            for (const BoundSelectItem& item : query.select_items) {
                std::int64_t value;
                DataType result_type;

                if (item.aggregate.has_value()) {
                    value = execute_aggregate(table, item, mask);
                    result_type = aggregate_result_type(item);
                }

                else {

                    if (item.expression == nullptr) {
                        throw std::invalid_argument(
                            "SELECT expression is missing");
                    }

                    // The binder only permits literal expressions here.
                    value = evaluate_scalar_expression(
                        table, *item.expression, 0);

                    result_type = item.expression->result_type;
                }

                std::string output_name = make_unique_output_name(
                    default_output_name(table, item), result_fields);

                result_fields.push_back(
                    Field{std::move(output_name), result_type});

                result_columns.push_back(create_result_column(
                    result_type, std::vector<std::int64_t>{value}));
            }

            Schema result_schema(std::move(result_fields));

            return QueryResult(
                std::move(result_schema),
                std::move(result_columns));
        }

        using GroupKey = std::vector<std::int64_t>;

        struct GroupKeyHash {
            std::size_t operator()(const GroupKey& key) const noexcept {
                std::size_t result = 0;

                for (std::int64_t value : key) {
                    const std::size_t value_hash =
                        std::hash<std::int64_t>{}(value);

                    result ^= value_hash +
                        static_cast<std::size_t>(0x9e3779b9U) +
                        (result << 6) + (result >> 2);
                }

                return result;
            }
        };

        struct GroupAggregateState {
            std::int64_t count = 0;
            std::int64_t sum = 0;
            std::int64_t minimum = 0;
            std::int64_t maximum = 0;
            bool has_value = false;
        };

        struct GroupState {
            std::size_t representative_row;
            std::vector<GroupAggregateState> aggregate_states;
        };

        std::int64_t read_group_column(const Table& table,
            std::size_t column_index, std::size_t row_index) {

            const Column& column = table.column(column_index);

            return std::visit(
                [row_index] (const auto& values) -> std::int64_t {

                    if (row_index >= values.size()) {
                        throw std::out_of_range(
                            "GROUP BY row index is out of range");
                    }

                    return static_cast<std::int64_t>(
                        values[row_index]);
                },

                column.data()
            );
        }

        GroupKey create_group_key(const Table& table,
            const BoundQuery& query, std::size_t row_index) {

            GroupKey key;

            key.reserve(query.group_by_column_indices.size());

            for (std::size_t column_index :
                 query.group_by_column_indices) {

                key.push_back(read_group_column(
                    table, column_index, row_index));
            }

            return key;
        }

        void update_group_aggregate(const Table& table,
            const BoundSelectItem& item, std::size_t row_index,
            GroupAggregateState& state) {

            if (!item.aggregate.has_value()) {
                throw std::invalid_argument(
                    "Cannot update a non-aggregate SELECT item");
            }

            const AggregateFunction aggregate =
                item.aggregate.value();

            if (aggregate == AggregateFunction::Count) {

                if (!item.is_star) {

                    if (item.expression == nullptr) {
                        throw std::invalid_argument(
                            "COUNT expression is missing");
                    }

                    static_cast<void>(evaluate_scalar_expression(
                        table, *item.expression, row_index));
                }

                if (state.count ==
                    std::numeric_limits<std::int64_t>::max()) {

                    throw std::overflow_error(
                        "Grouped COUNT result exceeds Int64");
                }

                state.count++;
                return;
            }

            if (item.expression == nullptr) {
                throw std::invalid_argument(
                    "Grouped aggregate expression is missing");
            }

            const std::int64_t value =
                evaluate_scalar_expression(
                    table, *item.expression, row_index);

            switch (aggregate) {
                case AggregateFunction::Sum:
                    state.sum = checked_add(state.sum, value);
                    return;

                case AggregateFunction::Min:

                    if (!state.has_value || value < state.minimum) {
                        state.minimum = value;
                    }

                    state.has_value = true;
                    return;

                case AggregateFunction::Max:

                    if (!state.has_value || value > state.maximum) {
                        state.maximum = value;
                    }

                    state.has_value = true;
                    return;

                case AggregateFunction::Count:
                    break;
            }

            throw std::invalid_argument(
                "Unsupported grouped aggregate function");
        }

        std::int64_t finish_group_aggregate(const BoundSelectItem& item,
            const GroupAggregateState& state) {

            if (!item.aggregate.has_value()) {
                throw std::invalid_argument(
                    "Cannot finish a non-aggregate SELECT item");
            }

            switch (item.aggregate.value()) {
                case AggregateFunction::Count:
                    return state.count;

                case AggregateFunction::Sum:
                    return state.sum;

                case AggregateFunction::Min:

                    if (!state.has_value) {
                        throw std::domain_error(
                            "Grouped MIN has no input value");
                    }

                    return state.minimum;

                case AggregateFunction::Max:

                    if (!state.has_value) {
                        throw std::domain_error(
                            "Grouped MAX has no input value");
                    }

                    return state.maximum;
            }

            throw std::invalid_argument(
                "Unsupported grouped aggregate function");
        }

        QueryResult execute_grouped_query(const Table& table,
            const BoundQuery& query, const SelectionMask& mask) {

            std::unordered_map<GroupKey, std::size_t,
                GroupKeyHash> group_indices;

            std::vector<GroupState> groups;

            for (std::size_t row_index = 0;
                 row_index < table.row_count(); row_index++) {

                if (!row_is_selected(mask, row_index)) {
                    continue;
                }

                GroupKey group_key = create_group_key(
                    table, query, row_index);

                auto group_iterator =
                    group_indices.find(group_key);

                std::size_t group_index;

                if (group_iterator == group_indices.end()) {
                    group_index = groups.size();

                    group_indices.emplace(
                        std::move(group_key), group_index);

                    groups.push_back(GroupState{
                        row_index,
                        std::vector<GroupAggregateState>(
                            query.select_items.size())});
                }

                else {
                    group_index = group_iterator->second;
                }

                GroupState& group = groups[group_index];

                for (std::size_t select_index = 0;
                     select_index < query.select_items.size();
                     select_index++) {

                    const BoundSelectItem& item =
                        query.select_items[select_index];

                    if (item.aggregate.has_value()) {
                        update_group_aggregate(table, item, row_index,
                            group.aggregate_states[select_index]);
                    }
                }
            }

            std::vector<Field> result_fields;
            std::vector<Column> result_columns;

            result_fields.reserve(query.select_items.size());
            result_columns.reserve(query.select_items.size());

            for (std::size_t select_index = 0;
                 select_index < query.select_items.size();
                 select_index++) {

                const BoundSelectItem& item =
                    query.select_items[select_index];

                std::vector<std::int64_t> values;

                values.reserve(groups.size());

                DataType result_type;

                if (item.aggregate.has_value()) {
                    result_type = aggregate_result_type(item);

                    for (const GroupState& group : groups) {
                        values.push_back(finish_group_aggregate(
                            item, group.aggregate_states[select_index]));
                    }
                }

                else {

                    if (item.expression == nullptr) {
                        throw std::invalid_argument(
                            "Grouped SELECT expression is missing");
                    }

                    result_type = item.expression->result_type;

                    for (const GroupState& group : groups) {
                        values.push_back(evaluate_scalar_expression(
                            table, *item.expression,
                            group.representative_row));
                    }
                }

                std::string output_name = make_unique_output_name(
                    default_output_name(table, item), result_fields);

                result_fields.push_back(
                    Field{std::move(output_name), result_type});

                result_columns.push_back(create_result_column(
                    result_type, std::move(values)));
            }

            Schema result_schema(std::move(result_fields));

            return QueryResult(
                std::move(result_schema),
                std::move(result_columns));
        }

    }

    //validates that query contains SELECT item/s
    //creates selection mask with WHERE 
    //and then checks whether any select item is an aggregate 
    QueryResult execute_query(const Table& table,
        const BoundQuery& query) {

        if (query.select_items.empty()) {
            throw std::invalid_argument(
                "Bound query has no SELECT items");
        }

        SelectionMask mask;

        if (query.where_expression != nullptr) {
            mask = evaluate_predicate_expression(
                table, *query.where_expression);
        }

        else {
            mask = SelectionMask(
                table.row_count(), std::uint8_t{1});
        }

        if (mask.size() != table.row_count()) {
            throw std::invalid_argument(
                "Selection mask size does not match table");
        }

        if (!query.group_by_column_indices.empty()) {
            return execute_grouped_query(table, query, mask);
        }

        const bool contains_aggregate = std::any_of(
            query.select_items.begin(), query.select_items.end(),
            [] (const BoundSelectItem& item) {
                return item.aggregate.has_value();
            });

        if (contains_aggregate) {
            return execute_aggregate_query(table, query, mask);
        }

        return execute_projection_query(table, query, mask);
    }



}