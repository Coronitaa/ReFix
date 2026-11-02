#pragma once
#include <string>

// Minimal JSON / GraphQL text helpers for the Party Animals local backend. The game builds queries with
// BaseDataClass.BuildMutate/BuildFullQuery: string args are JSON-encoded, numbers/bools are raw tokens.
namespace ReFixPAGql {
    // Parses a JSON/GraphQL string literal starting at text[pos] == '"'.
    bool ReadQuoted(const std::string& text, size_t pos, std::string& out, size_t* end = nullptr);

    std::string JsonEscape(const std::string& s);

    // `"value"` with JSON escaping.
    std::string JsonString(const std::string& s);

    // Value of a string member of a JSON object (first occurrence of "name").
    std::string JsonStringMember(const std::string& json, const char* name);

    // Raw scalar token of a JSON member ("name": 12 -> "12", true -> "true"); empty when missing.
    std::string JsonScalarMember(const std::string& json, const char* name);

    // First field of a GraphQL document: "mutation { createRoom(...) {...} }" -> "createRoom".
    std::string Operation(const std::string& query);

    // `name: "value"` argument (string); empty when missing.
    std::string StringArg(const std::string& query, const char* name);

    // `name: 12` argument as a raw token (number / bool / enum); empty when missing.
    std::string ScalarArg(const std::string& query, const char* name);

    // Successful LambdaResult envelope: {"code":0,...,"result":{"<operation>":<dataJson>}}.
    std::string Success(const std::string& operation, const std::string& dataJson);
}
