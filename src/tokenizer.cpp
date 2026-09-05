#include "fqe/tokenizer.hpp"

#include <algorithm>
#include <charconv>
#include <cctype>
#include <optional>
#include <stdexcept>
#include <system_error>
#include <unordered_map>
#include <utility>

namespace fqe {

    namespace {

        std::optional<TokenType> find_keyword(const std::string& word) {
            // Map keywords in Query to the equivalents used by tokenizer
            static const std::unordered_map<std::string, TokenType> keywords{
                {"SELECT", TokenType::Select},
                {"FROM", TokenType::From},
                {"WHERE", TokenType::Where},
                {"GROUP", TokenType::Group},
                {"BY", TokenType::By},
                {"HAVING", TokenType::Having},
                {"AS", TokenType::As},
                {"SUM", TokenType::Sum},
                {"COUNT", TokenType::Count},
                {"MIN", TokenType::Min},
                {"MAX", TokenType::Max},
                {"AND", TokenType::And},
                {"OR", TokenType::Or},
                {"NOT", TokenType::Not},
                {"BETWEEN", TokenType::Between},
                {"IN", TokenType::In},
                {"TRUE", TokenType::True},
                {"FALSE", TokenType::False}
            };

            auto iterator = keywords.find(word);

            if (iterator == keywords.end()) {
                return std::nullopt;
            }

            return iterator->second; //return corresponding token type 
        }

        //SQL keywords should be case-insensitive 
        // SELECT, select, Select -> TokenType::Select
        std::string uppercase_copy(std::string text) {

            std::transform(text.begin(), text.end(), text.begin(), [](unsigned char character) {
                return static_cast<char>(std::toupper(character));
            }); // SeLeCT -> SELECT

            return text;
        }

    }
    // input is only a constructor param -> stops existing when constructor returns 
    // This matters because this engine's design uses 2 function calls
    // fqe::Tokenizer tokenizer(some query)
    // std::vector<fqe::Token> tokens = toknenizer.tokenize()
    // this was implemented cuz without the class based approach for the tokenize,
    // the input and current state would have to be passed repeatedly into every 
    // function here
    // outside code can no longer corrupt the curson
    // the lifetime of the query is safely owned by the tokenizer
    // better error feedback cuz tokenize is explicitly called rather 
    // than implied in the first constructor command 
    Tokenizer::Tokenizer(std::string input) : input_(std::move(input)), current_(0) {}

    //if reached end of input return true
    bool Tokenizer::is_at_end() const noexcept {
        return current_ >= input_.size();
    }

    //return current index along input_ 
    char Tokenizer::peek() const noexcept {

        if (is_at_end()) { //no character left to peek at -> input already been processed 
            return '\0';
        }

        return input_[current_];
    }

    //same as above just for current_ + 1
    char Tokenizer::peek_next() const noexcept {

        if (current_ + 1 >= input_.size()) {
            return '\0';
        }

        return input_[current_ + 1];
    }

    //move along input 
    char Tokenizer::advance() noexcept {

        char character = input_[current_];

        current_++;

        return character;
    }

    // consume next char in cases like >= , <= 
    bool Tokenizer::match(char expected) noexcept {

        if (is_at_end() || input_[current_] != expected) {
            return false;
        }

        current_++;

        return true;
    }

