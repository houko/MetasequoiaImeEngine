#include "../../core/input_session.h"
#include "../../core/data_path.h"
#include "../../user_dictionary/user_dictionary_journal.h"

#include <sqlite3.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>

namespace
{
class Database
{
  public:
    explicit Database(const std::filesystem::path &path)
    {
        if (sqlite3_open(metasequoia::path_to_utf8(path).c_str(), &database_) != SQLITE_OK)
        {
            throw std::runtime_error("Failed to create the input-session test dictionary.");
        }
    }

    ~Database()
    {
        sqlite3_close(database_);
    }

    void execute(const char *sql)
    {
        char *error = nullptr;
        if (sqlite3_exec(database_, sql, nullptr, nullptr, &error) != SQLITE_OK)
        {
            const std::string message = error == nullptr ? "SQLite operation failed." : error;
            sqlite3_free(error);
            throw std::runtime_error(message);
        }
    }

    std::int64_t query_integer(const char *sql)
    {
        sqlite3_stmt *statement = nullptr;
        if (sqlite3_prepare_v2(database_, sql, -1, &statement, nullptr) != SQLITE_OK ||
            sqlite3_step(statement) != SQLITE_ROW)
        {
            sqlite3_finalize(statement);
            throw std::runtime_error("Failed to query the input-session test dictionary.");
        }
        const std::int64_t value = sqlite3_column_int64(statement, 0);
        sqlite3_finalize(statement);
        return value;
    }

  private:
    sqlite3 *database_ = nullptr;
};

void type(metasequoia::InputSession &session, const std::string &text)
{
    for (const char character : text)
    {
        if (!session.handle_character(character).handled)
        {
            throw std::runtime_error("A pinyin character was not handled.");
        }
    }
}

void require(bool condition, const char *message)
{
    if (!condition)
    {
        throw std::runtime_error(message);
    }
}

void write_file(const std::filesystem::path &path, const std::string &contents)
{
    std::filesystem::create_directories(path.parent_path());
    std::ofstream stream(path);
    stream << contents;
    if (!stream)
    {
        throw std::runtime_error("Failed to prepare an input-session helpcode fixture.");
    }
}

void set_data_directory(const std::filesystem::path &directory)
{
#ifdef _WIN32
    if (_wputenv_s(L"METASEQUOIA_IME_DATA_DIR", directory.c_str()) != 0)
#else
    if (setenv("METASEQUOIA_IME_DATA_DIR", metasequoia::path_to_utf8(directory).c_str(), 1) != 0)
#endif
    {
        throw std::runtime_error("Failed to set the data directory override.");
    }
}

void prepare_frequency_fixture(const std::filesystem::path &directory)
{
    std::filesystem::create_directories(directory);
    Database database(directory / "msime.db");
    database.execute("CREATE TABLE tbl_1_n(key TEXT, jp TEXT, value TEXT, weight INTEGER)");
    database.execute("INSERT INTO tbl_1_n VALUES('ni', 'n', '甲', 100)");
    database.execute("INSERT INTO tbl_1_n VALUES('ni', 'n', '乙', 90)");
    database.execute("INSERT INTO tbl_1_n VALUES('ni', 'n', '丙', 80)");
    database.execute("INSERT INTO tbl_1_n VALUES('ni', 'n', '丁', 70)");
    database.execute("INSERT INTO tbl_1_n VALUES('ni', 'n', '戊', 60)");
    database.execute("INSERT INTO tbl_1_n VALUES('ni', 'n', '己', 50)");
}

void prepare_shuangpin_frequency_fixture(const std::filesystem::path &directory)
{
    std::filesystem::create_directories(directory);
    Database database(directory / "msime.db");
    database.execute("CREATE TABLE tbl_2_n(key TEXT, jp TEXT, value TEXT, weight INTEGER)");
    database.execute("INSERT INTO tbl_2_n VALUES('ni''hao', 'nh', '你好', 100)");
    database.execute("INSERT INTO tbl_2_n VALUES('ni''hao', 'nh', '拟好', 50)");
}

void prepare_wubi_frequency_fixture(const std::filesystem::path &directory)
{
    std::filesystem::create_directories(directory);
    Database database(directory / "msime.db");
    database.execute("CREATE TABLE wubi86(key TEXT, value TEXT, weight INTEGER)");
    database.execute("INSERT INTO wubi86 VALUES('aaaa', '工', 100)");
    database.execute("INSERT INTO wubi86 VALUES('aaaa', '或', 50)");
}

std::size_t candidate_index(const metasequoia::InputSession &session, const std::string &word)
{
    const auto found = std::find_if(session.candidates().begin(), session.candidates().end(),
                                    [&](const WordItem &item) { return item.word == word; });
    if (found == session.candidates().end())
    {
        std::string message = "The expected edge-selection candidate was not produced: " + word + "; actual:";
        for (const auto &candidate : session.candidates())
        {
            message += " [" + candidate.word + "]";
        }
        throw std::runtime_error(message);
    }
    return static_cast<std::size_t>(std::distance(session.candidates().begin(), found));
}

bool same_candidate_words(const metasequoia::InputSession &left, const metasequoia::InputSession &right)
{
    if (left.candidates().size() != right.candidates().size())
    {
        return false;
    }
    return std::equal(left.candidates().begin(), left.candidates().end(), right.candidates().begin(),
                      [](const auto &left_item, const auto &right_item) {
                          return left_item.word == right_item.word;
                      });
}
} // namespace

