#include "fqe/filter.hpp"

#include <algorithm>
#include <limits>
#include <stdexcept>
#include <utility>
#include <variant>

namespace fqe {

    namespace {

        template <typename... FunctionTypes>

        struct Overloaded : FunctionTypes... {
            using FunctionTypes::operator()...;
        };

        template <typename... FunctionTypes>

        Overloaded(FunctionTypes...) -> Overloaded<FunctionTypes...>;

        void require_scalar_child(
            const ScalarExpressionPtr& child,
            const std::string& expression_name) {

            if (child == nullptr){
                throw std::invalid_argument(expression_name + " requires a scalar expression");
            }
        }

        void require_predicate_child(
            const PredicateExpressionPtr& child,
            const std::string& expression_name) {

            if (child == nullptr){
                throw std::invalid_argument(expression_name + " requires a predicate expression");
            }
        }

        bool compare_values(
            std::int64_t left_value,
            ComparisonOperator comparison,
            std::int64_t right_value) {

            switch (comparison) {

                case ComparisonOperator::Equal:
                    return left_value == right_value;

                case ComparisonOperator::NotEqual:
                    return left_value != right_value;

                case ComparisonOperator::LessThan:
                    return left_value < right_value;

                case ComparisonOperator::LessEqual:
                    return left_value <= right_value;

                case ComparisonOperator::GreaterThan:
                    return left_value > right_value;

                case ComparisonOperator::GreaterEqual:
                    return left_value >= right_value;
            }

            throw std::invalid_argument(
                "Unsupported comparison operator"
            );
        }

        // check if left_value + right_value > maximum 64 bit value or < minimum 64 bit value i.e 2**63 - 1 and -2^63 
        // we do this by checking left_value > maximum - right_value || left_value < minimum - right_value 
        bool addition_overflows(
            std::int64_t left_value,
            std::int64_t right_value) {

            const std::int64_t maximum =
                std::numeric_limits<std::int64_t>::max();

            const std::int64_t minimum =
                std::numeric_limits<std::int64_t>::min();

            return (right_value > 0 &&
                left_value > maximum - right_value) ||
                (right_value < 0 &&
                left_value < minimum - right_value);
        }

        // left_value - right_value > max || left_value - right_value < min 
        bool subtraction_overflows(
            std::int64_t left_value,
            std::int64_t right_value) {

            const std::int64_t maximum = std::numeric_limits<std::int64_t>::max();

            const std::int64_t minimum = std::numeric_limits<std::int64_t>::min();

            return (right_value > 0 && left_value < minimum + right_value) ||
             (right_value < 0 && left_value > maximum + right_value);
        }

        // left_value * right_value > max -> left_value > max / right_value and conv for min 
        // cannot divide is right = 0 or left = 0 -> inf so just always gonna be false since prod is 0
        // if left or right is - 1 we can have 2**63 since -2**63 * - 1 = 2**63 which is an overflow
        // case. Wont happen in the opposite scenario since max = 2**63 - 1 in signed 2's comp 
        bool multiplication_overflows(
            std::int64_t left_value,
            std::int64_t right_value) {

            const std::int64_t maximum = std::numeric_limits<std::int64_t>::max();

            const std::int64_t minimum = std::numeric_limits<std::int64_t>::min();

            if (left_value == 0 || right_value == 0){
                return false;
            }

            if (left_value == -1){
                return right_value == minimum;
            }

            if (right_value == -1){
                return left_value == minimum;
            }

            if (left_value > 0){

                if (right_value > 0){
                    return left_value > maximum / right_value;
                }

                return right_value < minimum / left_value;
            }

            if (right_value > 0){
                return left_value < minimum / right_value;
            }

            return left_value < maximum / right_value;
        }

