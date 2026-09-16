#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace fqe {

    enum class TokenType {
        Select,
        From,
        Where,
        Group,
        By,
        Having,
        As,

        Sum,
        Count,
        Min,
        Max,

        And,
        Or,
        Not,
        Between,
        In,
        True,
        False,

        Identifier,
        Integer,

        Equal,
        NotEqual,
        LessThan,
        LessEqual,
        GreaterThan,
        GreaterEqual,

        Plus,
        Minus,
        Star,
        Slash,

        LeftParenthesis,
        RightParenthesis,
        Comma,
        Semicolon,

        End
    };

    struct Token {
        TokenType type;
        std::string text;
        std::optional<std::int64_t> integer_value;
        std::size_t position;
    };

    class Tokenizer {

        public:
            explicit Tokenizer(std::string input);

            std::vector<Token> tokenize();

        private:
            bool is_at_end() const noexcept;

            char peek() const noexcept; //inspect current char without consumption

            char peek_next() const noexcept;

            char advance() noexcept; //consume a char

            bool match(char expected) noexcept; // consumes second char if >= or <= since
            // they become one token 

            void skip_ignored(); //spaces, newlines

            Token scan_word(); // handles keywords + identifiers

            Token scan_integer(); // literals + overflow checking

            Token scan_symbol(); // operators (AND, OR) + punctuation 
        

            Token make_token(TokenType type, std::size_t start_position) const; // build 
            // token given the actual type from TokenType along with the position 
            // along input_ where the token lies 

            std::string input_; //privately access input
            // input is only a constructor parameter and stops existing when constructor returns
            // notice that 2 seperate calls occur from public access of tokenizer
            std::size_t current_;
    };

    std::string_view to_string(TokenType type) noexcept;

}