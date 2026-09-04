#include "../../core/input_session.h"
#include "../../core/data_path.h"

#include <sqlite3.h>

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
        session.handle_command(metasequoia::Command::Backspace);
        require(session.preedit() == "niha", "Backspace did not remove the last pinyin character.");
        const auto raw = session.handle_command(metasequoia::Command::CommitRaw);
        require(raw.handled && raw.commit == "niha", "CommitRaw did not commit the typed input.");

        type(session, "nihao");
        const auto cancel = session.handle_command(metasequoia::Command::Cancel);
        require(cancel.handled && !cancel.commit.has_value() && !session.has_composition(),
                "Cancel did not discard the composition.");

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