            std::int64_t calculate_arithmetic(
            ArithmeticOperator operation,
            std::int64_t left_value,
            std::int64_t right_value) {
            
            // add/sub/mul/divide -> throw if overflow 
            switch (operation) {

                case ArithmeticOperator::Add:

                    if (addition_overflows(
                        left_value, right_value)) {

                        throw std::overflow_error("Integer overflow during addition");
                    }

                    return left_value + right_value;

                case ArithmeticOperator::Subtract:

                    if (subtraction_overflows(
                        left_value, right_value)) {

                        throw std::overflow_error("Integer overflow during subtraction");
                    }

                    return left_value - right_value;

                case ArithmeticOperator::Multiply:

                    if (multiplication_overflows(
                        left_value, right_value)) {

                        throw std::overflow_error("Integer overflow during multiplication");
                    }

                    return left_value * right_value;

                case ArithmeticOperator::Divide:
                    // cannot divide by 0
                    if (right_value == 0){
                        throw std::domain_error("Division by zero in scalar expression");
                    }

                    // causes 2**63 case which will overflow if right = -1 and left = min = -2**63 
                    // -> l/r = 2**63
                    if (left_value ==std::numeric_limits<std::int64_t>::min() &&
                     right_value == -1) {

                        throw std::overflow_error("Integer overflow during division");
                    }

                    return left_value / right_value;
            }

            throw std::invalid_argument("Unsupported arithmetic operator");
        }

        //Convert unbound scalar-expression tree -> bound scalar expression tree
        //Say we have: price * quantity -> Multiply -> ColumnReference ("price")
                                     // |---> ColumnReference ("quantity")
        // want to make "price" -> index = 0, type = some type
        //              "quantity" -> index = 1, type = some type
        // for example 
        
        //But like how?
        //Im thinking we take schema which gives name and index for each relevant column 
        //expression is unbound scalar expression 
        // select node type via std::visit(overloaded{type1, type2, type3}, node) as
        // expression.node is one of a column ref, integer literal, arithmetic expression

        BoundScalarExpressionPtr bind_scalar_expression(
            const Schema& schema,
            const ScalarExpression& expression) {

            return std::visit(
                Overloaded{
                    // capture all variables in lambda by ref -> as lambda will need schema
                    // for index and datatype
                    // current node is column ref ex. ColumnReference{"price"}
                    [&] (const ColumnReference& column)
                        -> BoundScalarExpressionPtr {

                        // find index from schema
                        std::size_t column_index = schema.index_of(column.column_name);
                        
                        // find data type from schema 
                        DataType column_type = schema.field(column_index).type;
                        
                        // create bound reference
                        // ColumnReference("price") -> BoundColumnReference(0, some type)
                        BoundColumnReference bound_column{column_index, column_type};
                        
                        // construct node and return its ptr
                        return std::make_unique<BoundScalarExpression>(BoundScalarNode{
                         std::move(bound_column)}, column_type);
                    },

                    // no need for schema or expression cuz we dont need type or index
                    // for something that is not part of the table in itself and only 
                    // exists in external query -> so we dont pass any variable surrounding
                    // lambda by ref 
                    [] (const IntegerLiteral& literal) -> BoundScalarExpressionPtr {

                        //construct BoundIntegerLiteral structure which then is used 
                        // to make the scalar expression ptr 
                        BoundIntegerLiteral bound_literal{literal.value};

                        return std::make_unique<BoundScalarExpression>(BoundScalarNode{
                         std::move(bound_literal)}, DataType::Int64);
                    },

                    [&] (const ArithmeticExpression& arithmetic) -> BoundScalarExpressionPtr {

                        if (arithmetic.left == nullptr || arithmetic.right == nullptr) {

                            throw std::invalid_argument("Arithmetic expression has a null child");
                        }

                        BoundScalarExpressionPtr left = bind_scalar_expression(schema, 
                         *arithmetic.left);

                        BoundScalarExpressionPtr right = bind_scalar_expression(schema, 
                         *arithmetic.right);

                        BoundArithmeticExpression bound_arithmetic{arithmetic.operation,
                         std::move(left), std::move(right)};

                        return std::make_unique<BoundScalarExpression>(BoundScalarNode{
                         std::move(bound_arithmetic)}, DataType::Int64);
                    }

                },

                expression.node // store created node here 
            );
        }