    // moves current_ past characters that parser doesnt require
    // Space, -- (single line comments), /* */ (block comments )
    void Tokenizer::skip_ignored() {

        while (!is_at_end()) {

            unsigned char character = static_cast<unsigned char>(peek());
            // space so advance 
            if (std::isspace(character)) {
                advance();
                continue;
            }
            // -- comment so advance current until we reach end of input_ or unitl the comment
            // is done. i.e the line is terminated w \n
            if (peek() == '-' && peek_next() == '-') {

                while (!is_at_end() && peek() != '\n') {
                    advance();
                }

                continue;
            }
            // enter comment block /*
            if (peek() == '/' && peek_next() == '*') {

                std::size_t comment_position = current_;
                
                // advance over /* not necessary since closing comment order of */ is
                // different from open -> just easier to make sense of what actually
                // occurs in this block
                advance();
                advance();

                // in comment block ... and leave it when */ is found 
                // advance current_ while in comment block 
                while (!is_at_end() && !(peek() == '*' && peek_next() == '/')) {
                    advance();
                }

                //reached the end of input_ but comment never closed so query is invalid 
                if (is_at_end()) {
                    throw std::invalid_argument("Unterminated block comment at position " +
                        std::to_string(comment_position));
                }

                // */ needs to be advanced over -> necessary since not part of while loop
                // we could latch the while loop for 2 cycles but that would jus make the 
                // code more unreadable 
                advance();
                advance();

                continue;
            }

            break; //once non essential characters are skipped stop consuming characters
            // without processing of them as they would be essential to query 
        }
    }
    // ex for >= 
    // start_position - current_ = 2, therefore token returned is type = >= and 
    // the substring is input[start_position to start_position + 1] 
    Token Tokenizer::make_token(TokenType type, std::size_t start_position) const {

        return Token{type, input_.substr(start_position, current_ - start_position), std::nullopt,
            start_position}; //no numeric value cuz its not an integer token -> so we use
    } //like it can be 1000, > and the only time integer_value will have any sort of meaning is if 
    // its an integer value. a symbol like > has no value that is meaninful to the query itself

    Token Tokenizer::scan_word() {

        std::size_t start_position = current_;

        while (!is_at_end()) {

            unsigned char character = static_cast<unsigned char>(peek());

            //stop only when the character is not alphanumeric and not an underscore which 
            // is the only valid non alphanumberic character in a word -> header name 
            if (!std::isalnum(character) && peek() != '_') {
                break;
            }

            advance();
        }

        std::string text = input_.substr(start_position, current_ - start_position);//extract
        //word

        std::string uppercase_text = uppercase_copy(text); //to check if its an
        //sql keyword present in find_keyword map 

        std::optional<TokenType> keyword = find_keyword(uppercase_text); 

        if (keyword.has_value()) {//in find_keyword map so not a header of a column 
            return Token{keyword.value(), std::move(text), std::nullopt, start_position};
        }
        // is header of column so it has to be an identifier type
        return Token{TokenType::Identifier, std::move(text), std::nullopt, start_position};
    }

    Token Tokenizer::scan_integer() {

        std::size_t start_position = current_;

        while (!is_at_end() && std::isdigit(static_cast<unsigned char>(peek()))) {
            advance();
        }

        std::string text = input_.substr(start_position, current_ - start_position);

        std::int64_t value = 0;

        const char* start = text.data();

        const char* end = start + text.size();

        auto result = std::from_chars(start, end, value);

        //check if integer > 2**63 - 1 or < -2**63 
        if (result.ec == std::errc::result_out_of_range) {
            throw std::out_of_range("Integer is outside the Int64 range at position " +
                std::to_string(start_position));
        }
        //input being scanned has not been converted to an integer successfully as
        // from_chars did not receive a string with only integers 
        if (result.ec != std::errc{} || result.ptr != end) {
            throw std::invalid_argument("Invalid integer '" + text + "' at position " +
                std::to_string(start_position));
        }
        // Integer_value is now meaningful 
        // we won't access text again so we can just move the data from text
        // Type is obviously an integer 
        return Token{TokenType::Integer, std::move(text), value, start_position};
    }

