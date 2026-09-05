#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "filter.hpp"
#include "tokenizer.hpp"

namespace fqe {
    
    enum class AggregateFunction {
        Sum,
        Count, 
        Min, 
        Max
    };

    struct SelectItem {

        //empty for expressions such as SELECT price or SELECT quantity -> ordinary projected expressions
        std::optional<AggregateFunction> aggregate; 

        // expression inside SUM(), MAX(), MIN() and nullptr for COUNT(*)
        ScalarExpressionPtr expression;

        bool is_star = false; // COUNT(*) item, for example will set this true

        // Empty unless Query contains AS
        std::optional<std::string> alias; 
    };

    struct ParsedQuery {

        std::vector<SelectItem> select_items;

        std::string table_name;

        PredicateExpressionPtr where_expression;

        std::vector<std::string> group_by_columns;
    };

    class Parser {

        public: 
            explicit Parser (std::vector<Token> tokens); //constructor 

            ParsedQuery parse_query();//public entry point to parse query statement 

        private:
            bool is_at_end() const noexcept;

            const Token& peek() const; 

            const Token& previous() const;

            const Token& advance();

            bool check(TokenType type) const noexcept; // check if current token has specific type
            // without consumption 

            bool match(TokenType type);

            const Token& consume(TokenType type, const std::string& message); 

            std::vector<SelectItem> parse_select_list();

            bool is_aggregate_function(TokenType type) const noexcept; 

            AggregateFunction parse_aggregate_function();

            PredicateExpressionPtr parse_predicate();

            PredicateExpressionPtr parse_or_expression(); //construct OR nodes 

            PredicateExpressionPtr parse_and_expression(); // construct AND nodes 

            PredicateExpressionPtr parse_not_expression(); // construct NOT nodes

            PredicateExpressionPtr parse_predicate_primary(); //predicate parantheses

            PredicateExpressionPtr parse_comparison_expression(); // BETWEEN + IN nodes

            ScalarExpressionPtr parse_scalar_expression();

            ScalarExpressionPtr parse_additive_expression(); // + and - nodes

            ScalarExpressionPtr parse_multiplicative_expression(); // * and / nodes

            ScalarExpressionPtr parse_unary_expression(); // -ve expressions

            ScalarExpressionPtr parse_scalar_primary(); //column names, integers, arithmetic 

            ComparisonOperator parse_comparison_operator();

            std::vector<std::int64_t> parse_integer_list();

            bool parenthesis_starts_predicate() const; // distingush logical and comparison
            // expressions such as (price > 1000 OR quantity < 500) from an arithmetic
            // expression such as (price * quantity) > 100000


            std::vector<Token> tokens_;

            std::size_t current_;


    };

}