        BoundPredicateExpressionPtr bind_predicate_node(const Schema& schema, 
         const PredicateExpression& expression) {

            return std::visit(
                Overloaded{

                    [&] (const ComparisonExpression& comparison)
                        -> BoundPredicateExpressionPtr {

                        if (comparison.left == nullptr ||
                            comparison.right == nullptr) {

                            throw std::invalid_argument(
                                "Comparison expression has a null child"
                            );
                        }

                        BoundComparisonExpression bound_comparison{
                            bind_scalar_expression(
                                schema, *comparison.left),
                            comparison.comparison,
                            bind_scalar_expression(
                                schema, *comparison.right)
                        };

                        return std::make_unique<
                            BoundPredicateExpression>(
                                BoundPredicateNode{
                                    std::move(bound_comparison)
                                }
                            );
                    },

                    [&] (const AndExpression& expression)
                        -> BoundPredicateExpressionPtr {

                        if (expression.left == nullptr ||
                            expression.right == nullptr) {

                            throw std::invalid_argument(
                                "AND expression has a null child"
                            );
                        }

                        BoundAndExpression bound_and{
                            bind_predicate_node(
                                schema, *expression.left),
                            bind_predicate_node(
                                schema, *expression.right)
                        };

                        return std::make_unique<
                            BoundPredicateExpression>(
                                BoundPredicateNode{
                                    std::move(bound_and)
                                }
                            );
                    },

                    [&] (const OrExpression& expression)
                        -> BoundPredicateExpressionPtr {

                        if (expression.left == nullptr ||
                            expression.right == nullptr) {

                            throw std::invalid_argument(
                                "OR expression has a null child"
                            );
                        }

                        BoundOrExpression bound_or{
                            bind_predicate_node(
                                schema, *expression.left),
                            bind_predicate_node(
                                schema, *expression.right)
                        };

                        return std::make_unique<
                            BoundPredicateExpression>(
                                BoundPredicateNode{
                                    std::move(bound_or)
                                }
                            );
                    },

                    [&] (const NotExpression& expression)
                        -> BoundPredicateExpressionPtr {

                        if (expression.child == nullptr){
                            throw std::invalid_argument(
                                "NOT expression has a null child"
                            );
                        }

                        BoundNotExpression bound_not{
                            bind_predicate_node(
                                schema, *expression.child)
                        };

                        return std::make_unique<
                            BoundPredicateExpression>(
                                BoundPredicateNode{
                                    std::move(bound_not)
                                }
                            );
                    },

                    [&] (const BetweenExpression& expression)
                        -> BoundPredicateExpressionPtr {

                        if (expression.value == nullptr){
                            throw std::invalid_argument(
                                "BETWEEN expression has a null value"
                            );
                        }

                        BoundBetweenExpression bound_between{
                            bind_scalar_expression(
                                schema, *expression.value),
                            expression.lower_bound,
                            expression.upper_bound,
                            expression.lower_inclusive,
                            expression.upper_inclusive
                        };

                        return std::make_unique<
                            BoundPredicateExpression>(
                                BoundPredicateNode{
                                    std::move(bound_between)
                                }
                            );
                    },

                    [&] (const InExpression& expression)
                        -> BoundPredicateExpressionPtr {

                        if (expression.value == nullptr){
                            throw std::invalid_argument(
                                "IN expression has a null value"
                            );
                        }

                        if (expression.candidates.empty()){
                            throw std::invalid_argument(
                                "IN expression requires candidates"
                            );
                        }

                        BoundInExpression bound_in{
                            bind_scalar_expression(
                                schema, *expression.value),
                            expression.candidates,
                            expression.negated
                        };

                        return std::make_unique<
                            BoundPredicateExpression>(
                                BoundPredicateNode{
                                    std::move(bound_in)
                                }
                            );
                    },

                    [] (const BooleanLiteral& literal)
                        -> BoundPredicateExpressionPtr {

                        return std::make_unique<
                            BoundPredicateExpression>(
                                BoundPredicateNode{literal}
                            );
                    }

                },

                expression.node
            );
        }

        std::int64_t evaluate_scalar_expression(
            const Table& table,
            const BoundScalarExpression& expression,
            std::size_t row_index) {

            return std::visit(
                Overloaded{

                    [&] (const BoundColumnReference& column)
                        -> std::int64_t {

                        const Column& table_column =
                            table.column(column.column_index);

                        if (table_column.type() !=
                            column.column_type) {

                            throw std::invalid_argument(
                                "Bound column type does not match table"
                            );
                        }

                        return std::visit(
                            [row_index] (const auto& values)
                                -> std::int64_t {

                                if (row_index >= values.size()){
                                    throw std::out_of_range(
                                        "Row index out of range"
                                    );
                                }

                                return static_cast<std::int64_t>(
                                    values[row_index]
                                );
                            },

                            table_column.data()
                        );
                    },

                    [] (const BoundIntegerLiteral& literal)
                        -> std::int64_t {

                        return literal.value;
                    },

                    [&] (
                        const BoundArithmeticExpression& arithmetic)
                        -> std::int64_t {

                        std::int64_t left_value =
                            evaluate_scalar_expression(
                                table, *arithmetic.left, row_index);

                        std::int64_t right_value =
                            evaluate_scalar_expression(
                                table, *arithmetic.right, row_index);

                        return calculate_arithmetic(
                            arithmetic.operation,
                            left_value,
                            right_value
                        );
                    }

                },

                expression.node
            );
        }

