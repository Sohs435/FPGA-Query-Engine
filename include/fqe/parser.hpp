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

            std::vector<SelectItem> parse_select_list(); //parses one or more comma-seperated select items
            // e.g SELECT sum(price), COUNT(*), instrument 
            // call parse_select_item repeatedly
            // SUM(price)
            // COUNT(*)
            // instrument
            // until full SELECT list is accounted for 

            SelectItem parse_select_item(); //parses one item from SELECT list

            std::vector<std::string> parse_group_by_list(); //parses comma seperated column names
            // after GROUP BY
            // Ex. Group BY instrument, timestamp

            bool is_aggregate_function(TokenType type) const noexcept; 

            AggregateFunction parse_aggregate_function();

            PredicateExpressionPtr parse_predicate(); //construct everything after WHERE using
            // the functions below -> the predicate statement 

            PredicateExpressionPtr parse_or_expression(); //construct OR nodes 

            PredicateExpressionPtr parse_and_expression(); // construct AND nodes 

            PredicateExpressionPtr parse_not_expression(); // construct NOT nodes

            PredicateExpressionPtr parse_predicate_primary(); //predicate parantheses

            PredicateExpressionPtr parse_comparison_expression(); // BETWEEN + IN nodes

            ScalarExpressionPtr parse_scalar_expression(); // produce integer value for each row
            // itself not true or false 
            // returns parse_additive_expression()

            ScalarExpressionPtr parse_additive_expression(); // + and - nodes

            ScalarExpressionPtr parse_multiplicative_expression(); // * and / nodes

            ScalarExpressionPtr parse_unary_expression(); // -ve expressions

            ScalarExpressionPtr parse_scalar_primary(); //column names, integers, arithmetic 

            ComparisonOperator parse_comparison_operator(); //consume comparison totken 
            //and covert it into ComparisonOperator
            // TokenType::Equal -> ComparisonOperator::Equal for example 
            // only there to convert token into operation stored in the predicate tree 

            std::int64_t parse_integer_value();

            std::vector<std::int64_t> parse_integer_list(); // Parses the values inside IN
            // instrument IN(1,3,5) -> std::vector<std:int64_t> {1,3,5}

            bool parenthesis_starts_predicate() const; // distingush logical and comparison
            // expressions such as (price > 1000 OR quantity < 500) from an arithmetic
            // expression such as (price * quantity) > 100000


            std::vector<Token> tokens_; //sequence created by tokenizer

            std::size_t current_; //index of next token that needs to be processed
            // i.e just the index of tokens_ about to processed 


    };

}