// Self-check of the overlay's JSON reader on the shape of Steam's GetGameAchievements answer.
#include "../src/overlay/mini_json.h"
#include <cassert>
#include <cstdio>

using refix::overlay::json::Parse;
using refix::overlay::json::Value;

int main() {
    const Value v = Parse(R"({"response":{"achievements":[
        {"internal_name":"ACH.WAKE_UP","localized_name":"Monstruo é 🏆","localized_desc":"Line\nTwo \"q\"",
         "hidden":true,"player_percent_unlocked":"64.7","max_progress_int":10,"neg":-1.5e2,"n":null},
        {"internal_name":"B","hidden":false}]}})");
    const Value& list = v["response"]["achievements"];
    assert(list.type == Value::Type::Array && list.items.size() == 2);
    const Value& a = list.items[0];
    assert(a.Str("internal_name") == "ACH.WAKE_UP");
    assert(a.Str("localized_name") == "Monstruo \xC3\xA9 \xF0\x9F\x8F\x86");
    assert(a.Str("localized_desc") == "Line\nTwo \"q\"");
    assert(a.Bool("hidden") && !list.items[1].Bool("hidden"));
    assert(a.Num("player_percent_unlocked") > 64.6 && a.Num("player_percent_unlocked") < 64.8);
    assert(a.Num("max_progress_int") == 10 && a.Num("neg") == -150 && a.Num("missing", 7) == 7);
    assert(a["n"].type == Value::Type::Null);

    // Empty answer of a game without achievements, and broken input.
    assert(Parse(R"({"response":{}})")["response"].type == Value::Type::Object);
    assert(Parse(R"({"response":{"a":[1,2})").type == Value::Type::Null);
    assert(Parse("").type == Value::Type::Null);
    assert(Parse(std::string(200, '[')).type == Value::Type::Null);   // depth limit
    std::puts("test_mini_json: OK");
    return 0;
}