        bool evaluate_predicate_at_row(
            const Table& table,
            const BoundPredicateExpression& expression,
            std::size_t row_index) {

            return std::visit(
                Overloaded{

                    [&] (
                        const BoundComparisonExpression& comparison) {

                        std::int64_t left_value =
                            evaluate_scalar_expression(
                                table, *comparison.left, row_index);

                        std::int64_t right_value =
                            evaluate_scalar_expression(
                                table, *comparison.right, row_index);

                        return compare_values(
                            left_value,
                            comparison.comparison,
                            right_value
                        );
                    },

                    [&] (const BoundAndExpression& expression) {

                        return evaluate_predicate_at_row(
                            table, *expression.left, row_index) &&
                            evaluate_predicate_at_row(
                                table, *expression.right, row_index);
                    },

                    [&] (const BoundOrExpression& expression) {

                        return evaluate_predicate_at_row(
                            table, *expression.left, row_index) ||
                            evaluate_predicate_at_row(
                                table, *expression.right, row_index);
                    },

                    [&] (const BoundNotExpression& expression) {

                        return !evaluate_predicate_at_row(
                            table, *expression.child, row_index);
                    },

                    [&] (
                        const BoundBetweenExpression& expression) {

                        std::int64_t value =
                            evaluate_scalar_expression(
                                table, *expression.value, row_index);

                        bool above_lower_bound =
                            expression.lower_inclusive
                                ? value >= expression.lower_bound
                                : value > expression.lower_bound;

                        bool below_upper_bound =
                            expression.upper_inclusive
                                ? value <= expression.upper_bound
                                : value < expression.upper_bound;

                        return above_lower_bound &&
                            below_upper_bound;
                    },

                    [&] (const BoundInExpression& expression) {

                        std::int64_t value =
                            evaluate_scalar_expression(
                                table, *expression.value, row_index);

                        bool found = std::find(
                            expression.candidates.begin(),
                            expression.candidates.end(),
                            value
                        ) != expression.candidates.end();

                        return expression.negated
                            ? !found
                            : found;
                    },

                    [] (const BooleanLiteral& literal) {

                        return literal.value;
                    }

                },

                expression.node
            );
        }

    }

    ScalarExpression::ScalarExpression(ScalarNode node)
        : node(std::move(node)) {}

    PredicateExpression::PredicateExpression(PredicateNode node)
        : node(std::move(node)) {}

    BoundScalarExpression::BoundScalarExpression(
        BoundScalarNode node, DataType result_type)
        : node(std::move(node)), result_type(result_type) {}

    BoundPredicateExpression::BoundPredicateExpression(
        BoundPredicateNode node)
        : node(std::move(node)) {}

    ScalarExpressionPtr make_column_reference(
        std::string column_name) {

        if (column_name.empty()){
            throw std::invalid_argument(
                "Column reference cannot be empty"
            );
        }

        ColumnReference column{
            std::move(column_name)
        };

        return std::make_unique<ScalarExpression>(
            ScalarNode{std::move(column)}
        );
    }

    ScalarExpressionPtr make_integer_literal(
        std::int64_t value) {

        IntegerLiteral literal{
            value
        };

        return std::make_unique<ScalarExpression>(
            ScalarNode{literal}
        );
    }

    ScalarExpressionPtr make_arithmetic(
        ArithmeticOperator operation,
        ScalarExpressionPtr left,
        ScalarExpressionPtr right) {

        require_scalar_child(left, "Arithmetic expression");
        require_scalar_child(right, "Arithmetic expression");

        ArithmeticExpression arithmetic{
            operation,
            std::move(left),
            std::move(right)
        };

        return std::make_unique<ScalarExpression>(
            ScalarNode{std::move(arithmetic)}
        );
    }

