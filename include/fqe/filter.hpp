#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <variant>
#include <vector>

#include "data_type.hpp"
#include "predicate.hpp"
#include "schema.hpp"
#include "table.hpp"

// Column to literal + column to column comparisons
// Arithmetic expressions
// Nested AND, OR, NOT
// Inclusive/Exclusive ranges
// Const T/F
// Binding column names to indices
// Recursive execution w short circuiting 
namespace fqe {

    // Arithmetic expressions
    enum class ArithmeticOperator {
        Add, Subtract, Multiply, Divide
    };

    struct ScalarExpression;

    using ScalarExpressionPtr = std::unique_ptr<ScalarExpression>;

    struct ColumnReference {
        std::string column_name;
    };

    struct IntegerLiteral {
        std::int64_t value; 
    };

    struct ArithmeticExpression {
        ArithmeticOperator operation;
        ScalarExpressionPtr left;
        ScalarExpressionPtr right; 
    };

    using ScalarNode = std::variant<ColumnReference, IntegerLiteral, ArithmeticExpression>;

    struct ScalarExpression{
        explicit ScalarExpression (ScalarNode node);

        ScalarNode node; 
    };

    // AND/OR/NOT/BETWEEN expressions 
    struct PredicateExpression; 

    using PredicateExpressionPtr = std::unique_ptr<PredicateExpression>;

    struct ComparisonExpression {
        ScalarExpressionPtr left;
        ComparisonOperator comparison; 
        ScalarExpressionPtr right; 
    };

    struct AndExpression {
        PredicateExpressionPtr left; 
        PredicateExpressionPtr right; 
    };

    struct OrExpression {
        PredicateExpressionPtr left; 
        PredicateExpressionPtr right; 
    };

    struct NotExpression {
        PredicateExpressionPtr child; 
    };

    struct BetweenExpression {
        ScalarExpressionPtr value; 
        std::int64_t lower_bound; 
        std::int64_t upper_bound; 

        bool lower_inclusive;
        bool upper_inclusive; 
    };

    struct InExpression {
        ScalarExpressionPtr value; 
        std::vector<std::int64_t> candidates;
        bool negated; 
    };

    struct BooleanLiteral {
        bool value; 
    };

    // can be and, or, not, comparison, inclusive, boolean literal T/F
    using PredicateNode = std::variant<ComparisonExpression, AndExpression, OrExpression, 
     NotExpression, BetweenExpression, InExpression, BooleanLiteral>; 

     struct PredicateExpression {

        explicit PredicateExpression (PredicateNode node);

        PredicateNode node; 
     };

     struct BoundScalarExpression; 

    using BoundScalarExpressionPtr = std::unique_ptr<BoundScalarExpression>; 

    struct BoundColumnReference {
        std::size_t column_index; 
        DataType column_type; 
    };

    struct BoundIntegerLiteral {
        std::int64_t value; 
    };
    struct BoundArithmeticExpression {
        ArithmeticOperator operation;
        BoundScalarExpressionPtr left; 
        BoundScalarExpressionPtr right;
    };
    
    using BoundScalarNode = std::variant<BoundColumnReference, BoundIntegerLiteral, 
     BoundArithmeticExpression>;

    struct BoundScalarExpression {
        BoundScalarExpression(BoundScalarNode node, DataType result_type);

        BoundScalarNode node;
        DataType result_type;
    };

    struct BoundPredicateExpression;

    using BoundPredicateExpressionPtr = std::unique_ptr<BoundPredicateExpression>;

    struct BoundComparisonExpression {
        BoundScalarExpressionPtr left;
        ComparisonOperator comparison;
        BoundScalarExpressionPtr right;
    };

    struct BoundAndExpression {
        BoundPredicateExpressionPtr left;
        BoundPredicateExpressionPtr right;
    };

    struct BoundOrExpression {
        BoundPredicateExpressionPtr left;
        BoundPredicateExpressionPtr right;
    };

    struct BoundNotExpression {
        BoundPredicateExpressionPtr child;
    };

    struct BoundBetweenExpression {
        BoundScalarExpressionPtr value;
        std::int64_t lower_bound;
        std::int64_t upper_bound;
        bool lower_inclusive;
        bool upper_inclusive;
    };

    struct BoundInExpression {
        BoundScalarExpressionPtr value;
        std::vector<std::int64_t> candidates;
        bool negated;
    };

    using BoundPredicateNode = std::variant<BoundComparisonExpression, BoundAndExpression,
     BoundOrExpression, BoundNotExpression, BoundBetweenExpression, BoundInExpression, 
      BooleanLiteral>;

    struct BoundPredicateExpression {

        explicit BoundPredicateExpression(BoundPredicateNode node);

        BoundPredicateNode node;
    };

    ScalarExpressionPtr make_column_reference(std::string column_name);

    ScalarExpressionPtr make_integer_literal(std::int64_t value);

    ScalarExpressionPtr make_arithmetic(ArithmeticOperator operation, ScalarExpressionPtr left,
     ScalarExpressionPtr right);

    PredicateExpressionPtr make_comparison(ScalarExpressionPtr left, 
     ComparisonOperator comparison, ScalarExpressionPtr right);

    PredicateExpressionPtr make_comparison(ComparisonPredicate predicate);

    PredicateExpressionPtr make_and(PredicateExpressionPtr left, PredicateExpressionPtr right);

    PredicateExpressionPtr make_or(PredicateExpressionPtr left, PredicateExpressionPtr right);

    PredicateExpressionPtr make_not(PredicateExpressionPtr child);

    PredicateExpressionPtr make_between(ScalarExpressionPtr value, std::int64_t lower_bound,
     std::int64_t upper_bound, bool lower_inclusive = true, bool upper_inclusive = true);

    PredicateExpressionPtr make_between(std::string column_name, std::int64_t lower_bound,
     std::int64_t upper_bound, bool lower_inclusive = true, bool upper_inclusive = true);

    PredicateExpressionPtr make_in(ScalarExpressionPtr value, 
     std::vector<std::int64_t> candidates, bool negated = false);

    PredicateExpressionPtr make_in(std::string column_name, 
     std::vector<std::int64_t> candidates, bool negated = false);

    PredicateExpressionPtr make_boolean(bool value);

    BoundPredicateExpressionPtr bind_predicate_expression( const Schema& schema,
     const PredicateExpression& expression);

    SelectionMask evaluate_predicate_expression(const Table& table,
     const BoundPredicateExpression& expression);


}