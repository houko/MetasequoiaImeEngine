#include "../../core/input_session.h"
#include "../../core/data_path.h"

#include <sqlite3.h>

#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <filesystem>
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
} // namespace

int main()
{
    const auto unique_suffix = std::to_string(std::chrono::high_resolution_clock::now().time_since_epoch().count());
    const std::filesystem::path data_directory =
        std::filesystem::temp_directory_path() / std::filesystem::u8path("metasequoia-session-词库-" + unique_suffix);
    std::filesystem::create_directories(data_directory);
#ifdef _WIN32
    if (_wputenv_s(L"METASEQUOIA_IME_DATA_DIR", data_directory.c_str()) != 0)
#else
    if (setenv("METASEQUOIA_IME_DATA_DIR", metasequoia::path_to_utf8(data_directory).c_str(), 1) != 0)
#endif
    {
        throw std::runtime_error("Failed to set the data directory override.");
    }

    {
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

        require(!session.handle_character('1').handled, "A digit was swallowed instead of passed through.");
        require(!session.handle_command(metasequoia::Command::Backspace).handled,
                "Backspace was swallowed while no composition was active.");
        require(!session.handle_command(metasequoia::Command::CommitRaw).handled,
                "CommitRaw was swallowed while no composition was active.");
        require(!session.select_candidate(static_cast<std::size_t>(0)).handled,
                "A candidate was selected while no composition was active.");

        require(session.handle_character('\'').handled, "The apostrophe separator was not handled.");
    }

    std::filesystem::remove_all(data_directory);
    return 0;
}
