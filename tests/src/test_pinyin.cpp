//
// 测试拼音输入法的核心逻辑，包括双拼和全拼方案，以及动态切换输入方案的功能。
//
#include <Windows.h>
#include <chrono>
#include <stdexcept>
#include <string>
#include <fmt/core.h>
#include "fmt/base.h"
#include "core/ime_session.h"
#include "quanpin/quanpin_dictionary.h"
#include "shuangpin/shuangpin_dictionary.h"

using namespace std;

namespace
{

void expect(bool condition, const std::string &message)
{
    if (!condition)
    {
        throw std::runtime_error(message);
    }
}

void expect_session_state(const ImeSession &session, const std::string &expected_preedit)
{
    expect(session.get_preedit() == expected_preedit,
           fmt::format("Expected preedit '{}', got '{}'", expected_preedit, session.get_preedit()));
    expect(session.get_request().raw_input == expected_preedit,
           fmt::format("Expected raw_input '{}', got '{}'", expected_preedit, session.get_request().raw_input));
}

} // namespace

void print_candidates(const std::vector<WordItem> &result)
{
    for (const auto &[code, word, weight] : result)
    {
        fmt::println("Candidate: {} [{}] ({})", word, code, weight);
    }
}

void run_quanpin_query_case(QuanpinDictionary &dictionary, const std::string &query)
{
    const auto start = std::chrono::high_resolution_clock::now();
    const auto result = dictionary.query(query);
    const auto end = std::chrono::high_resolution_clock::now();
    const auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);

    fmt::println("Query: {}", query);
    fmt::println("Time: {} us", duration.count());
    print_candidates(result);
}

void feed_sequence(ImeSession &session, const vector<UINT> &sequence, const vector<WCHAR> &wch_sequence = {})
{
    for (int i = 0; i < sequence.size(); ++i)
    {
        std::chrono::high_resolution_clock::time_point start = std::chrono::high_resolution_clock::now();
        session.handle_key(sequence[i], 0, i < wch_sequence.size() ? wch_sequence[i] : 0);
        std::chrono::high_resolution_clock::time_point end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);

        fmt::println("Preedit: {}", session.get_preedit());
        fmt::println("Time: {} us", duration.count());
    }
}

void test_shuangpin_session()
{
    ImeSession session(SchemeType::Shuangpin);
    const vector<UINT> sequence{'C', 'L', 'S'};      // 按键的 vk 码
    const vector<WCHAR> wch_sequence{'c', 'l', 's'}; // 实际的字符，区分大小写输入

    fmt::println("==== Shuangpin ====");
    feed_sequence(session, sequence, wch_sequence);
    print_candidates(session.get_candidates());
}

void test_shuangpin_session02()
{
    // ImeSession session(SchemeType::Quanpin);
    ImeSession session(SchemeType::Shuangpin);
    // const vector<UINT> sequence{'C', 'E', 'L', 'I', 'S', 'H', 'I'};
    const vector<UINT> sequence{'C', 'E', 'L', 'I', 'U', 'I'};
    const vector<WCHAR> wch_sequence{'c', 'e', 'l', 'i', 'u', 'i'};

    fmt::println("==== Shuangpin ====");
    feed_sequence(session, sequence, wch_sequence);
    print_candidates(session.get_candidates());
}

void test_quanpin_session()
{
    ImeSession session(SchemeType::Quanpin);
    const vector<UINT> sequence{'C', 'E', 'S', 'H', 'I'};
    const vector<WCHAR> wch_sequence{'c', 'e', 's', 'h', 'i'};

    fmt::println("==== Quanpin ====");
    feed_sequence(session, sequence, wch_sequence);
    print_candidates(session.get_candidates());
}

void test_dynamic_switch()
{
    ImeSession session(SchemeType::Shuangpin);

    fmt::println("==== Switch Scheme ====");
    feed_sequence(session, {'N', 'I'}, {'n', 'i'});
    fmt::println("Before switch preedit: {}", session.get_preedit());

    session.switch_scheme(SchemeType::Quanpin);
    fmt::println("After switch preedit: {}", session.get_preedit());

    feed_sequence(session, {'N', 'I', 'H', 'A', 'O'}, {'n', 'i', 'h', 'a', 'o'});
    print_candidates(session.get_candidates());
}