    PredicateExpressionPtr make_comparison(
        ScalarExpressionPtr left,
        ComparisonOperator comparison,
        ScalarExpressionPtr right) {

        require_scalar_child(left, "Comparison expression");
        require_scalar_child(right, "Comparison expression");

        ComparisonExpression expression{
            std::move(left),
            comparison,
            std::move(right)
        };

        return std::make_unique<PredicateExpression>(
            PredicateNode{std::move(expression)}
        );
    }

    PredicateExpressionPtr make_comparison(
        ComparisonPredicate predicate) {

        return make_comparison(
            make_column_reference(
                std::move(predicate.column_name)),
            predicate.comparison,
            make_integer_literal(predicate.value)
        );
    }

    PredicateExpressionPtr make_and(
        PredicateExpressionPtr left,
        PredicateExpressionPtr right) {

        require_predicate_child(left, "AND expression");
        require_predicate_child(right, "AND expression");

        AndExpression expression{
            std::move(left),
            std::move(right)
        };

        return std::make_unique<PredicateExpression>(
            PredicateNode{std::move(expression)}
        );
    }

    PredicateExpressionPtr make_or(
        PredicateExpressionPtr left,
        PredicateExpressionPtr right) {

        require_predicate_child(left, "OR expression");
        require_predicate_child(right, "OR expression");

        OrExpression expression{
            std::move(left),
            std::move(right)
        };

        return std::make_unique<PredicateExpression>(
            PredicateNode{std::move(expression)}
        );
    }

    PredicateExpressionPtr make_not(
        PredicateExpressionPtr child) {

        require_predicate_child(child, "NOT expression");

        NotExpression expression{
            std::move(child)
        };

        return std::make_unique<PredicateExpression>(
            PredicateNode{std::move(expression)}
        );
    }

    PredicateExpressionPtr make_between(
        ScalarExpressionPtr value,
        std::int64_t lower_bound,
        std::int64_t upper_bound,
        bool lower_inclusive,
        bool upper_inclusive) {

        require_scalar_child(value, "BETWEEN expression");

        BetweenExpression expression{
            std::move(value),
            lower_bound,
            upper_bound,
            lower_inclusive,
            upper_inclusive
        };

        return std::make_unique<PredicateExpression>(
            PredicateNode{std::move(expression)}
        );
    }

    PredicateExpressionPtr make_between(
        std::string column_name,
        std::int64_t lower_bound,
        std::int64_t upper_bound,
        bool lower_inclusive,
        bool upper_inclusive) {

        return make_between(
            make_column_reference(std::move(column_name)),
            lower_bound,
            upper_bound,
            lower_inclusive,
            upper_inclusive
        );
    }

    PredicateExpressionPtr make_in(
        ScalarExpressionPtr value,
        std::vector<std::int64_t> candidates,
        bool negated) {

        require_scalar_child(value, "IN expression");

        if (candidates.empty()){
            throw std::invalid_argument(
                "IN expression requires at least one candidate"
            );
        }

        InExpression expression{
            std::move(value),
            std::move(candidates),
            negated
        };

        return std::make_unique<PredicateExpression>(
            PredicateNode{std::move(expression)}
        );
    }

    PredicateExpressionPtr make_in(
        std::string column_name,
        std::vector<std::int64_t> candidates,
        bool negated) {

        return make_in(
            make_column_reference(std::move(column_name)),
            std::move(candidates),
            negated
        );
    }

    PredicateExpressionPtr make_boolean(bool value) {

        BooleanLiteral literal{
            value
        };

        return std::make_unique<PredicateExpression>(
            PredicateNode{literal}
        );
    }

    BoundPredicateExpressionPtr bind_predicate_expression(
        const Schema& schema,
        const PredicateExpression& expression) {

        return bind_predicate_node(schema, expression);
    }

    SelectionMask evaluate_predicate_expression(
        const Table& table,
        const BoundPredicateExpression& expression) {

        SelectionMask mask(table.row_count(), 0);

        for (std::size_t i = 0; i < table.row_count(); i++){

            mask[i] = static_cast<std::uint8_t>(
                evaluate_predicate_at_row(table, expression, i)
            );
        }

        return mask;
    }

}