    Token Tokenizer::scan_symbol() {

        std::size_t start_position = current_;

        char character = advance();

        switch (character) {

            case '=':
                return make_token(TokenType::Equal, start_position);

            case '!':

                if (match('=')) {
                    return make_token(TokenType::NotEqual, start_position);
                }
                // need next char to be = for !=, no other context for ! symbol 
                throw std::invalid_argument("Expected '=' after '!' at position " +
                    std::to_string(start_position));

            case '<':
                // <= 
                if (match('=')) {
                    return make_token(TokenType::LessEqual, start_position);
                }
                // <> -> standard symbol for != in sql -> != is actually a 
                // non-standard extension
                if (match('>')) {
                    return make_token(TokenType::NotEqual, start_position);
                }
                // < 
                return make_token(TokenType::LessThan, start_position);

            case '>':
                // >= 
                if (match('=')) {
                    return make_token(TokenType::GreaterEqual, start_position);
                }
                // > 
                return make_token(TokenType::GreaterThan, start_position);

                // no such thing as ++, **, // etc
                // -- and /* */ do exist but already checked for in skip_ignored
            case '+':
                return make_token(TokenType::Plus, start_position);

            case '-':
                return make_token(TokenType::Minus, start_position);

            case '*':
                return make_token(TokenType::Star, start_position);

            case '/':
                return make_token(TokenType::Slash, start_position);

            case '(':
                return make_token(TokenType::LeftParenthesis, start_position);

            case ')':
                return make_token(TokenType::RightParenthesis, start_position);

            case ',':
                return make_token(TokenType::Comma, start_position);

            case ';':
                return make_token(TokenType::Semicolon, start_position);
        }
        // all other symbol types and combinations are invalid 
        throw std::invalid_argument("Unexpected character '" + std::string(1, character) +
            "' at position " + std::to_string(start_position));
    }
    // 
    std::vector<Token> Tokenizer::tokenize() {

        current_ = 0;

        std::vector<Token> tokens;

        while (true) {//until end of input_ 

            skip_ignored();//check for comment/space 

            if (is_at_end()) {//end of input reached break
                break;
            }

            char character = peek(); // charater access along input_ 

            if (std::isalpha(static_cast<unsigned char>(character)) || character == '_') {
                tokens.push_back(scan_word());
                continue;
            } //word/keyword 

            if (std::isdigit(static_cast<unsigned char>(character))) {
                tokens.push_back(scan_integer());
                continue;
            } // integer

            tokens.push_back(scan_symbol()); //symbol
        }

        tokens.push_back(Token{TokenType::End, "", std::nullopt, current_});//end of token list
        // sigifies end of query

        return tokens;//list of tokens in order 
    }
    // allows for token type values to be readable during testing, debugging etc. 
    std::string_view to_string(TokenType type) noexcept {

        switch (type) {

            case TokenType::Select:
                return "Select";

            case TokenType::From:
                return "From";

            case TokenType::Where:
                return "Where";

            case TokenType::Group:
                return "Group";

            case TokenType::By:
                return "By";

            case TokenType::Having:
                return "Having";

            case TokenType::As:
                return "As";

            case TokenType::Sum:
                return "Sum";

            case TokenType::Count:
                return "Count";

            case TokenType::Min:
                return "Min";

            case TokenType::Max:
                return "Max";

            case TokenType::And:
                return "And";

            case TokenType::Or:
                return "Or";

            case TokenType::Not:
                return "Not";

            case TokenType::Between:
                return "Between";

            case TokenType::In:
                return "In";

            case TokenType::True:
                return "True";

            case TokenType::False:
                return "False";

            case TokenType::Identifier:
                return "Identifier";

            case TokenType::Integer:
                return "Integer";

            case TokenType::Equal:
                return "Equal";

            case TokenType::NotEqual:
                return "NotEqual";

            case TokenType::LessThan:
                return "LessThan";

            case TokenType::LessEqual:
                return "LessEqual";

            case TokenType::GreaterThan:
                return "GreaterThan";

            case TokenType::GreaterEqual:
                return "GreaterEqual";

            case TokenType::Plus:
                return "Plus";

            case TokenType::Minus:
                return "Minus";

            case TokenType::Star:
                return "Star";

            case TokenType::Slash:
                return "Slash";

            case TokenType::LeftParenthesis:
                return "LeftParenthesis";

            case TokenType::RightParenthesis:
                return "RightParenthesis";

            case TokenType::Comma:
                return "Comma";

            case TokenType::Semicolon:
                return "Semicolon";

            case TokenType::End:
                return "End";
        }

        return "Unknown";
    }

}