void test_quanpin_session_backspace()
{
    ImeSession session(SchemeType::Quanpin);

    fmt::println("==== Quanpin Backspace ====");
    feed_sequence(session, {'C', 'E', 'S', 'H', 'I'}, {'c', 'e', 's', 'h', 'i'});
    expect_session_state(session, "ceshi");
    expect(!session.get_candidates().empty(), "Quanpin session should have candidates before backspace.");

    session.handle_key(VK_BACK);
    fmt::println("Preedit after backspace: {}", session.get_preedit());
    expect_session_state(session, "cesh");
    expect(session.get_request().valid, "Quanpin session request should stay valid after backspace.");
}

void test_shuangpin_session_backspace()
{
    ImeSession session(SchemeType::Shuangpin);

    fmt::println("==== Shuangpin Backspace ====");
    feed_sequence(session, {'C', 'E', 'L', 'I', 'U', 'I'}, {'c', 'e', 'l', 'i', 'u', 'i'});
    expect_session_state(session, "celiui");
    expect(!session.get_candidates().empty(), "Shuangpin session should have candidates before backspace.");

    session.handle_key(VK_BACK);
    fmt::println("Preedit after backspace: {}", session.get_preedit());
    expect_session_state(session, "celiu");
    expect(session.get_request().valid, "Shuangpin session request should stay valid after backspace.");
}

void test_quanpin_dictionary_backspace()
{
    QuanpinDictionary dictionary;

    fmt::println("==== Quanpin Dictionary Backspace ====");
    dictionary.handleVkCode('C', 0, 'c');
    dictionary.handleVkCode('E', 0, 'e');
    dictionary.handleVkCode('S', 0, 's');
    expect(dictionary.get_pinyin_sequence() == "ces",
           fmt::format("Expected quanpin dictionary sequence 'ces', got '{}'", dictionary.get_pinyin_sequence()));

    dictionary.handleVkCode(VK_BACK, 0);
    expect(dictionary.get_pinyin_sequence() == "ce",
           fmt::format("Expected quanpin dictionary sequence 'ce' after backspace, got '{}'",
                       dictionary.get_pinyin_sequence()));
    expect(!dictionary.get_current_candidate_list().empty(),
           "Quanpin dictionary should still have candidates after backspace.");
}

void test_shuangpin_dictionary_backspace()
{
    ShuangpinDictionary dictionary;

    fmt::println("==== Shuangpin Dictionary Backspace ====");
    dictionary.handleVkCode('C', 0, 'c');
    dictionary.handleVkCode('E', 0, 'e');
    dictionary.handleVkCode('L', 0, 'l');
    expect(dictionary.get_pinyin_sequence() == "cel",
           fmt::format("Expected shuangpin dictionary sequence 'cel', got '{}'", dictionary.get_pinyin_sequence()));

    dictionary.handleVkCode(VK_BACK, 0);
    expect(dictionary.get_pinyin_sequence() == "ce",
           fmt::format("Expected shuangpin dictionary sequence 'ce' after backspace, got '{}'",
                       dictionary.get_pinyin_sequence()));
    expect(!dictionary.get_current_candidate_list().empty(),
           "Shuangpin dictionary should still have candidates after backspace.");
}

void test_quanpin_query_timings()
{
    QuanpinDictionary dictionary;

    fmt::println("==== Quanpin Query Timings ====");
    run_quanpin_query_case(dictionary, "nih");
    run_quanpin_query_case(dictionary, "niha");
    run_quanpin_query_case(dictionary, "nihao");
    run_quanpin_query_case(dictionary, "ni");
    run_quanpin_query_case(dictionary, "n");
    run_quanpin_query_case(dictionary, "shen");
    run_quanpin_query_case(dictionary, "shenme");
    run_quanpin_query_case(dictionary, "shenmeshi");
    run_quanpin_query_case(dictionary, "shenmeshi");
    run_quanpin_query_case(dictionary, "shenmeshui");
    run_quanpin_query_case(dictionary, "shenmesh");
    run_quanpin_query_case(dictionary, "shenmes");
}

int main(int argc, char *argv[])
{
    test_shuangpin_session();
    test_shuangpin_session02();
    test_quanpin_session();
    test_dynamic_switch();
    test_quanpin_session_backspace();
    test_shuangpin_session_backspace();
    test_quanpin_dictionary_backspace();
    test_shuangpin_dictionary_backspace();
    test_quanpin_query_timings();
    fmt::println("All tests passed.");
    return 0;
}