int main()
{
    const auto unique_suffix = std::to_string(std::chrono::high_resolution_clock::now().time_since_epoch().count());
    const std::filesystem::path data_directory =
        std::filesystem::temp_directory_path() / std::filesystem::u8path("metasequoia-session-词库-" + unique_suffix);
    std::filesystem::create_directories(data_directory);
    set_data_directory(data_directory);

    {
        const std::filesystem::path helpcode_directory = data_directory / "helpcodes";
        write_file(helpcode_directory / "helpcode.txt", "你=ab\n拟=cd\n好=ef\n");
        write_file(helpcode_directory / "zrm_helpcode_big_unique.txt", "你=cb\n拟=ad\n好=ef\n");
        write_file(helpcode_directory / "shouyou2_0_helpcode.txt", "你=ab\n拟=cd\n好=ef\n");
        write_file(helpcode_directory / "shouyouplus_helpcode.txt", "你=ab\n拟=cd\n好=ef\n");
        write_file(helpcode_directory / "xiaohe_helpcode.txt", "你=ab\n拟=cd\n好=ef\n");

        Database database(data_directory / "msime.db");
        database.execute("CREATE TABLE tbl_2_n(key TEXT, jp TEXT, value TEXT, weight INTEGER)");
        database.execute("INSERT INTO tbl_2_n VALUES('ni''hao', 'nh', '你好', 200)");
        database.execute("INSERT INTO tbl_2_n VALUES('ni''hao', 'nh', '拟好', 100)");
        database.execute("INSERT INTO tbl_2_n VALUES('ni''hao', 'nh', '𠀀方案𠮷', 90)");
        database.execute("INSERT INTO tbl_2_n VALUES('ni''hao', 'nh', 'C语言 2', 80)");
        database.execute("INSERT INTO tbl_2_n VALUES('ni''hao', 'nh', 'GitHub', 70)");

        metasequoia::InputSession session(SchemeType::Quanpin);
        type(session, "nihao");
        require(session.preedit() == "nihao", "The preedit did not mirror the raw pinyin.");
        require(session.raw_segmentation() == "ni'hao" && session.normalized_segmentation() == "ni'hao",
                "Quanpin segmentation was not exposed through the native session API.");
        require(session.has_composition(), "Typing pinyin did not start a composition.");
        require(session.candidates().size() >= 2, "The engine did not return both dictionary candidates.");

        const auto selected = session.select_candidate(static_cast<std::size_t>(1));
        require(selected.handled && selected.commit == "拟好", "Selecting the second candidate committed wrong text.");
        require(!session.has_composition(), "Selecting a candidate did not end the composition.");

        type(session, "nihao");
        const auto by_word = session.select_candidate(std::string("拟好"));
        require(by_word.handled && by_word.commit == "拟好", "Selecting a candidate by word committed the wrong text.");

        type(session, "nihao");
        const auto out_of_range = session.select_candidate(session.candidates().size());
        require(!out_of_range.handled, "An out-of-range candidate index was accepted.");
        const auto unknown_word = session.select_candidate(std::string("没有这个词"));
        require(!unknown_word.handled, "An unknown candidate word was accepted.");

        const auto leading = session.handle_command(metasequoia::Command::CommitCandidate);
        require(leading.handled && leading.commit == "你好", "CommitCandidate did not commit the leading candidate.");

        type(session, "nihao");
        const auto first_bmp = session.select_candidate_edge(candidate_index(session, "拟好"),
                                                              metasequoia::CandidateEdge::FirstHan);
        require(first_bmp.handled && first_bmp.commit == "拟" && !session.has_composition(),
                "FirstHan did not commit the first BMP Han character and reset the composition.");

        type(session, "nihao");
        const auto last_bmp = session.select_candidate_edge(candidate_index(session, "拟好"),
                                                             metasequoia::CandidateEdge::LastHan);
        require(last_bmp.handled && last_bmp.commit == "好" && !session.has_composition(),
                "LastHan did not commit the last BMP Han character and reset the composition.");

        type(session, "nihao");
        const auto first_supplementary = session.select_candidate_edge(
            candidate_index(session, "𠀀方案𠮷"), metasequoia::CandidateEdge::FirstHan);
        require(first_supplementary.handled && first_supplementary.commit == "𠀀" && !session.has_composition(),
                "FirstHan split a supplementary-plane Han character.");

        type(session, "nihao");
        const auto last_supplementary = session.select_candidate_edge(
            candidate_index(session, "𠀀方案𠮷"), metasequoia::CandidateEdge::LastHan);
        require(last_supplementary.handled && last_supplementary.commit == "𠮷" && !session.has_composition(),
                "LastHan split a supplementary-plane Han character.");

        type(session, "nihao");
        const auto first_mixed = session.select_candidate_edge(candidate_index(session, "C语言 2"),
                                                                metasequoia::CandidateEdge::FirstHan);
        require(first_mixed.handled && first_mixed.commit == "语",
                "FirstHan did not skip a non-Han candidate prefix.");

        type(session, "nihao");
        const auto last_mixed = session.select_candidate_edge(candidate_index(session, "C语言 2"),
                                                               metasequoia::CandidateEdge::LastHan);
        require(last_mixed.handled && last_mixed.commit == "言",
                "LastHan did not skip a non-Han candidate suffix.");

        type(session, "nihao");
        const auto no_han = session.select_candidate_edge(candidate_index(session, "GitHub"),
                                                           metasequoia::CandidateEdge::FirstHan);
        require(!no_han.handled && session.has_composition(),
                "A candidate without Han characters was consumed by edge selection.");
        session.handle_command(metasequoia::Command::Cancel);

        type(session, "nihao");
        session.handle_command(metasequoia::Command::Backspace);
        require(session.preedit() == "niha", "Backspace did not remove the last pinyin character.");
        const auto raw = session.handle_command(metasequoia::Command::CommitRaw);
        require(raw.handled && raw.commit == "niha", "CommitRaw did not commit the typed input.");

        type(session, "nihao");
        const auto cancel = session.handle_command(metasequoia::Command::Cancel);
        require(cancel.handled && !cancel.commit.has_value() && !session.has_composition(),
                "Cancel did not discard the composition.");

        type(session, "nihao");
        session.switch_scheme(SchemeType::Wubi);
        require(session.scheme() == SchemeType::Wubi, "Switching to Wubi did not update the active scheme.");
        require(!session.has_composition() && session.candidates().empty(),
                "Switching schemes did not discard the old composition.");

        session.switch_scheme(SchemeType::JapaneseRomaji);
        require(session.scheme() == SchemeType::JapaneseRomaji,
                "Switching without a composition did not update the active scheme.");
        session.switch_scheme(SchemeType::Quanpin);

        const std::vector<std::string> supported_helpcode_schemas{
            "lantian", "ziranma", "shouyou2_0", "shouyouplus", "xiaohe"};
        for (const std::string &schema : supported_helpcode_schemas)
        {
            require(metasequoia::InputSession::is_supported_helpcode_schema(schema) &&
                        metasequoia::InputSession::select_helpcode_schema(schema),
                    "A Windows-supported helpcode schema was rejected.");
        }
        require(!metasequoia::InputSession::is_supported_helpcode_schema("unknown") &&
                    !metasequoia::InputSession::select_helpcode_schema("unknown"),
                "An unknown helpcode schema was accepted.");

        metasequoia::InputSession quanpin_helpcode(SchemeType::Quanpin);
        quanpin_helpcode.set_quanpin_helpcode_enabled(true);
        require(metasequoia::InputSession::select_helpcode_schema("lantian"),
                "The Lantian helpcode fixture was not selected.");
        type(quanpin_helpcode, "nihaoC");
        require(!quanpin_helpcode.candidates().empty() && quanpin_helpcode.candidates().front().word == "拟好",
                "Quanpin helpcode did not reorder candidates after a complete spelling.");
        quanpin_helpcode.handle_command(metasequoia::Command::Cancel);
        type(quanpin_helpcode, "nihC");
        metasequoia::InputSession quanpin_without_helpcode(SchemeType::Quanpin);
        type(quanpin_without_helpcode, "nihC");
        require(same_candidate_words(quanpin_helpcode, quanpin_without_helpcode),
                "Quanpin helpcode changed candidates after an incomplete base spelling.");

        metasequoia::InputSession shuangpin_helpcode(SchemeType::Shuangpin);
        shuangpin_helpcode.set_shuangpin_helpcode_enabled(true);
        type(shuangpin_helpcode, "nihcc");
        require(shuangpin_helpcode.raw_segmentation() == "ni'hc'c" &&
                    shuangpin_helpcode.normalized_segmentation() == "ni'hao'c" &&
                    !shuangpin_helpcode.candidates().empty() &&
                    shuangpin_helpcode.candidates().front().word == "拟好",
                "Shuangpin helpcode or exposed segmentation did not match the complete base spelling.");

        require(!session.handle_character('1').handled, "A digit was swallowed instead of passed through.");
        require(!session.handle_command(metasequoia::Command::Backspace).handled,
                "Backspace was swallowed while no composition was active.");
        require(!session.handle_command(metasequoia::Command::CommitRaw).handled,
                "CommitRaw was swallowed while no composition was active.");
        require(!session.select_candidate(static_cast<std::size_t>(0)).handled,
                "A candidate was selected while no composition was active.");

        require(session.handle_character('\'').handled, "The apostrophe separator was not handled.");
    }

    struct FrequencyCase
    {
        metasequoia::FrequencyAdjustmentMode mode;
        const char *name;
        std::size_t expected_index;
        int linear_step;
    };
    const std::array frequency_cases{
        FrequencyCase{metasequoia::FrequencyAdjustmentMode::Disabled, "disabled", 5, 1},
        FrequencyCase{metasequoia::FrequencyAdjustmentMode::Pin, "pin", 0, 1},
        FrequencyCase{metasequoia::FrequencyAdjustmentMode::Halve, "halve", 2, 1},
        FrequencyCase{metasequoia::FrequencyAdjustmentMode::Linear, "linear", 3, 2},
        FrequencyCase{metasequoia::FrequencyAdjustmentMode::Promote, "promote", 4, 1},
    };
    for (const FrequencyCase &frequency_case : frequency_cases)
    {
        user_dictionary::close_default_user_database();
        const std::filesystem::path directory = data_directory / (std::string("frequency-") + frequency_case.name);
        prepare_frequency_fixture(directory);
        set_data_directory(directory);

        metasequoia::InputSession learning_session(SchemeType::Quanpin);
        require(learning_session.set_frequency_adjustment(
                    {frequency_case.mode, 1, frequency_case.linear_step}),
                "A supported frequency adjustment configuration was rejected.");
        type(learning_session, "ni");
        const auto learned = learning_session.select_candidate(std::string("己"));
        require(learned.handled && learned.commit == "己" && !learned.diagnostic.has_value(),
                "Frequency learning changed or diagnosed a successful candidate commit.");

        metasequoia::InputSession reopened(SchemeType::Quanpin);
        type(reopened, "ni");
        require(candidate_index(reopened, "己") == frequency_case.expected_index,
                "A frequency mode did not persist the Windows-compatible ranking transition.");
        const bool journal_exists = std::filesystem::exists(directory / "msime_user.db");
        require(journal_exists == (frequency_case.mode != metasequoia::FrequencyAdjustmentMode::Disabled),
                "Frequency learning wrote an unexpected user journal state.");
    }

    user_dictionary::close_default_user_database();
    const std::filesystem::path trigger_directory = data_directory / "frequency-trigger";
    prepare_frequency_fixture(trigger_directory);
    set_data_directory(trigger_directory);
    for (int selection = 0; selection < 2; ++selection)
    {
        metasequoia::InputSession triggered(SchemeType::Quanpin);
        require(triggered.set_frequency_adjustment({metasequoia::FrequencyAdjustmentMode::Pin, 2, 1}),
                "A valid trigger-count configuration was rejected.");
        type(triggered, "ni");
        require(triggered.select_candidate(std::string("己")).commit == "己",
                "A deferred frequency adjustment blocked candidate commit.");

        metasequoia::InputSession observed(SchemeType::Quanpin);
        type(observed, "ni");
        require(candidate_index(observed, "己") == (selection == 0 ? 5U : 0U),
                "Frequency trigger_count did not defer exactly the configured number of selections.");
    }

    user_dictionary::close_default_user_database();
    const std::filesystem::path first_candidate_directory = data_directory / "frequency-first-candidate";
    prepare_frequency_fixture(first_candidate_directory);
    set_data_directory(first_candidate_directory);
    metasequoia::InputSession first_candidate_session(SchemeType::Quanpin);
    require(first_candidate_session.set_frequency_adjustment({metasequoia::FrequencyAdjustmentMode::Pin, 1, 1}),
            "A valid first-candidate learning configuration was rejected.");
    type(first_candidate_session, "ni");
    require(first_candidate_session.select_candidate(static_cast<std::size_t>(0)).commit == "甲" &&
                !std::filesystem::exists(first_candidate_directory / "msime_user.db"),
            "Selecting the already-leading candidate created frequency state.");

    user_dictionary::close_default_user_database();
    const std::filesystem::path shuangpin_directory = data_directory / "frequency-shuangpin";
    prepare_shuangpin_frequency_fixture(shuangpin_directory);
    set_data_directory(shuangpin_directory);
    metasequoia::InputSession shuangpin_learning(SchemeType::Shuangpin);
    require(shuangpin_learning.set_frequency_adjustment({metasequoia::FrequencyAdjustmentMode::Pin, 1, 1}),
            "A valid Shuangpin frequency configuration was rejected.");
    type(shuangpin_learning, "nihc");
    require(shuangpin_learning.select_candidate(std::string("拟好")).commit == "拟好",
            "Shuangpin frequency learning blocked candidate commit.");
    metasequoia::InputSession reopened_shuangpin(SchemeType::Shuangpin);
    type(reopened_shuangpin, "nihc");
    require(!reopened_shuangpin.candidates().empty() && reopened_shuangpin.candidates().front().word == "拟好",
            "Shuangpin frequency learning did not persist through the canonical pinyin key.");

    user_dictionary::close_default_user_database();
    const std::filesystem::path wubi_directory = data_directory / "frequency-wubi";
    prepare_wubi_frequency_fixture(wubi_directory);
    set_data_directory(wubi_directory);
    metasequoia::InputSession wubi_learning(SchemeType::Wubi);
    require(wubi_learning.set_frequency_adjustment({metasequoia::FrequencyAdjustmentMode::Pin, 1, 1}),
            "A valid Wubi frequency configuration was rejected.");
    type(wubi_learning, "aaaa");
    require(wubi_learning.select_candidate(std::string("或")).commit == "或",
            "Wubi frequency learning blocked candidate commit.");
    metasequoia::InputSession reopened_wubi(SchemeType::Wubi);
    type(reopened_wubi, "aaaa");
    require(!reopened_wubi.candidates().empty() && reopened_wubi.candidates().front().word == "或",
            "Wubi frequency learning did not persist through the Wubi table.");

    user_dictionary::close_default_user_database();
    const std::filesystem::path failure_directory = data_directory / "frequency-write-failure";
    prepare_frequency_fixture(failure_directory);
    std::filesystem::create_directory(failure_directory / "msime_user.db");
    set_data_directory(failure_directory);
    metasequoia::InputSession failing_learning_session(SchemeType::Quanpin);
    require(failing_learning_session.set_frequency_adjustment({metasequoia::FrequencyAdjustmentMode::Pin, 1, 1}),
            "A valid write-failure learning configuration was rejected.");
    type(failing_learning_session, "ni");
    const auto failure_commit = failing_learning_session.select_candidate(std::string("己"));
    require(failure_commit.handled && failure_commit.commit == "己" && failure_commit.diagnostic.has_value() &&
                failure_commit.diagnostic->find("己") == std::string::npos &&
                failure_commit.diagnostic->find("ni") == std::string::npos,
            "A frequency write failure blocked commit or exposed input text in its diagnostic.");

    user_dictionary::close_default_user_database();
    const std::filesystem::path partial_write_directory = data_directory / "frequency-partial-write";
    prepare_frequency_fixture(partial_write_directory);
    set_data_directory(partial_write_directory);
    require(user_dictionary::ensure_user_database(user_dictionary::default_user_db_path()),
            "The partial-write fixture could not create the user dictionary schema.");
    user_dictionary::close_default_user_database();
    {
        Database user_database(partial_write_directory / "msime_user.db");
        user_database.execute(
            "CREATE TRIGGER reject_frequency_journal BEFORE INSERT ON user_dictionary_operations "
            "BEGIN SELECT RAISE(FAIL, 'injected journal failure'); END");
    }
    metasequoia::InputSession partial_write_session(SchemeType::Quanpin);
    require(partial_write_session.set_frequency_adjustment({metasequoia::FrequencyAdjustmentMode::Pin, 1, 1}),
            "A valid partial-write learning configuration was rejected.");
    type(partial_write_session, "ni");
    const auto partial_write_commit = partial_write_session.select_candidate(std::string("己"));
    require(partial_write_commit.handled && partial_write_commit.commit == "己" &&
                partial_write_commit.diagnostic.has_value() &&
                partial_write_commit.diagnostic->find("己") == std::string::npos &&
                partial_write_commit.diagnostic->find("ni") == std::string::npos,
            "A partial frequency write blocked commit or exposed input text in its diagnostic.");
    {
        Database main_database(partial_write_directory / "msime.db");
        require(main_database.query_integer("SELECT weight FROM tbl_1_n WHERE key='ni' AND value='己'") == 50,
                "A failed journal write left a partial frequency update in the main dictionary.");
    }

    metasequoia::FrequencyAdjustmentOptions invalid_frequency;
    invalid_frequency.mode = static_cast<metasequoia::FrequencyAdjustmentMode>(99);
    require(!failing_learning_session.set_frequency_adjustment(invalid_frequency),
            "An unknown frequency mode was accepted.");
    invalid_frequency = {};
    invalid_frequency.trigger_count = 0;
    require(!failing_learning_session.set_frequency_adjustment(invalid_frequency),
            "An out-of-range frequency trigger count was accepted.");
    invalid_frequency = {};
    invalid_frequency.linear_step = 11;
    require(!failing_learning_session.set_frequency_adjustment(invalid_frequency),
            "An out-of-range frequency linear step was accepted.");
    user_dictionary::close_default_user_database();

    metasequoia::InputSession unicode_session(SchemeType::Quanpin);
    require(unicode_session.handle_character('U', true).handled &&
                unicode_session.local_input_mode() == metasequoia::LocalInputMode::Unicode &&
                unicode_session.preedit() == "U" && unicode_session.candidates().empty(),
            "Shift+U did not enter an empty Unicode composition.");
    require(unicode_session.handle_character('g').handled && unicode_session.preedit() == "U" &&
                unicode_session.candidates().empty(),
            "Unicode mode accepted or forwarded a non-hexadecimal character.");
    for (const char character : std::string("4e00"))
    {
        require(unicode_session.handle_character(character).handled,
                "Unicode mode rejected a hexadecimal character.");
    }
    require(unicode_session.preedit() == "U4e00" && unicode_session.candidates().size() == 1 &&
                unicode_session.candidates().front().word == "一" &&
                unicode_session.candidates().front().pinyin == "U+4E00" &&
                unicode_session.candidates().front().source == CandidateSource::Generated,
            "Unicode mode did not produce the Windows-compatible BMP candidate.");
    const auto unicode_commit = unicode_session.select_candidate(0);
    require(unicode_commit.handled && unicode_commit.commit == "一" && !unicode_session.has_composition() &&
                unicode_session.local_input_mode() == metasequoia::LocalInputMode::None,
            "Committing a Unicode candidate did not leave the local mode.");

    require(unicode_session.handle_character('U', true).handled && unicode_session.handle_character('+').handled,
            "Unicode mode rejected its optional plus prefix.");
    for (const char character : std::string("1f600"))
    {
        require(unicode_session.handle_character(character).handled,
                "Unicode mode rejected a supplementary-plane hexadecimal character.");
    }
    require(unicode_session.preedit() == "U+1f600" && unicode_session.candidates().size() == 1 &&
                unicode_session.candidates().front().word == "😀",
            "Unicode mode did not produce a supplementary-plane scalar.");
    require(unicode_session.handle_command(metasequoia::Command::Cancel).handled && !unicode_session.has_composition(),
            "Cancel did not leave Unicode mode.");

    const auto require_invalid_unicode = [&](const std::string &hex) {
        require(unicode_session.handle_character('U', true).handled,
                "Unicode mode could not be re-entered for invalid-scalar coverage.");
        for (const char character : hex)
        {
            require(unicode_session.handle_character(character).handled,
                    "Unicode mode rejected an invalid scalar's hexadecimal spelling.");
        }
        require(unicode_session.candidates().empty(), "Unicode mode produced an invalid scalar candidate.");
        require(unicode_session.handle_command(metasequoia::Command::Cancel).handled,
                "Unicode invalid-scalar fixture could not be cancelled.");
    };
    require_invalid_unicode("d800");
    require_invalid_unicode("110000");
    require_invalid_unicode("0000001");

    require(unicode_session.handle_character('U', true).handled,
            "Unicode prefix was not handled before Backspace coverage.");
    require(unicode_session.handle_command(metasequoia::Command::Backspace).handled &&
                !unicode_session.has_composition() &&
                unicode_session.local_input_mode() == metasequoia::LocalInputMode::None,
            "Backspace on a bare Unicode prefix did not leave the mode.");

    metasequoia::InputSession plain_uppercase(SchemeType::Quanpin);
    require(plain_uppercase.handle_character('U').handled &&
                plain_uppercase.local_input_mode() == metasequoia::LocalInputMode::None,
            "An uppercase character without Shift-only entered Unicode mode.");
    metasequoia::LocalModeOptions disabled_local_modes;
    disabled_local_modes.unicode = false;
    metasequoia::InputSession disabled_unicode(SchemeType::Quanpin);
    disabled_unicode.set_local_mode_options(disabled_local_modes);
    require(disabled_unicode.handle_character('U', true).handled &&
                disabled_unicode.local_input_mode() == metasequoia::LocalInputMode::None,
            "A disabled Unicode mode still intercepted Shift+U.");

    metasequoia::InputSession wubi_unicode(SchemeType::Wubi);
    require(wubi_unicode.handle_character('U', true).handled &&
                wubi_unicode.local_input_mode() == metasequoia::LocalInputMode::None,
            "Unicode mode started outside a pinyin scheme.");
    metasequoia::InputSession switch_clears_unicode(SchemeType::Shuangpin);
    require(switch_clears_unicode.handle_character('U', true).handled &&
                switch_clears_unicode.local_input_mode() == metasequoia::LocalInputMode::Unicode,
            "Shuangpin could not enter Unicode mode.");
    switch_clears_unicode.switch_scheme(SchemeType::Quanpin);
    require(!switch_clears_unicode.has_composition() &&
                switch_clears_unicode.local_input_mode() == metasequoia::LocalInputMode::None,
            "Switching schemes did not clear Unicode mode.");

    std::filesystem::remove_all(data_directory);
    return 0;
}
